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
    float process();
    float processWithFM(float fmInput, float fmAmount);
    
    // Phase control
    void resetPhase();
    float getPhase() const;
    void setPhase(float phase);
    void sync();

private:
    void updatePhaseIncrement();
    void updateEffectiveIncrements();
    float generateSupersaw(uint32_t baseIncrement);
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
    
    // Noise state
    uint32_t noiseState_;
    
};

#endif
