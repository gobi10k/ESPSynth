#include "CombFilter.h"
#include <cmath>
#include <cstring>

const char* COMB_MODE_NAMES[] = {"FB", "FF", "AP", "K-S"};

// Precomputed conversion factors
constexpr float INT16_TO_FLOAT = 1.0f / 32768.0f;
constexpr float FLOAT_TO_INT16 = 32768.0f;

CombFilter::CombFilter() :
    writePos_(0),
    delaySamples_(100.0f),
    feedback_(0.7f),
    damping_(0.3f),
    mode_(CombMode::FEEDBACK),
    mix_(0.5f),
    detuneRatio_(1.0f),
    dampState_(0.0f),
    allpassCoef_(0.5f),
    exciteLevel_(0.0f),
    exciteCounter_(0),
    noiseState_(0xABCD),
    lastDelayed_(0.0f),
    interpType_(InterpType::LINEAR_FAST)
{
    oneMinusDamping_ = 1.0f - damping_;
    oneMinusMix_ = 1.0f - mix_;
    reset();
}

void CombFilter::setPitch(float hz) {
    hz = constrain(hz, 40.0f, 5000.0f);
    delaySamples_ = SAMPLE_RATE / hz;
    delaySamples_ *= detuneRatio_;
    
    // Clamp with safety margin
    if (delaySamples_ >= COMB_BUFFER_SIZE - 2) {
        delaySamples_ = COMB_BUFFER_SIZE - 2;
    }
    if (delaySamples_ < 2.0f) {
        delaySamples_ = 2.0f;
    }
}

void CombFilter::setDelaySamples(float samples) {
    if (samples < 2.0f) samples = 2.0f;
    else if (samples > COMB_BUFFER_SIZE - 2) samples = COMB_BUFFER_SIZE - 2;
    delaySamples_ = samples;
}

void CombFilter::setFeedback(float fb) {
    if (fb < -0.99f) fb = -0.99f;
    else if (fb > 0.99f) fb = 0.99f;
    feedback_ = fb;
}

void CombFilter::setDamping(float damp) {
    if (damp < 0.0f) damp = 0.0f;
    else if (damp > 0.99f) damp = 0.99f;
    damping_ = damp;
    oneMinusDamping_ = 1.0f - damp;
}

void CombFilter::setMode(CombMode mode) {
    mode_ = mode;
}

void CombFilter::setMix(float mix) {
    if (mix < 0.0f) mix = 0.0f;
    else if (mix > 1.0f) mix = 1.0f;
    mix_ = mix;
    oneMinusMix_ = 1.0f - mix;
}

void CombFilter::setDetune(float cents) {
    // Use fast approximation
    detuneRatio_ = fastExp2(cents / 1200.0f);
}

float CombFilter::getPitch() const {
    return SAMPLE_RATE / delaySamples_;
}

void CombFilter::excite(float amplitude) {
    exciteLevel_ = amplitude;
    exciteCounter_ = static_cast<int16_t>(delaySamples_);
}

void CombFilter::reset() {
    memset(buffer_, 0, sizeof(buffer_));
    writePos_ = 0;
    dampState_ = 0.0f;
    exciteLevel_ = 0.0f;
    exciteCounter_ = 0;
    lastDelayed_ = 0.0f;
    noiseState_ = 0xABCD;
}

inline float CombFilter::int16ToFloat(int16_t value) {
    // Fast int16 to float conversion
    return static_cast<float>(value) * INT16_TO_FLOAT;
}

inline int16_t CombFilter::floatToInt16(float value) {
    // Fast float to int16 with clipping
    if (value > 1.0f) value = 1.0f;
    else if (value < -1.0f) value = -1.0f;
    return static_cast<int16_t>(value * 32767.0f);
}

inline float CombFilter::readDelay(float delay) {
    // No interpolation (fastest)
    float readPos = static_cast<float>(writePos_) - delay;
    if (readPos < 0.0f) readPos += COMB_BUFFER_SIZE;
    
    int readIdx = static_cast<int>(readPos);
    return int16ToFloat(buffer_[readIdx]);
}

inline float CombFilter::readDelayLinear(float delay) {
    // Linear interpolation
    float readPos = static_cast<float>(writePos_) - delay;
    if (readPos < 0.0f) readPos += COMB_BUFFER_SIZE;
    
    int readIdx0 = static_cast<int>(readPos);
    int readIdx1 = readIdx0 + 1;
    if (readIdx1 >= COMB_BUFFER_SIZE) readIdx1 = 0;
    
    float frac = readPos - static_cast<float>(readIdx0);
    
    float s0 = int16ToFloat(buffer_[readIdx0]);
    float s1 = int16ToFloat(buffer_[readIdx1]);
    
    return s0 + frac * (s1 - s0);
}

inline float CombFilter::readDelayLinearFast(float delay) {
    // Optimized linear interpolation with integer math
    float readPos = static_cast<float>(writePos_) - delay;
    if (readPos < 0.0f) readPos += COMB_BUFFER_SIZE;
    
    int readIdx0 = static_cast<int>(readPos);
    int readIdx1 = readIdx0 + 1;
    if (readIdx1 >= COMB_BUFFER_SIZE) readIdx1 = 0;
    
    float frac = readPos - static_cast<float>(readIdx0);
    
    // Get integer values and interpolate in integer domain
    int32_t s0 = buffer_[readIdx0];
    int32_t s1 = buffer_[readIdx1];
    
    // Linear interpolation: s0 + frac*(s1 - s0)
    // Scale to maintain precision
    int32_t result = s0 + static_cast<int32_t>(frac * (s1 - s0));
    
    return static_cast<float>(result) * INT16_TO_FLOAT;
}

inline void CombFilter::writeBuffer(float value) {
    // Use polynomial soft clipping for better quality
    value = fastPolyClip(value);
    buffer_[writePos_] = floatToInt16(value);
    writePos_++;
    if (writePos_ >= COMB_BUFFER_SIZE) {
        writePos_ = 0;
    }
}

inline float CombFilter::processSample(float input) {
    // Handle excitation
    if (exciteCounter_ > 0) {
        input += fastRandFloat(noiseState_) * exciteLevel_;
        exciteCounter_--;
    }
    
    // Read delayed signal with selected interpolation
    float delayed = 0.0f;
    switch (interpType_) {
        case InterpType::NONE:
            delayed = readDelay(delaySamples_);
            break;
        case InterpType::LINEAR:
            delayed = readDelayLinear(delaySamples_);
            break;
        case InterpType::LINEAR_FAST:
            delayed = readDelayLinearFast(delaySamples_);
            break;
    }
    
    // Damping filter with precomputed values
    if (damping_ > 0.001f) {
        dampState_ = delayed * oneMinusDamping_ + dampState_ * damping_;
        delayed = dampState_;
    }
    
    float output = 0.0f;
    float toWrite = 0.0f;
    
    switch (mode_) {
        case CombMode::FEEDBACK:
            output = input + feedback_ * delayed;
            toWrite = output;
            break;
            
        case CombMode::FEEDFORWARD:
            output = input + feedback_ * delayed;
            toWrite = input;
            break;
            
        case CombMode::ALLPASS: {
            float diff = input - delayed;
            output = delayed + allpassCoef_ * diff;
            toWrite = input + feedback_ * delayed;
            break;
        }
            
        case CombMode::KARPLUS_STRONG: {
            output = input + feedback_ * delayed;
            float averaged = (delayed + lastDelayed_) * 0.5f;
            lastDelayed_ = delayed;
            toWrite = input + feedback_ * averaged;
            break;
        }
    }
    
    // Write to buffer
    writeBuffer(toWrite);
    
    // Mix with precomputed factors
    return input * oneMinusMix_ + output * mix_;
}

float CombFilter::process(float input) {
    #ifdef DEBUG
    if (isnan(input) || isinf(input)) {
        return 0.0f;
    }
    #endif
    
    return processSample(input);
}