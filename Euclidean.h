#ifndef EUCLIDEAN_H
#define EUCLIDEAN_H

#include "Config.h"

/**
 * Euclidean Rhythm Generator
 * 
 * Generates rhythms by distributing K pulses as evenly as possible
 * over N steps. The algorithm produces musically useful patterns found
 * in African, Cuban, Brazilian, and electronic music.
 * 
 * Examples:
 *   E(3,8) = [x . . x . . x .] - Cuban Tresillo
 *   E(5,8) = [x . x x . x x .] - Cuban Cinquillo
 *   E(4,12) = [x . . x . . x . . x . .] - Fandango
 *   E(5,12) = [x . x . x . . x . x . .] - Bossa Nova
 */

constexpr uint8_t MAX_EUCLIDEAN_STEPS = 32;

class EuclideanRhythm {
public:
    EuclideanRhythm();
    
    // Set pattern parameters
    void setSteps(uint8_t steps);      // Total steps (1-32)
    void setPulses(uint8_t pulses);    // Number of hits (0-steps)
    void setRotation(uint8_t rot);     // Pattern rotation (0 to steps-1)
    
    uint8_t getSteps() const { return steps_; }
    uint8_t getPulses() const { return pulses_; }
    uint8_t getRotation() const { return rotation_; }
    
    // Get current step position
    uint8_t getCurrentStep() const { return currentStep_; }
    
    // Advance and return true if current step is a pulse
    bool advance();
    
    // Check if a specific step is a pulse
    bool isPulse(uint8_t step) const;
    
    // Get the pattern as a bitmask
    uint32_t getPatternMask() const { return patternMask_; }
    
    // Reset to step 0
    void reset();
    
    // Generate pattern visualization string (for debugging)
    void getPatternString(char* buf, size_t bufLen) const;

private:
    void regeneratePattern();
    
    uint8_t steps_;
    uint8_t pulses_;
    uint8_t rotation_;
    uint8_t currentStep_;
    
    uint32_t patternMask_;  // Bit i = 1 if step i is a pulse
};

/**
 * Euclidean Sequencer
 * 
 * A simple sequencer that uses Euclidean rhythms to trigger events.
 * Includes probability, swing, and accent controls.
 */
class EuclideanSequencer {
public:
    EuclideanSequencer();
    
    // Pattern
    void setSteps(uint8_t steps);
    void setPulses(uint8_t pulses);
    void setRotation(uint8_t rot);
    
    // Timing
    void setTempo(float bpm);
    void setDivision(uint8_t div);   // 1,2,4,8,16
    void setSwing(float swing);       // 0-1 (0=none, 0.5=triplet feel, 1=extreme)
    
    // Probability
    void setProbability(float prob);  // 0-1, chance each pulse actually triggers
    
    // Accents (every N pulses is accented)
    void setAccentPeriod(uint8_t period);  // 0=none, 2=every other, 4=every 4th
    void setAccentVelocity(uint8_t vel);   // Velocity for accented notes
    void setNormalVelocity(uint8_t vel);   // Velocity for normal notes
    
    // Process - call every sample
    // Returns true when a step triggers
    bool process();
    
    // Get trigger info after process() returns true
    uint8_t getVelocity() const { return lastVelocity_; }
    bool isAccented() const { return lastAccent_; }
    
    // Sync
    void clockTick();     // External clock pulse
    void clockReset();    // Reset to step 0
    void setExternalClock(bool ext);
    
    void reset();

private:
    EuclideanRhythm rhythm_;
    
    float tempo_;
    uint8_t division_;
    float swing_;
    float probability_;
    
    uint8_t accentPeriod_;
    uint8_t accentVelocity_;
    uint8_t normalVelocity_;
    
    uint32_t sampleCounter_;
    uint32_t samplesPerStep_;
    uint32_t swingOffset_;
    bool swingState_;
    
    uint8_t lastVelocity_;
    bool lastAccent_;
    uint8_t pulseCount_;
    
    bool externalClock_;
    uint8_t clockDivCounter_;
};

#endif
