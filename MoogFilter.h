#ifndef MOOG_FILTER_H
#define MOOG_FILTER_H

#include "Config.h"
#include "MathUtils.h"

/**
 * Moog Ladder Filter Approximation
 * 
 * A 4-pole (24dB/octave) lowpass filter with resonance,
 * approximating the classic Moog ladder topology.
 * 
 * This implementation uses the simplified Stilson/Smith model
 * with nonlinear saturation for analog character.
 * 
 * Features:
 *   - Self-oscillation at high resonance
 *   - Warm, musical character
 *   - Nonlinear saturation in feedback path
 *   - Stable at all settings
 */

class MoogFilter {
public:
    MoogFilter();
    
    void setCutoff(float hz);
    void setResonance(float res);  // 0-1, self-oscillates near 1
    void setDrive(float drive);    // Pre-filter saturation (1-4)
    
    float getCutoff() const { return cutoffHz_; }
    float getResonance() const { return resonance_; }
    
    // Modulation input (added to cutoff)
    void setCutoffMod(float mod) { cutoffMod_ = mod; }
    void updateCoefficients(float modHz);
    
    float process(float input);
    void reset();

private:
    float cutoffHz_;
    float resonance_;
    float drive_;
    float cutoffMod_;
    
    // Filter coefficient
    float gMod_;  // Current modulated coefficient
    float invGMod_;
    
    // 4 stages of state
    float stage_[4];
    
    // Delay elements for feedback
    float delay_[4];
    
};

/**
 * Ladder Filter with multiple modes
 * 
 * Extends the basic Moog filter with different tap configurations
 * to provide LP, HP, BP, and other responses.
 */
enum class LadderMode : uint8_t {
    LP24 = 0,    // 4-pole lowpass (classic Moog)
    LP18,        // 3-pole lowpass (softer)
    LP12,        // 2-pole lowpass (TB-303 style)
    LP6,         // 1-pole lowpass (subtle)
    BP12,        // Bandpass (2-pole)
    HP24,        // 4-pole highpass
    NUM_MODES
};

extern const char* LADDER_MODE_NAMES[];

class LadderFilter {
public:
    LadderFilter();
    
    void setCutoff(float hz);
    void setResonance(float res);
    void setMode(LadderMode mode);
    void setDrive(float drive);
    void setKeyTracking(float amount);  // 0-1, filter follows pitch
    
    float getCutoff() const { return cutoffHz_; }
    float getResonance() const { return resonance_; }
    LadderMode getMode() const { return mode_; }
    
    void setCutoffMod(float mod) { cutoffMod_ = mod; }
    void setKeyFreq(float hz) { keyFreq_ = hz; }
    void updateCoefficients(float modHz);
    
    float process(float input);
    void reset();

private:
    float cutoffHz_;
    float resonance_;
    LadderMode mode_;
    float drive_;
    float keyTracking_;
    float keyFreq_;
    float cutoffMod_;
    
    // Filter coefficient
    float gMod_;
    float invGMod_;
    
    // 4 stages
    float stage_[4];
    float delay_[4];
    
    // Tap coefficients for different modes
    float taps_[5];
    
    void updateTaps();
};

#endif
