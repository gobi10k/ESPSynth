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
    
    float getCutoff() const { return cutoffHz_; }
    float getResonance() const { return resonance_; }
    FilterMode getMode() const { return mode_; }
    
    // Modulation input - added to cutoff each sample
    void setCutoffMod(float mod) { cutoffMod_ = mod; }
    void updateCoefficients(float modHz);
    
    float process(float input);
    
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
    
    float cutoffMod_;
    
    // Filter coefficients
    float fMod_;   // Frequency coefficient
    float q_;   // Resonance coefficient
    
    // State variables
    float low_;
    float high_;
    float band_;
    float notch_;
};

#endif
