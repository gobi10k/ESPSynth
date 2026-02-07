#include "Oscillator.h"
#include "MathUtils.h"
#include <math.h>

// Pre-calculated supersaw multipliers (avoid powf per sample)
static const float SUPERSAW_MULT[7] = {
    0.9937f,  // -0.11 semitones
    0.9965f,  // -0.06 semitones  
    0.9988f,  // -0.02 semitones
    1.0000f,  //  0.00 semitones
    1.0012f,  // +0.02 semitones
    1.0035f,  // +0.06 semitones
    1.0064f   // +0.11 semitones
};

Oscillator::Oscillator() : 
    phase_(0), 
    effectiveIncrement_(0),
    basePhaseIncrement_(0),
    frequency_(440.0f),
    baseFrequency_(440.0f),
    waveform_(Waveform::SAW),
    tableIndex_(2),
    amplitude_(0.5f),
    detuneCents_(0.0f),
    detuneMultiplier_(1.0f),
    coarseTune_(0),
    coarseMultiplier_(1.0f),
    supersawDetune_(0.5f),
    pulseWidth_(0.5f),
    fmMod_(0.0f),
    pwMod_(0.0f),
    pitchMod_(0.0f),
    pitchMult_(1.0f),
    noiseState_(22222),
    lastPulse_(0.0f)
{
    for (int i = 0; i < 7; i++) {
        supersawPhases_[i] = (uint32_t)(rand());  // Random start phases
    }
    setFrequency(440.0f);
}

void Oscillator::setFrequency(float freq, bool force) {
    if (!force && fabsf(freq - baseFrequency_) < 0.001f) return;

    baseFrequency_ = constrain(freq, 20.0f, 20000.0f);
    frequency_ = baseFrequency_ * detuneMultiplier_ * coarseMultiplier_;
    updatePhaseIncrement();
    tableIndex_ = Wavetables::tableIndexForFreq(frequency_);
}

void Oscillator::setWaveform(Waveform wf) {
    waveform_ = wf;
}

void Oscillator::setPitchMod(float semitones) {
    if (semitones != pitchMod_) {
        pitchMod_ = semitones;
        pitchMult_ = fastExp2(pitchMod_ / 12.0f);
        updateEffectiveIncrements();
    }
}

void Oscillator::setAmplitude(float amp) {
    amplitude_ = constrain(amp, 0.0f, 1.0f);
}

void Oscillator::setDetune(float cents) {
    detuneCents_ = constrain(cents, -100.0f, 100.0f);
    detuneMultiplier_ = powf(2.0f, detuneCents_ / 1200.0f);
    frequency_ = baseFrequency_ * detuneMultiplier_ * coarseMultiplier_;
    updatePhaseIncrement();
}

void Oscillator::setCoarse(int8_t semitones) {
    coarseTune_ = constrain(semitones, -24, 24);
    coarseMultiplier_ = powf(2.0f, coarseTune_ / 12.0f);
    frequency_ = baseFrequency_ * detuneMultiplier_ * coarseMultiplier_;
    updatePhaseIncrement();
}

void Oscillator::setPulseWidth(float pw) {
    pulseWidth_ = constrain(pw, 0.05f, 0.95f);
}

void Oscillator::setSupersawDetune(float d) {
    supersawDetune_ = constrain(d, 0.0f, 1.0f);
    updateEffectiveIncrements();
}

void Oscillator::updatePhaseIncrement() {
    basePhaseIncrement_ = (uint32_t)(frequency_ * PHASE_INCREMENT_MULTIPLIER);
    updateEffectiveIncrements();
}

void Oscillator::updateEffectiveIncrements() {
    effectiveIncrement_ = (uint32_t)(basePhaseIncrement_ * pitchMult_);
    if (waveform_ == Waveform::SUPERSAW) {
        for (int i = 0; i < 7; i++) {
            float ratio = 1.0f + (SUPERSAW_MULT[i] - 1.0f) * supersawDetune_ * 2.0f;
            effectiveSupersawIncrements_[i] = (uint32_t)(effectiveIncrement_ * ratio);
        }
    }
}

void Oscillator::resetPhase() {
    phase_ = 0;
}

float Oscillator::getPhase() const {
    return phase_ * PHASE_TO_FLOAT;
}

void Oscillator::setPhase(float phase) {
    phase_ = (uint32_t)(phase * (float)PHASE_MAX);
}

void Oscillator::sync() {
    phase_ = 0;
}


