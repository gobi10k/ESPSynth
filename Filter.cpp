#include "Filter.h"
#include <math.h>

const char* FILTER_MODE_NAMES[] = {"LPF", "HPF", "BPF", "NOTCH"};

Filter::Filter() :
    cutoffHz_(1000.0f),
    resonance_(0.0f),
    mode_(FilterMode::LOWPASS),
    cutoffMod_(0.0f),
    f_(0.0f),
    q_(1.0f),
    low_(0.0f),
    high_(0.0f),
    band_(0.0f),
    notch_(0.0f)
{
    updateCoefficients();
}

void Filter::setCutoff(float hz) {
    cutoffHz_ = constrain(hz, 20.0f, 20000.0f);
    updateCoefficients();
}

void Filter::setResonance(float r) {
    resonance_ = constrain(r, 0.0f, 1.0f);
    // Q from ~0.5 (no resonance) to ~50 (self-oscillation)
    q_ = 1.0f - resonance_ * 0.98f;
}

void Filter::setMode(FilterMode mode) {
    mode_ = mode;
}

void Filter::updateCoefficients() {
    // Attempt to keep stable at high frequencies
    float normalizedFreq = cutoffHz_ / SAMPLE_RATE;
    f_ = 2.0f * sinf(M_PI * min(normalizedFreq, 0.45f));
}

void Filter::reset() {
    low_ = 0.0f;
    high_ = 0.0f;
    band_ = 0.0f;
    notch_ = 0.0f;
}

float Filter::process(float input) {
    // Apply cutoff modulation
    float modFreq = cutoffHz_ + cutoffMod_;
    modFreq = constrain(modFreq, 20.0f, 20000.0f);
    float normalizedFreq = modFreq / SAMPLE_RATE;
    float fMod = 2.0f * sinf(M_PI * min(normalizedFreq, 0.45f));
    
    // Reset modulation for next sample
    cutoffMod_ = 0.0f;
    
    // State variable filter iteration (2x oversampled for stability)
    for (int i = 0; i < 2; i++) {
        low_ += fMod * band_;
        high_ = input - low_ - q_ * band_;
        band_ += fMod * high_;
        notch_ = high_ + low_;
    }
    
    // Select output based on mode
    switch (mode_) {
        case FilterMode::LOWPASS:
            return low_;
        case FilterMode::HIGHPASS:
            return high_;
        case FilterMode::BANDPASS:
            return band_;
        case FilterMode::NOTCH:
            return notch_;
        default:
            return low_;
    }
}
