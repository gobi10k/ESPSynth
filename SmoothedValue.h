#ifndef SMOOTHED_VALUE_H
#define SMOOTHED_VALUE_H

#include "Config.h"

/**
 * Smoothed parameter value to prevent zipper noise
 * Uses one-pole lowpass filter for smooth transitions
 */
class SmoothedValue {
public:
    SmoothedValue(float initialValue = 0.0f, float smoothTimeMs = 5.0f) :
        current_(initialValue),
        target_(initialValue),
        coef_(0.0f),
        smoothTime_(smoothTimeMs)
    {
        setSmoothTime(smoothTimeMs);
    }
    
    void setSmoothTime(float ms, float sampleRate = SAMPLE_RATE) {
        smoothTime_ = ms;
        if (ms <= 0.0f) {
            coef_ = 1.0f;  // Instant
        } else {
            // Time constant for ~99% convergence
            coef_ = 1.0f - expf(-1000.0f / (ms * sampleRate));
        }
    }
    
    void setTarget(float value) {
        target_ = value;
    }
    
    void setImmediate(float value) {
        current_ = target_ = value;
    }
    
    float process() {
        current_ += coef_ * (target_ - current_);
        return current_;
    }
    
    float getCurrent() const { return current_; }
    float getTarget() const { return target_; }
    float getSmoothTime() const { return smoothTime_; }
    bool isSmoothing() const { return fabsf(target_ - current_) > 0.0001f; }

private:
    float current_;
    float target_;
    float coef_;
    float smoothTime_;
};

#endif
