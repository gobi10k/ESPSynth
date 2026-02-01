#include "Arpeggiator.h"
#include <stdlib.h>
#include <string.h>

const char* ARP_MODE_NAMES[] = {"OFF", "UP", "DOWN", "U/D", "D/U", "RND", "ORD"};

Arpeggiator::Arpeggiator() :
    mode_(ArpMode::OFF),
    tempo_(120.0f),
    division_(4),
    gateLength_(0.5f),
    octaveRange_(1),
    numHeld_(0),
    stepIndex_(0),
    direction_(1),
    currentOctave_(0),
    currentNote_(0),
    currentVelocity_(0),
    gateOn_(false),
    sampleCounter_(0),
    samplesPerStep_(0),
    gateOffSample_(0),
    externalClock_(false),
    clockCounter_(0)
{
    memset(heldNotes_, 0, sizeof(heldNotes_));
    memset(heldVelocities_, 0, sizeof(heldVelocities_));
    memset(sortedNotes_, 0, sizeof(sortedNotes_));
    memset(sortedVelocities_, 0, sizeof(sortedVelocities_));
    setTempo(120.0f);
}

void Arpeggiator::setMode(ArpMode mode) {
    mode_ = mode;
    stepIndex_ = 0;
    direction_ = 1;
    currentOctave_ = 0;
}

void Arpeggiator::setTempo(float bpm) {
    tempo_ = constrain(bpm, 30.0f, 300.0f);
    // Calculate samples per step based on tempo and division
    float beatsPerSecond = tempo_ / 60.0f;
    float stepsPerSecond = beatsPerSecond * division_ / 4.0f;
    samplesPerStep_ = (uint32_t)(SAMPLE_RATE / stepsPerSecond);
    gateOffSample_ = (uint32_t)(samplesPerStep_ * gateLength_);
}

void Arpeggiator::setDivision(uint8_t div) {
    division_ = div;
    setTempo(tempo_);  // Recalculate
}

void Arpeggiator::setGateLength(float length) {
    gateLength_ = constrain(length, 0.1f, 1.0f);
    gateOffSample_ = (uint32_t)(samplesPerStep_ * gateLength_);
}

void Arpeggiator::setOctaveRange(uint8_t octaves) {
    octaveRange_ = constrain(octaves, 1, 4);
}

void Arpeggiator::noteOn(uint8_t note, uint8_t velocity) {
    if (numHeld_ >= ARP_MAX_NOTES) return;
    
    // Check if already held
    for (int i = 0; i < numHeld_; i++) {
        if (heldNotes_[i] == note) return;
    }
    
    heldNotes_[numHeld_] = note;
    heldVelocities_[numHeld_] = velocity;
    numHeld_++;
    
    sortNotes();
    
    // Start from beginning when first note pressed
    if (numHeld_ == 1) {
        stepIndex_ = 0;
        currentOctave_ = 0;
        direction_ = 1;
        sampleCounter_ = 0;
    }
}

void Arpeggiator::noteOff(uint8_t note) {
    // Find and remove note
    for (int i = 0; i < numHeld_; i++) {
        if (heldNotes_[i] == note) {
            // Shift remaining notes down
            for (int j = i; j < numHeld_ - 1; j++) {
                heldNotes_[j] = heldNotes_[j + 1];
                heldVelocities_[j] = heldVelocities_[j + 1];
            }
            numHeld_--;
            sortNotes();
            return;
        }
    }
}

void Arpeggiator::allNotesOff() {
    numHeld_ = 0;
    gateOn_ = false;
}

void Arpeggiator::sortNotes() {
    // Copy to sorted array
    memcpy(sortedNotes_, heldNotes_, numHeld_);
    memcpy(sortedVelocities_, heldVelocities_, numHeld_);
    
    // Simple bubble sort (small array)
    for (int i = 0; i < numHeld_ - 1; i++) {
        for (int j = 0; j < numHeld_ - i - 1; j++) {
            if (sortedNotes_[j] > sortedNotes_[j + 1]) {
                uint8_t tmpN = sortedNotes_[j];
                uint8_t tmpV = sortedVelocities_[j];
                sortedNotes_[j] = sortedNotes_[j + 1];
                sortedVelocities_[j] = sortedVelocities_[j + 1];
                sortedNotes_[j + 1] = tmpN;
                sortedVelocities_[j + 1] = tmpV;
            }
        }
    }
}

uint8_t Arpeggiator::getNextNote() {
    if (numHeld_ == 0) return 60;  // Return middle C as default
    
    int idx = 0;
    
    switch (mode_) {
        case ArpMode::UP:
            idx = stepIndex_ % numHeld_;
            break;
            
        case ArpMode::DOWN:
            idx = (numHeld_ - 1) - (stepIndex_ % numHeld_);
            break;
            
        case ArpMode::UP_DOWN:
        case ArpMode::DOWN_UP: {
            int range = numHeld_ * 2 - 2;
            if (range <= 0) range = 1;
            int pos = stepIndex_ % range;
            if (pos < numHeld_) {
                idx = (mode_ == ArpMode::UP_DOWN) ? pos : (numHeld_ - 1 - pos);
            } else {
                idx = (mode_ == ArpMode::UP_DOWN) ? 
                      (range - pos) : (pos - numHeld_ + 1);
            }
            break;
        }
            
        case ArpMode::RANDOM:
            idx = rand() % numHeld_;
            break;
            
        case ArpMode::ORDER:
            idx = stepIndex_ % numHeld_;
            // Bounds check
            if (idx < 0) idx = 0;
            if (idx >= numHeld_) idx = numHeld_ - 1;
            return heldNotes_[idx] + (currentOctave_ * 12);
            
        default:
            idx = 0;
    }
    
    // Bounds check
    if (idx < 0) idx = 0;
    if (idx >= numHeld_) idx = numHeld_ - 1;
    if (idx >= ARP_MAX_NOTES) idx = ARP_MAX_NOTES - 1;
    
    return sortedNotes_[idx] + (currentOctave_ * 12);
}

void Arpeggiator::advanceStep() {
    stepIndex_++;
    
    // Handle octave cycling
    int notesPerOctave = (mode_ == ArpMode::UP_DOWN || mode_ == ArpMode::DOWN_UP) ?
                         (numHeld_ * 2 - 2) : numHeld_;
    if (notesPerOctave <= 0) notesPerOctave = 1;
    
    if (stepIndex_ >= notesPerOctave) {
        stepIndex_ = 0;
        currentOctave_++;
        if (currentOctave_ >= octaveRange_) {
            currentOctave_ = 0;
        }
    }
}

bool Arpeggiator::process() {
    if (mode_ == ArpMode::OFF || numHeld_ == 0) {
        gateOn_ = false;
        return false;
    }
    
    bool triggered = false;
    
    sampleCounter_++;
    
    // Check for gate off
    if (gateOn_ && sampleCounter_ >= gateOffSample_) {
        gateOn_ = false;
    }
    
    // Check for next step
    if (sampleCounter_ >= samplesPerStep_) {
        sampleCounter_ = 0;
        
        currentNote_ = getNextNote();
        
        // Get velocity safely
        int velIdx = stepIndex_ % numHeld_;
        if (velIdx < 0) velIdx = 0;
        if (velIdx >= ARP_MAX_NOTES) velIdx = ARP_MAX_NOTES - 1;
        
        currentVelocity_ = (mode_ == ArpMode::ORDER) ?
                           heldVelocities_[velIdx] :
                           sortedVelocities_[velIdx];
        
        gateOn_ = true;
        triggered = true;
        
        advanceStep();
    }
    
    return triggered;
}

void Arpeggiator::clockTick() {
    if (!externalClock_) return;
    // 24 PPQN MIDI clock
    clockCounter_++;
    if (clockCounter_ >= (24 / division_)) {
        clockCounter_ = 0;
        // Force next step
        sampleCounter_ = samplesPerStep_;
    }
}

void Arpeggiator::clockReset() {
    clockCounter_ = 0;
    stepIndex_ = 0;
    currentOctave_ = 0;
    sampleCounter_ = 0;
}

void Arpeggiator::setExternalClock(bool external) {
    externalClock_ = external;
}
