#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include "Config.h"

enum class ArpMode : uint8_t {
    OFF = 0,
    UP,
    DOWN,
    UP_DOWN,
    DOWN_UP,
    RANDOM,
    ORDER,      // Order played
    NUM_MODES
};

extern const char* ARP_MODE_NAMES[];

constexpr uint8_t ARP_MAX_NOTES = 8;

class Arpeggiator {
public:
    Arpeggiator();
    
    void setMode(ArpMode mode);
    void setTempo(float bpm);
    void setDivision(uint8_t div);  // 1,2,4,8,16,32
    void setGateLength(float length);  // 0.1 to 1.0
    void setOctaveRange(uint8_t octaves);  // 1-4
    
    ArpMode getMode() const { return mode_; }
    float getTempo() const { return tempo_; }
    uint8_t getDivision() const { return division_; }
    
    // Note input
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void allNotesOff();
    
    // Process - returns true when new note should trigger
    // Sets currentNote and currentVelocity
    bool process();
    
    uint8_t getCurrentNote() const { return currentNote_; }
    uint8_t getCurrentVelocity() const { return currentVelocity_; }
    bool isGateOn() const { return gateOn_; }
    
    // External clock sync
    void clockTick();
    void clockReset();
    void setExternalClock(bool external);

private:
    void sortNotes();
    void advanceStep();
    uint8_t getNextNote();
    
    ArpMode mode_;
    float tempo_;
    uint8_t division_;
    float gateLength_;
    uint8_t octaveRange_;
    
    // Held notes
    uint8_t heldNotes_[ARP_MAX_NOTES];
    uint8_t heldVelocities_[ARP_MAX_NOTES];
    uint8_t numHeld_;
    
    // Sorted notes for playback
    uint8_t sortedNotes_[ARP_MAX_NOTES];
    uint8_t sortedVelocities_[ARP_MAX_NOTES];
    
    // Playback state
    int8_t stepIndex_;
    int8_t direction_;  // 1 or -1
    uint8_t currentOctave_;
    uint8_t currentNote_;
    uint8_t currentVelocity_;
    bool gateOn_;
    
    // Timing
    uint32_t sampleCounter_;
    uint32_t samplesPerStep_;
    uint32_t gateOffSample_;
    
    bool externalClock_;
    uint32_t clockCounter_;
};

#endif
