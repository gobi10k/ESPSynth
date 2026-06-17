#ifndef REVERB_H
#define REVERB_H

#include "Config.h"

/**
 * Optimized FDN Reverb with improved quality
 * 
 * Key optimizations:
 * - Pre-calculated coefficients to avoid runtime calculations
 * - Optimized damping filters
 * - Improved diffusion network
 * - More efficient delay line access
 */

// Optimized delay lengths (mutually prime for reduced coloration)
constexpr uint16_t FDN_DELAYS[4] = {541, 643, 727, 839};
constexpr uint16_t FDN_MAX_DELAY = 1024;  // Changed to power of 2 for bitmask optimization
constexpr uint16_t PREDELAY_MAX = 2048;   // Power of 2 for bitmask
constexpr uint16_t DIFF_BUFFER_SIZE = 256;

class FDNReverb {
public:
    FDNReverb();
    
    void setDecay(float seconds);
    void setSize(float size);
    void setDamping(float damp);
    void setMix(float mix);
    void setPreDelay(float ms);
    
    float getDecay() const { return decayTime_; }
    float getSize() const { return roomSize_; }
    float getDamping() const { return damping_; }
    float getMix() const { return mix_; }
    float getPreDelay() const { return preDelayMs_; }
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool en) { enabled_ = en; }
    void setFreeze(bool frozen) { frozen_ = frozen; }
    bool isFrozen() const { return frozen_; }
    
    float process(float input);
    void processStereo(float inL, float inR, float& outL, float& outR, bool liteMode = false);
    void reset();

private:
    void updateDecayCoefficients();
    void updateInternalCoefficients();
    
    // Optimized buffer access with bitmask (requires power of 2 sizes)
    static constexpr uint32_t DELAY_MASK = FDN_MAX_DELAY - 1;
    static constexpr uint32_t PREDELAY_MASK = PREDELAY_MAX - 1;
    static constexpr uint32_t DIFF_MASK = DIFF_BUFFER_SIZE - 1;
    
    // int16 buffers with optimized layout
    int16_t delayLines_[4][FDN_MAX_DELAY];
    uint16_t writePos_[4];
    uint16_t delayTimes_[4];
    
    // Pre-calculated coefficients for performance
    float feedbackGain_;
    float dampingCoef_;
    float dampState_[4];
    float inputGain_;  // Pre-calculated input gain
    
    // Pre-delay buffer
    int16_t preDelayBuffer_[PREDELAY_MAX];
    uint16_t preDelayPos_;
    uint16_t preDelaySamples_;
    float preDelayMs_;  // Store original ms value
    
    // Parameters
    float decayTime_;
    float roomSize_;
    float damping_;
    float mix_;
    
    // Diffusion buffers with improved coefficients
    int16_t diffBuf1_[DIFF_BUFFER_SIZE];
    int16_t diffBuf2_[DIFF_BUFFER_SIZE];
    uint16_t diffPos1_, diffPos2_;
    float diffCoeff1_, diffCoeff2_;

    bool enabled_;
    bool frozen_;
};

#endif