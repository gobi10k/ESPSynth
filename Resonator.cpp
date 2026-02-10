#include "Resonator.h"
#include "MathUtils.h"
#include <math.h>
#include <string.h>

const char* RESONATOR_PROFILE_NAMES[] = {
    "HARM", "BELL", "DRUM", "TUBE", "MRMB", "CUST"
};

// Harmonic ratios for each profile - tuned for better spectral character
const float RESONATOR_RATIOS[][MAX_RESONATORS] = {
    {1.0f, 2.001f, 3.0f, 4.002f, 5.001f, 6.0f},       // HARMONIC (string)
    {1.0f, 1.6f, 2.381f, 3.0f, 3.487f, 4.393f},       // BELL (inharmonic)
    {1.0f, 1.585f, 2.135f, 2.297f, 2.647f, 2.917f},   // MEMBRANE (drum)
    {1.0f, 3.001f, 5.0f, 7.001f, 9.0f, 11.001f},      // TUBE (odd harmonics)
    {1.0f, 3.997f, 9.432f, 17.06f, 26.89f, 38.91f},   // MARIMBA (bar)
    {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}              // CUSTOM
};

ResonatorBank::ResonatorBank() :
    fundamental_(220.0f),
    profile_(ResonatorProfile::HARMONIC),
    resonance_(10.0f),
    damping_(0.3f),
    brightness_(0.8f),
    mix_(0.5f),
    brightnessCoef_(0.0f),
    brightnessState_(0.0f),
    dirty_(false)
{
    // Default gains (decreasing for higher partials)
    for (int i = 0; i < MAX_RESONATORS; i++) {
        gains_[i] = 1.0f / (i + 1);  // 1, 0.5, 0.33, 0.25, 0.2, 0.17
        decayFactors_[i] = 1.0f;
        customRatios_[i] = RESONATOR_RATIOS[(int)ResonatorProfile::HARMONIC][i];
    }
    
    reset();
    updateCoefficients();
}

void ResonatorBank::setFrequency(float hz) {
    fundamental_ = constrain(hz, 20.0f, 5000.0f);
    updateCoefficients();
}

void ResonatorBank::setProfile(ResonatorProfile profile) {
    profile_ = profile;
    updateCoefficients();
}

void ResonatorBank::setCustomRatios(const float* ratios) {
    for (int i = 0; i < MAX_RESONATORS; i++) {
        customRatios_[i] = constrain(ratios[i], 0.25f, 32.0f);
    }
    if (profile_ == ResonatorProfile::CUSTOM) {
        updateCoefficients();
    }
}

void ResonatorBank::setResonatorGain(int index, float gain) {
    if (index >= 0 && index < MAX_RESONATORS) {
        gains_[index] = constrain(gain, 0.0f, 2.0f);
    }
}

void ResonatorBank::setResonatorDecay(int index, float decay) {
    if (index >= 0 && index < MAX_RESONATORS) {
        decayFactors_[index] = constrain(decay, 0.1f, 4.0f);
    }
}

void ResonatorBank::setResonance(float q) {
    resonance_ = constrain(q, 0.5f, 50.0f);
    updateCoefficients();
}

void ResonatorBank::setDamping(float damp) {
    damping_ = constrain(damp, 0.0f, 1.0f);
    // Damping affects Q inversely
    updateCoefficients();
}

void ResonatorBank::setBrightness(float bright) {
    brightness_ = constrain(bright, 0.0f, 1.0f);
    // Convert to lowpass coefficient
    // brightness=1 means full bandwidth, brightness=0 means heavy filtering
    float cutoff = 500.0f + brightness_ * 15000.0f;
    float normalizedFreq = cutoff / SAMPLE_RATE;
    brightnessCoef_ = 1.0f - expf(-2.0f * M_PI * normalizedFreq);
}

void ResonatorBank::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

void ResonatorBank::reset() {
    for (int i = 0; i < MAX_RESONATORS; i++) {
        x1_[i] = x2_[i] = 0.0f;
    }
    brightnessState_ = 0.0f;
}

void ResonatorBank::updateCoefficients() {
    const float* ratios = (profile_ == ResonatorProfile::CUSTOM) ?
                          customRatios_ :
                          RESONATOR_RATIOS[(int)profile_];
    
    // Effective Q considering damping - wider range for more "ping"
    float effectiveQ = resonance_ * (1.0f - damping_ * 0.5f);
    effectiveQ = max(1.0f, effectiveQ);
    
    for (int i = 0; i < MAX_RESONATORS; i++) {
        float freq = fundamental_ * ratios[i];
        
        // Clamp to Nyquist with safety margin
        if (freq >= SAMPLE_RATE * 0.48f) {
            nb0_[i] = 0.0f;
            na1_[i] = 0.0f;
            na2_[i] = 0.0f;
            continue;
        }
        
        // Higher partials decay with more control
        float partialQ = effectiveQ / (1.0f + i * 0.1f * damping_);
        partialQ *= decayFactors_[i];
        
        // 2-pole resonator coefficients
        // Pole radius r determines the decay time/bandwidth
        float r = 1.0f - (M_PI * freq) / (SAMPLE_RATE * partialQ);
        r = constrain(r, 0.0f, 0.999f);

        float theta = 2.0f * M_PI * freq / SAMPLE_RATE;
        na1_[i] = -2.0f * r * cosf(theta);
        na2_[i] = r * r;
        
        // Gain normalization: ensure peak gain is audible but safe
        // Using (1-r) gives roughly unity gain for a resonator
        nb0_[i] = (1.0f - r) * (1.0f + damping_ * 0.5f);
    }
    
    dirty_ = true;
    setBrightness(brightness_);  // Update brightness filter
}

void ResonatorBank::applyDirtyCoefficients() {
    if (!dirty_) return;

    for (int i = 0; i < MAX_RESONATORS; i++) {
        b0_[i] = nb0_[i];
        a1_[i] = na1_[i];
        a2_[i] = na2_[i];
    }
    dirty_ = false;
}


float ResonatorBank::process(float input) {
    if (isnan(input) || isinf(input)) return 0.0f;

    applyDirtyCoefficients();

    // Sum all resonators
    float resonated = 0.0f;
    
    for (int i = 0; i < MAX_RESONATORS; i++) {
        resonated += processResonator(i, input) * gains_[i];
    }
    
    // Normalize and filter - balanced for headroom
    resonated *= 0.25f;
    brightnessState_ += brightnessCoef_ * (resonated - brightnessState_);
    
    // Output limiting to prevent explosion at high Q
    float out = fastTanh(brightnessState_);

    // Mix dry and wet
    return input * (1.0f - mix_) + out * mix_;
}