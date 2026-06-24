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
    void setMorph(float morph) { morph_ = morph; }
    void setCustomTable(int slot, float* table, uint16_t size) {
        if (slot == 0) {
            customTable_ = table;
            customTableSize_ = size;
        } else {
            customTableB_ = table;
        }
    }
    
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
            case Waveform::RAMP_DOWN: sample = Wavetables::readRampDown(phase_, tableIndex_); break;
            case Waveform::MORPH:    sample = Wavetables::readMorph(phase_, morph_, tableIndex_); break;
            case Waveform::SD_TABLE: sample = Wavetables::readCustomMorph(phase_, customTable_, customTableB_, morph_, customTableSize_); break;
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
            case Waveform::RAMP_DOWN: sample = Wavetables::readRampDown(phase_, tableIndex_); break;
            case Waveform::MORPH:    sample = Wavetables::readMorph(phase_, morph_, tableIndex_); break;
            case Waveform::SD_TABLE: sample = Wavetables::readCustomMorph(phase_, customTable_, customTableB_, morph_, customTableSize_); break;
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

    inline float generateSupersaw() {
        if (effectiveIncrement_ == 0) return 0.0f;
        float sum = 0.0f;
        for (int i = 0; i < 7; i++) {
            supersawPhases_[i] += effectiveSupersawIncrements_[i];
            sum += Wavetables::readSaw(supersawPhases_[i], tableIndex_);
        }
        return sum * 0.143f;
    }

    inline float generateNoise() {
        return fastRandFloat(noiseState_);
    }

private:
    void updatePhaseIncrement();
    void updateEffectiveIncrements();
    
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

    // Morph and Custom
    float morph_;
    float* customTable_;
    float* customTableB_;
    uint16_t customTableSize_;
};

#endif
