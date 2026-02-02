#include "Filter.h"
#include <math.h>

const char* FILTER_MODE_NAMES[] = {"LPF", "HPF", "BPF", "NOTCH"};

Filter::Filter() :
    cutoffHz_(1000.0f),
    resonance_(0.0f),
    mode_(FilterMode::LOWPASS),
    cutoffMod_(0.0f),
    fMod_(0.0f),
    q_(1.0f),
    low_(0.0f),
    high_(0.0f),
    band_(0.0f),
    notch_(0.0f)
{
    updateCoefficients(0.0f);
}

void Filter::setCutoff(float hz) {
    cutoffHz_ = constrain(hz, 20.0f, 20000.0f);
    updateCoefficients(0.0f);
}

void Filter::setResonance(float r) {
    resonance_ = constrain(r, 0.0f, 1.0f);
    // Q from ~1.0 (no resonance) to ~0.02 (self-oscillation)
    q_ = 1.0f - resonance_ * 0.98f;
}

void Filter::setMode(FilterMode mode) {
    mode_ = mode;
}

void Filter::updateCoefficients(float modHz) {
    float modFreq = cutoffHz_ + modHz;
    modFreq = constrain(modFreq, 20.0f, 20000.0f);
    float normalizedFreq = modFreq / SAMPLE_RATE;
    fMod_ = 2.0f * sinf(M_PI * min(normalizedFreq, 0.45f));
}

void Filter::reset() {
    low_ = 0.0f;
    high_ = 0.0f;
    band_ = 0.0f;
    notch_ = 0.0f;
}

float Filter::process(float input) {
    // State variable filter iteration (2x oversampled for stability)
    for (int i = 0; i < 2; i++) {
        low_ += fMod_ * band_;
        high_ = input - low_ - q_ * band_;
        band_ += fMod_ * high_;
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
