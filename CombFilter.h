#ifndef COMB_FILTER_H
#define COMB_FILTER_H

#include "Config.h"

/**
 * Memory-Optimized Comb Filter Resonator
 * 
 * Uses int16_t buffer to save memory.
 * Reduced buffer size (1200 samples = 25Hz minimum at 48kHz)
 * 
 * Memory: ~2.4KB per comb (vs 9.6KB with floats)
 */

// Reduced buffer: 25Hz minimum pitch
constexpr uint16_t COMB_BUFFER_SIZE = 1200;

enum class CombMode : uint8_t {
    FEEDBACK = 0,
    FEEDFORWARD,
    ALLPASS,
    KARPLUS_STRONG,
    NUM_MODES
};

extern const char* COMB_MODE_NAMES[];

class CombFilter {
public:
    CombFilter();
    
    void setPitch(float hz);
    void setDelaySamples(float samples);
    void setFeedback(float fb);
    void setDamping(float damp);
    void setMode(CombMode mode);
    void setMix(float mix);
    void setDetune(float cents);
    
    float getPitch() const;
    float getFeedback() const { return feedback_; }
    CombMode getMode() const { return mode_; }
    
    float process(float input);
    void excite(float amplitude = 1.0f);
    void reset();

private:
    // int16 buffer: 1200 × 2 = 2,400 bytes
    int16_t buffer_[COMB_BUFFER_SIZE];
    uint16_t writePos_;
    float delaySamples_;
    float feedback_;
    float damping_;
    CombMode mode_;
    float mix_;
    float detuneRatio_;
    float dampState_;
    float allpassCoef_;
    float exciteLevel_;
    int exciteCounter_;
    float lastDelayed_;  // For K-S averaging
};

#endif
