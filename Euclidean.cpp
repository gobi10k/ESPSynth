#include "Euclidean.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// EUCLIDEAN RHYTHM
// ============================================================================

EuclideanRhythm::EuclideanRhythm() :
    steps_(8),
    pulses_(3),
    rotation_(0),
    currentStep_(0),
    patternMask_(0)
{
    regeneratePattern();
}

void EuclideanRhythm::setSteps(uint8_t steps) {
    steps_ = constrain(steps, 1, MAX_EUCLIDEAN_STEPS);
    if (pulses_ > steps_) pulses_ = steps_;
    if (rotation_ >= steps_) rotation_ = 0;
    regeneratePattern();
}

void EuclideanRhythm::setPulses(uint8_t pulses) {
    pulses_ = constrain(pulses, 0, steps_);
    regeneratePattern();
}

void EuclideanRhythm::setRotation(uint8_t rot) {
    rotation_ = rot % steps_;
    regeneratePattern();
}

void EuclideanRhythm::regeneratePattern() {
    patternMask_ = 0;
    
    if (pulses_ == 0 || steps_ == 0) return;
    if (pulses_ >= steps_) {
        // All pulses
        patternMask_ = (1UL << steps_) - 1;
        return;
    }
    
    // Bjorklund's algorithm
    // This distributes pulses as evenly as possible over steps
    
    uint8_t pattern[MAX_EUCLIDEAN_STEPS];
    memset(pattern, 0, sizeof(pattern));
    
    // Initialize: first 'pulses' positions have 1s
    for (int i = 0; i < pulses_; i++) {
        pattern[i] = 1;
    }
    
    // Iteratively move to distribute evenly
    int remainder = steps_ - pulses_;
    int divisor = pulses_;
    int level = 0;
    
    // Counts for building pattern
    int counts[MAX_EUCLIDEAN_STEPS];
    int remainders[MAX_EUCLIDEAN_STEPS];
    
    remainders[0] = remainder;
    
    while (remainders[level] > 1) {
        counts[level] = divisor / remainders[level];
        remainders[level + 1] = divisor % remainders[level];
        divisor = remainders[level];
        level++;
    }
    counts[level] = divisor;
    
    // Build pattern using Bresenham-like approach (produces same results as Bjorklund)
    memset(pattern, 0, sizeof(pattern));
    
    if (pulses_ > 0) {
        int error = steps_ / 2;
        for (int i = 0; i < steps_; i++) {
            error -= pulses_;
            if (error < 0) {
                pattern[i] = 1;
                error += steps_;
            }
        }
    }
    
    // Apply rotation and build mask
    for (int i = 0; i < steps_; i++) {
        int rotatedIdx = (i + rotation_) % steps_;
        if (pattern[rotatedIdx]) {
            patternMask_ |= (1UL << i);
        }
    }
}

bool EuclideanRhythm::advance() {
    bool isPulse = (patternMask_ & (1UL << currentStep_)) != 0;
    currentStep_ = (currentStep_ + 1) % steps_;
    return isPulse;
}

bool EuclideanRhythm::isPulse(uint8_t step) const {
    if (step >= steps_) return false;
    return (patternMask_ & (1UL << step)) != 0;
}

void EuclideanRhythm::reset() {
    currentStep_ = 0;
}

void EuclideanRhythm::getPatternString(char* buf, size_t bufLen) const {
    size_t pos = 0;
    for (int i = 0; i < steps_ && pos < bufLen - 1; i++) {
        buf[pos++] = isPulse(i) ? 'x' : '.';
    }
    buf[pos] = '\0';
}

// ============================================================================
// EUCLIDEAN SEQUENCER
// ============================================================================

EuclideanSequencer::EuclideanSequencer() :
    tempo_(120.0f),
    division_(4),
    swing_(0.0f),
    probability_(1.0f),
    accentPeriod_(0),
    accentVelocity_(127),
    normalVelocity_(100),
    sampleCounter_(0),
    samplesPerStep_(0),
    swingOffset_(0),
    swingState_(false),
    lastVelocity_(100),
    lastAccent_(false),
    pulseCount_(0),
    externalClock_(false),
    clockDivCounter_(0)
{
    setTempo(tempo_);
}

void EuclideanSequencer::setSteps(uint8_t steps) {
    rhythm_.setSteps(steps);
}

void EuclideanSequencer::setPulses(uint8_t pulses) {
    rhythm_.setPulses(pulses);
}

void EuclideanSequencer::setRotation(uint8_t rot) {
    rhythm_.setRotation(rot);
}

void EuclideanSequencer::setTempo(float bpm) {
    tempo_ = constrain(bpm, 30.0f, 300.0f);
    float beatsPerSecond = tempo_ / 60.0f;
    float stepsPerSecond = beatsPerSecond * division_ / 4.0f;
    samplesPerStep_ = (uint32_t)(SAMPLE_RATE / stepsPerSecond);
    swingOffset_ = (uint32_t)(samplesPerStep_ * swing_ * 0.5f);
}

void EuclideanSequencer::setDivision(uint8_t div) {
    division_ = div;
    setTempo(tempo_);
}

void EuclideanSequencer::setSwing(float swing) {
    swing_ = constrain(swing, 0.0f, 0.9f);
    swingOffset_ = (uint32_t)(samplesPerStep_ * swing_ * 0.5f);
}

void EuclideanSequencer::setProbability(float prob) {
    probability_ = constrain(prob, 0.0f, 1.0f);
}

void EuclideanSequencer::setAccentPeriod(uint8_t period) {
    accentPeriod_ = period;
}

void EuclideanSequencer::setAccentVelocity(uint8_t vel) {
    accentVelocity_ = vel;
}

void EuclideanSequencer::setNormalVelocity(uint8_t vel) {
    normalVelocity_ = vel;
}

bool EuclideanSequencer::process() {
    if (externalClock_) return false;  // Wait for clock tick
    
    sampleCounter_++;
    
    // Apply swing: alternate steps are delayed
    uint32_t targetSamples = samplesPerStep_;
    if (swingState_) {
        targetSamples += swingOffset_;
    }
    
    if (sampleCounter_ >= targetSamples) {
        sampleCounter_ -= targetSamples;
        swingState_ = !swingState_;
        
        // Check if this step is a pulse
        if (rhythm_.advance()) {
            // Apply probability
            float r = (float)rand() / RAND_MAX;
            if (r < probability_) {
                pulseCount_++;
                
                // Check for accent
                lastAccent_ = (accentPeriod_ > 0) && 
                              ((pulseCount_ % accentPeriod_) == 0);
                lastVelocity_ = lastAccent_ ? accentVelocity_ : normalVelocity_;
                
                return true;
            }
        }
    }
    
    return false;
}

void EuclideanSequencer::clockTick() {
    if (!externalClock_) return;
    
    // 24 PPQN MIDI clock
    clockDivCounter_++;
    if (clockDivCounter_ >= (24 / division_)) {
        clockDivCounter_ = 0;
        
        swingState_ = !swingState_;
        
        // Skip processing if swing delay
        if (swingState_ && swing_ > 0.0f) {
            // Delay this step (simplified - would need timer for proper swing)
        }
        
        if (rhythm_.advance()) {
            float r = (float)rand() / RAND_MAX;
            if (r < probability_) {
                pulseCount_++;
                lastAccent_ = (accentPeriod_ > 0) && 
                              ((pulseCount_ % accentPeriod_) == 0);
                lastVelocity_ = lastAccent_ ? accentVelocity_ : normalVelocity_;
            }
        }
    }
}

void EuclideanSequencer::clockReset() {
    clockDivCounter_ = 0;
    rhythm_.reset();
    pulseCount_ = 0;
    swingState_ = false;
}

void EuclideanSequencer::setExternalClock(bool ext) {
    externalClock_ = ext;
}

void EuclideanSequencer::reset() {
    sampleCounter_ = 0;
    rhythm_.reset();
    pulseCount_ = 0;
    swingState_ = false;
    clockDivCounter_ = 0;
}
