#ifndef EFFECTS_H
#define EFFECTS_H

#include "Config.h"

// ============================================================================
// DELAY LINE
// ============================================================================

// Max delay time ~250ms at 48kHz, using int16 to save memory
constexpr uint16_t MAX_DELAY_SAMPLES = 12000;

class Delay {
public:
    Delay();
    
    void setTime(float seconds);
    void setFeedback(float fb);     // 0-1 (careful >0.9)
    void setMix(float mix);         // 0=dry, 1=wet
    
    float getTime() const { return delayTime_; }
    float getFeedback() const { return feedback_; }
    float getMix() const { return mix_; }
    
    float process(float input);
    void clear();

private:
    int16_t buffer_[MAX_DELAY_SAMPLES];  // 16-bit saves 50% memory
    uint16_t writePos_;
    uint16_t delaySamples_;
    
    float delayTime_;
    float feedback_;
    float mix_;
};

// ============================================================================
// SATURATION / DISTORTION
// ============================================================================

enum class SaturationType : uint8_t {
    SOFT = 0,       // tanh - warm
    HARD,           // Hard clip
    FOLDBACK,       // Wavefolding
    BITCRUSH,       // Lo-fi
    NUM_TYPES
};

class Saturation {
public:
    Saturation();
    
    void setDrive(float drive);     // 1-10
    void setType(SaturationType type);
    void setMix(float mix);
    
    float getDrive() const { return drive_; }
    SaturationType getType() const { return type_; }
    
    float process(float input);

private:
    float drive_;
    float bitcrushScale_;  // cached powf(2, 16/drive_), updated in setDrive()
    SaturationType type_;
    float mix_;
};

// ============================================================================
// CHORUS
// ============================================================================

// Chorus delay buffer (~20ms max)
constexpr uint16_t CHORUS_BUFFER_SIZE = 1024;

class Chorus {
public:
    Chorus();
    
    void setRate(float hz);         // LFO rate
    void setDepth(float depth);     // 0-1
    void setMix(float mix);
    
    float getRate() const { return rate_; }
    float getDepth() const { return depth_; }
    
    float process(float input);
    void clear();

private:
    int16_t buffer_[CHORUS_BUFFER_SIZE];  // 16-bit saves memory
    uint16_t writePos_;
    
    float rate_;
    float depth_;
    float mix_;
    
    uint32_t lfoPhase_;
    uint32_t lfoIncrement_;
};

// ============================================================================
// EFFECTS CHAIN
// ============================================================================

class EffectsChain {
public:
    EffectsChain();
    
    Saturation saturation;
    Chorus chorus;
    Delay delay;
    
    void setEnabled(bool sat, bool chr, bool dly);
    
    float process(float input);

    bool isSatEnabled() const { return satEnabled_; }
    bool isChorusEnabled() const { return chorusEnabled_; }
    bool isDelayEnabled() const { return delayEnabled_; }

private:
    bool satEnabled_;
    bool chorusEnabled_;
    bool delayEnabled_;
};

#endif
