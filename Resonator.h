#ifndef RESONATOR_H
#define RESONATOR_H

#include "Config.h"
#include <atomic>

/**
 * Modal Resonator Bank
 * 
 * A bank of tuned bandpass filters that simulate the natural resonances
 * of physical objects. Creates bell-like, string-like, and drum-like textures.
 * 
 * Each resonator is a 2nd-order bandpass filter (biquad) tuned to a harmonic
 * or inharmonic frequency relationship with the fundamental.
 */

constexpr uint8_t MAX_RESONATORS = 6;

// Harmonic profiles for different character types
enum class ResonatorProfile : uint8_t {
    HARMONIC = 0,    // String/flute: 1, 2, 3, 4, 5, 6
    BELL,            // Inharmonic bell: 1, 2.4, 3.5, 4.2, 5.8, 6.7
    MEMBRANE,        // Drum-like: 1, 1.59, 2.14, 2.3, 2.65, 2.92
    TUBE,            // Closed tube: 1, 3, 5, 7, 9, 11
    MARIMBA,         // Bar percussion: 1, 2.76, 5.4, 8.9, 13.3, 18.6
    CUSTOM,          // User-defined ratios
    NUM_PROFILES
};

extern const char* RESONATOR_PROFILE_NAMES[];
extern const float RESONATOR_RATIOS[][MAX_RESONATORS];

class ResonatorBank {
public:
    ResonatorBank();
    
    // Set fundamental frequency (all resonators tune relative to this)
    void setFrequency(float hz);
    
    // Character profile
    void setProfile(ResonatorProfile profile);
    void setCustomRatios(const float* ratios);  // 6 ratios
    
    // Per-resonator control
    void setResonatorGain(int index, float gain);     // 0-1 per resonator
    void setResonatorDecay(int index, float decay);   // Decay time factor
    
    // Global parameters
    void setResonance(float q);       // Q factor for all resonators (0.5-50)
    void setDamping(float damp);      // Global damping (0-1, higher = shorter decay)
    void setBrightness(float bright); // High frequency rolloff (0-1)
    void setMix(float mix);           // Dry/wet (0=dry, 1=resonator only)
    
    float getFrequency() const { return fundamental_; }
    ResonatorProfile getProfile() const { return profile_; }
    float getResonance() const { return resonance_; }
    float getDamping() const { return damping_; }
    float getBrightness() const { return brightness_; }
    float getMix() const { return mix_; }

    // Process audio
    float process(float input);
    
    // Reset state (call on note-on for clean attack)
    void reset();

private:
    void updateCoefficients();
    void applyDirtyCoefficients();

    inline float processResonator(int index, float input) {
        // Direct Form II Transposed biquad optimized for Bandpass (b1=0, b2=-b0)
        float b0 = b0_[index];
        float output = b0 * input + x1_[index];
        x1_[index] = -a1_[index] * output + x2_[index];
        x2_[index] = -b0 * input - a2_[index] * output;
        return output;
    }
    
    float fundamental_;
    ResonatorProfile profile_;
    float customRatios_[MAX_RESONATORS];
    
    // Per-resonator parameters
    float gains_[MAX_RESONATORS];
    float decayFactors_[MAX_RESONATORS];
    
    // Global parameters
    float resonance_;
    float damping_;
    float brightness_;
    float mix_;
    
    // Biquad coefficients per resonator
    float b0_[MAX_RESONATORS], b1_[MAX_RESONATORS], b2_[MAX_RESONATORS];
    float a1_[MAX_RESONATORS], a2_[MAX_RESONATORS];
    
    // Shadow coefficients for atomic updates
    float nb0_[MAX_RESONATORS], nb1_[MAX_RESONATORS], nb2_[MAX_RESONATORS];
    float na1_[MAX_RESONATORS], na2_[MAX_RESONATORS];
    std::atomic<bool> dirty_;

    // Biquad state per resonator
    float x1_[MAX_RESONATORS], x2_[MAX_RESONATORS];
    float y1_[MAX_RESONATORS], y2_[MAX_RESONATORS];
    
    // Brightness filter (one-pole lowpass on output)
    float brightnessCoef_;
    float brightnessState_;
};

#endif
