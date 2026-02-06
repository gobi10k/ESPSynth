#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include "Config.h"
#include "Wavetables.h"
#include "MathUtils.h"

class Oscillator {
public:
    Oscillator();
    
    // Basic parameters
    void setFrequency(float freq, bool force = false);
    void setWaveform(Waveform wf);
    void setAmplitude(float amp);
    void setDetune(float cents);      // Detune in cents (-100 to +100)
    void setPulseWidth(float pw);     // 0.1 to 0.9 for pulse wave
    
    float getFrequency() const { return frequency_; }
    Waveform getWaveform() const { return waveform_; }
    float getAmplitude() const { return amplitude_; }
    float getDetune() const { return detuneCents_; }
    float getPulseWidth() const { return pulseWidth_; }
    
    // Modulation inputs (call before process())
    void setFMMod(float mod) { fmMod_ = mod; }
    void setPitchMod(float semitones);
    
    // Processing
    inline float process() {
        float sample = 0.0f;
        switch (waveform_) {
            case Waveform::SINE:     sample = Wavetables::readSine(phase_); break;
            case Waveform::SAW:      sample = Wavetables::readSaw(phase_, tableIndex_); break;
            case Waveform::SQUARE:   sample = Wavetables::readSquare(phase_, tableIndex_); break;
            case Waveform::TRIANGLE: sample = Wavetables::readTriangle(phase_, tableIndex_); break;
            case Waveform::PULSE: {
                float t = phase_ * PHASE_TO_FLOAT;
                sample = (t < pulseWidth_) ? 1.0f : -1.0f;
                sample = lastPulse_ * 0.3f + sample * 0.7f;
                lastPulse_ = sample;
                break;
            }
            case Waveform::SUPERSAW: sample = generateSupersaw(); break;
            case Waveform::NOISE:    sample = generateNoise(); break;
            default: sample = 0.0f;
        }
        phase_ += effectiveIncrement_;
        return sample * amplitude_;
    }

    inline float processWithFM(float fmInput, float fmAmount) {
        float fmOffset = fmInput * fmAmount * (float)basePhaseIncrement_;
        float totalIncrement = (float)basePhaseIncrement_ + fmOffset;
        if (totalIncrement < 0.0f) totalIncrement = 0.0f;
        if (totalIncrement > (float)PHASE_MAX * 0.5f) totalIncrement = (float)PHASE_MAX * 0.5f;
        uint32_t effInc = (uint32_t)(totalIncrement * pitchMult_);

        float sample = 0.0f;
        switch (waveform_) {
            case Waveform::SINE:     sample = Wavetables::readSine(phase_); break;
            case Waveform::SAW:      sample = Wavetables::readSaw(phase_, tableIndex_); break;
            case Waveform::SQUARE:   sample = Wavetables::readSquare(phase_, tableIndex_); break;
            case Waveform::TRIANGLE: sample = Wavetables::readTriangle(phase_, tableIndex_); break;
            case Waveform::PULSE: {
                float t = phase_ * PHASE_TO_FLOAT;
                sample = (t < pulseWidth_) ? 1.0f : -1.0f;
                sample = lastPulse_ * 0.3f + sample * 0.7f;
                lastPulse_ = sample;
                break;
            }
            case Waveform::SUPERSAW: sample = generateSupersaw(); break;
            case Waveform::NOISE:    sample = generateNoise(); break;
            default: sample = Wavetables::readSine(phase_);
        }
        phase_ += effInc;
        return sample * amplitude_;
    }
    
    // Phase control
    void resetPhase();
    float getPhase() const;
    void setPhase(float phase);
    void sync();

private:
    void updatePhaseIncrement();
    void updateEffectiveIncrements();
    float generateSupersaw();
    float generateNoise();
    
    uint32_t phase_;
    uint32_t effectiveIncrement_;
    uint32_t basePhaseIncrement_;
    
    float frequency_;
    float baseFrequency_;
    Waveform waveform_;
    int tableIndex_;
    float amplitude_;
    
    // Extended parameters
    float detuneCents_;
    float detuneMultiplier_;
    float pulseWidth_;
    
    // Modulation
    float fmMod_;
    float pitchMod_;
    float pitchMult_;
    
    // Supersaw state (7 detuned saws)
    uint32_t supersawPhases_[7];
    uint32_t effectiveSupersawIncrements_[7];
    
    // Noise state
    uint32_t noiseState_;
    
    // Pulse filter state
    float lastPulse_;
};

#endif
