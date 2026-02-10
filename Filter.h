#ifndef FILTER_H
#define FILTER_H

#include "Config.h"

enum class FilterMode : uint8_t {
    LOWPASS = 0,
    HIGHPASS,
    BANDPASS,
    NOTCH,
    NUM_MODES
};

extern const char* FILTER_MODE_NAMES[];

/**
 * State Variable Filter (Chamberlin)
 * 
 * Classic topology, well-suited for modulation.
 * Provides simultaneous LP/HP/BP/Notch outputs.
 */
class Filter {
public:
    Filter();
    
    void setCutoff(float hz);
    void setResonance(float q);  // 0-1, self-oscillates near 1
    void setMode(FilterMode mode);
    void setKeyTracking(float amount);
    void setKeyFreq(float hz) { keyFreq_ = hz; }
    
    float getCutoff() const { return cutoffHz_; }
    float getResonance() const { return resonance_; }
    FilterMode getMode() const { return mode_; }
    
    // Modulation input - added to cutoff each sample
    void setCutoffMod(float mod) { cutoffMod_ = mod; }
    void updateCoefficients(float modHz);
    
    inline float process(float input) {
        // True 2x oversampling with linear interpolation
        float halfInput = (lastInput_ + input) * 0.5f;

        // First iteration (half-sample)
        low_ += fMod_ * band_;
        high_ = halfInput - low_ - q_ * band_;
        band_ += fMod_ * high_;

        // Second iteration (full-sample)
        low_ += fMod_ * band_;
        high_ = input - low_ - q_ * band_;
        band_ += fMod_ * high_;

        notch_ = high_ + low_;
        lastInput_ = input;

        float out = *activeOutput_;
        CHECK_AND_RESET(out, *this);
        return out;
    }
    
    // Access individual outputs
    float getLowpass() const { return low_; }
    float getHighpass() const { return high_; }
    float getBandpass() const { return band_; }
    float getNotch() const { return notch_; }
    
    void reset();

private:
    float cutoffHz_;
    float resonance_;
    FilterMode mode_;
    float keyTracking_;
    float keyFreq_;
    
    float cutoffMod_;
    
    // Filter coefficients
    float fMod_;   // Frequency coefficient
    float q_;   // Resonance coefficient
    
    // State variables
    float low_;
    float high_;
    float band_;
    float notch_;
    float* activeOutput_;
    float lastInput_;
};

#endif
