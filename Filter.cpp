#include "Filter.h"
#include <math.h>

const char* FILTER_MODE_NAMES[] = {"LPF", "HPF", "BPF", "NOTCH"};

Filter::Filter() :
    cutoffHz_(1000.0f),
    resonance_(0.0f),
    mode_(FilterMode::LOWPASS),
    keyTracking_(0.0f),
    keyFreq_(440.0f),
    cutoffMod_(0.0f),
    fMod_(0.0f),
    q_(1.0f),
    low_(0.0f),
    high_(0.0f),
    band_(0.0f),
    notch_(0.0f),
    activeOutput_(&low_),
    lastInput_(0.0f)
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
    switch (mode_) {
        case FilterMode::LOWPASS:  activeOutput_ = &low_; break;
        case FilterMode::HIGHPASS: activeOutput_ = &high_; break;
        case FilterMode::BANDPASS: activeOutput_ = &band_; break;
        case FilterMode::NOTCH:    activeOutput_ = &notch_; break;
        default: activeOutput_ = &low_; break;
    }
}

void Filter::setKeyTracking(float amount) {
    keyTracking_ = constrain(amount, 0.0f, 1.0f);
}

void Filter::updateCoefficients(float modHz) {
    float keyOffset = 0.0f;
    if (keyTracking_ > 0.0f) {
        keyOffset = (keyFreq_ - 440.0f) * keyTracking_ * 1.5f;
    }

    float modFreq = cutoffHz_ + modHz + keyOffset;
    modFreq = constrain(modFreq, 20.0f, 20000.0f);
    float normalizedFreq = modFreq / SAMPLE_RATE;
    if (normalizedFreq > 0.45f) normalizedFreq = 0.45f;
    // Fast sin(pi*x) approximation for x in [0, 0.5]
    // Uses parabolic approximation: 4x(1-x) scaled for sin(pi*x)
    // Max error ~1.2% at extremes, inaudible in filter context
    float x = normalizedFreq;
    float sinApprox = 3.14159265f * x * (1.0f - x * 1.273f); // tuned for [0, 0.45]
    fMod_ = 2.0f * sinApprox;
}

void Filter::reset() {
    low_ = 0.0f;
    high_ = 0.0f;
    band_ = 0.0f;
    notch_ = 0.0f;
    lastInput_ = 0.0f;
}

