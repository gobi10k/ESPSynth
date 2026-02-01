#include "Oscillator.h"
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
    phaseIncrement_(0),
    basePhaseIncrement_(0),
    frequency_(440.0f),
    baseFrequency_(440.0f),
    waveform_(Waveform::SAW),
    tableIndex_(2),
    amplitude_(0.5f),
    detuneCents_(0.0f),
    detuneMultiplier_(1.0f),
    pulseWidth_(0.5f),
    fmMod_(0.0f),
    pitchMod_(0.0f),
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
    frequency_ = baseFrequency_ * detuneMultiplier_;
    updatePhaseIncrement();
    tableIndex_ = Wavetables::tableIndexForFreq(frequency_);
}

void Oscillator::setWaveform(Waveform wf) {
    waveform_ = wf;
}

void Oscillator::setAmplitude(float amp) {
    amplitude_ = constrain(amp, 0.0f, 1.0f);
}

void Oscillator::setDetune(float cents) {
    detuneCents_ = constrain(cents, -100.0f, 100.0f);
    detuneMultiplier_ = powf(2.0f, detuneCents_ / 1200.0f);
    frequency_ = baseFrequency_ * detuneMultiplier_;
    updatePhaseIncrement();
}

void Oscillator::setPulseWidth(float pw) {
    pulseWidth_ = constrain(pw, 0.05f, 0.95f);
}

void Oscillator::updatePhaseIncrement() {
    basePhaseIncrement_ = (uint32_t)(frequency_ * PHASE_INCREMENT_MULTIPLIER);
    phaseIncrement_ = basePhaseIncrement_;
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

float Oscillator::generateSupersaw() {
    // Safety check
    if (basePhaseIncrement_ == 0) return 0.0f;
    if (tableIndex_ < 0 || tableIndex_ >= NUM_OCTAVE_TABLES) {
        tableIndex_ = 0;
    }
    
    float sum = 0.0f;
    
    for (int i = 0; i < 7; i++) {
        uint32_t inc = (uint32_t)(basePhaseIncrement_ * SUPERSAW_MULT[i]);
        supersawPhases_[i] += inc;
        sum += Wavetables::readSaw(supersawPhases_[i], tableIndex_);
    }
    
    return sum * 0.143f;  // 1/7 = 0.143
}

float Oscillator::generateNoise() {
    // Simple xorshift PRNG for white noise
    noiseState_ ^= noiseState_ << 13;
    noiseState_ ^= noiseState_ >> 17;
    noiseState_ ^= noiseState_ << 5;
    return (float)(int32_t)noiseState_ / (float)INT32_MAX;
}

float Oscillator::process() {
    // Apply pitch modulation
    uint32_t effectiveIncrement = basePhaseIncrement_;
    if (pitchMod_ != 0.0f) {
        float pitchMult = powf(2.0f, pitchMod_ / 12.0f);
        effectiveIncrement = (uint32_t)(basePhaseIncrement_ * pitchMult);
    }
    
    float sample = 0.0f;
    
    switch (waveform_) {
        case Waveform::SINE:
            sample = Wavetables::readSine(phase_);
            break;
            
        case Waveform::SAW:
            sample = Wavetables::readSaw(phase_, tableIndex_);
            break;
            
        case Waveform::SQUARE:
            sample = Wavetables::readSquare(phase_, tableIndex_);
            break;
            
        case Waveform::TRIANGLE:
            sample = Wavetables::readTriangle(phase_, tableIndex_);
            break;
            
        case Waveform::PULSE: {
            // Variable pulse width using phase comparison
            float t = phase_ * PHASE_TO_FLOAT;
            sample = (t < pulseWidth_) ? 1.0f : -1.0f;
            // Simple lowpass to reduce aliasing
            sample = lastPulse_ * 0.3f + sample * 0.7f;
            lastPulse_ = sample;
            break;
        }
            
        case Waveform::SUPERSAW:
            sample = generateSupersaw();
            break;
            
        case Waveform::NOISE:
            sample = generateNoise();
            break;
            
        default:
            sample = 0.0f;
    }
    
    phase_ += effectiveIncrement;
    pitchMod_ = 0.0f;  // Reset modulation
    
    return sample * amplitude_;
}

float Oscillator::processWithFM(float fmInput, float fmAmount) {
    // FM synthesis: modulate phase increment
    // Use signed math to handle negative fmInput correctly
    int32_t fmOffset = (int32_t)(fmInput * fmAmount * (float)basePhaseIncrement_);
    int32_t totalIncrement = (int32_t)basePhaseIncrement_ + fmOffset;

    // Clamp to non-negative to avoid backwards phase motion/wild noise
    if (totalIncrement < 0) totalIncrement = 0;
    uint32_t effectiveIncrement = (uint32_t)totalIncrement;
    
    // Apply pitch mod on top
    if (pitchMod_ != 0.0f) {
        float pitchMult = powf(2.0f, pitchMod_ / 12.0f);
        effectiveIncrement = (uint32_t)(effectiveIncrement * pitchMult);
    }
    
    float sample = 0.0f;
    
    switch (waveform_) {
        case Waveform::SINE:
            sample = Wavetables::readSine(phase_);
            break;
        case Waveform::SAW:
            sample = Wavetables::readSaw(phase_, tableIndex_);
            break;
        case Waveform::SQUARE:
            sample = Wavetables::readSquare(phase_, tableIndex_);
            break;
        case Waveform::TRIANGLE:
            sample = Wavetables::readTriangle(phase_, tableIndex_);
            break;
        case Waveform::PULSE: {
            float t = phase_ * PHASE_TO_FLOAT;
            sample = (t < pulseWidth_) ? 1.0f : -1.0f;
            sample = lastPulse_ * 0.3f + sample * 0.7f;
            lastPulse_ = sample;
            break;
        }
        case Waveform::SUPERSAW:
            sample = generateSupersaw();
            break;
        case Waveform::NOISE:
            sample = generateNoise();
            break;
        default:
            sample = Wavetables::readSine(phase_);
    }
    
    phase_ += effectiveIncrement;
    pitchMod_ = 0.0f;
    
    return sample * amplitude_;
}
