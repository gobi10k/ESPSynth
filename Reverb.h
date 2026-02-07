#ifndef REVERB_H
#define REVERB_H

#include "Config.h"

/**
 * Memory-Optimized FDN Reverb
 * 
 * Uses int16_t buffers instead of float to save ~50% RAM.
 * Smaller delay lines optimized for ESP32 constraints.
 * 
 * Memory usage: ~10KB total (vs ~48KB with floats)
 */

// Reduced delay sizes - still prime for reduced coloration
constexpr uint16_t FDN_DELAYS[4] = {541, 643, 727, 839};
constexpr uint16_t FDN_MAX_DELAY = 850;

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
    float getPreDelay() const { return (float)preDelayTime_ * 1000.0f / SAMPLE_RATE; }
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool en) { enabled_ = en; }
    void setFreeze(bool frozen) { frozen_ = frozen; }
    bool isFrozen() const { return frozen_; }
    
    float process(float input);
    void processStereo(float inL, float inR, float& outL, float& outR);
    void reset();

private:
    void updateDecayCoefficients();
    
    // int16 buffers: 4 × 850 × 2 = 6,800 bytes
    int16_t delayLines_[4][FDN_MAX_DELAY];
    uint16_t writePos_[4];
    uint16_t delayTimes_[4];
    
    float feedbackGain_;
    float dampingCoef_;
    float dampState_[4];
    
    // Pre-delay: 2400 × 2 = 4,800 bytes (50ms)
    static constexpr uint16_t PREDELAY_MAX = 2400;
    int16_t preDelayBuffer_[PREDELAY_MAX];
    uint16_t preDelayPos_;
    uint16_t preDelayTime_;
    
    float decayTime_;
    float roomSize_;
    float damping_;
    float mix_;
    
    // Diffusion
    int16_t diffBuf1_[256];
    int16_t diffBuf2_[256];
    uint16_t diffPos1_, diffPos2_;

    bool enabled_;
    bool frozen_;
};

#endif
