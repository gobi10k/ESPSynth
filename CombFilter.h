#ifndef COMB_FILTER_H
#define COMB_FILTER_H

#include "Config.h"
#include "MathUtils.h"
#include <cstdint>

/**
 * Memory-Optimized Comb Filter with CPU improvements
 * 
 * Keeps int16_t buffer for memory efficiency (2.4KB)
 * Adds CPU optimizations: power-of-two, interpolation, etc.
 */

// Keep original buffer size for memory compatibility
constexpr uint16_t COMB_BUFFER_SIZE = 1200;
// Use power of two if possible, but stay close to original
constexpr uint16_t COMB_BUFFER_MASK = 1023; // For 1024-sized power of two operations

enum class InterpType : uint8_t {
    NONE = 0,     // No interpolation (fastest)
    LINEAR,       // Linear interpolation
    LINEAR_FAST,  // Linear with int16 optimizations
    NUM_TYPES
};

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
    void setInterpolation(InterpType type) { interpType_ = type; }
    
    float getPitch() const;
    float getFeedback() const { return feedback_; }
    float getDamping() const { return damping_; }
    float getMix() const { return mix_; }
    CombMode getMode() const { return mode_; }
    
    float process(float input);
    void excite(float amplitude = 1.0f);
    void reset();

private:
    // int16 buffer: 1200 × 2 = 2,400 bytes (memory efficient)
    int16_t buffer_[COMB_BUFFER_SIZE];
    uint16_t writePos_;
    float delaySamples_;
    float feedback_;
    float damping_;
    CombMode mode_;
    float mix_;
    float detuneRatio_;
    
    // State variables
    float dampState_;
    float allpassCoef_;
    float exciteLevel_;
    int16_t exciteCounter_;
    uint32_t noiseState_;
    float lastDelayed_;
    
    // Optimization variables
    InterpType interpType_;
    float oneMinusDamping_;
    float oneMinusMix_;
    
    // Private methods
    inline float readDelay(float delay);
    inline float readDelayLinear(float delay);
    inline float readDelayLinearFast(float delay);
    inline void writeBuffer(float value);
    inline float int16ToFloat(int16_t value);
    inline int16_t floatToInt16(float value);
    
    // Fast inline processing
    __attribute__((always_inline)) 
    inline float processSample(float input);
};

#endif