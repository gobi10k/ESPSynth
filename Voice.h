#ifndef VOICE_H
#define VOICE_H

#include "Config.h"
#include "Oscillator.h"
#include "Envelope.h"
#include "Filter.h"
#include "MoogFilter.h"
#include "SmoothedValue.h"

enum class VoiceState : uint8_t {
    FREE = 0,
    ACTIVE,
    RELEASING
};

// Filter type selection (Resonator/Comb are global, not per-voice)
enum class VoiceFilterType : uint8_t {
    SVF = 0,      // State Variable Filter
    LADDER,       // Moog Ladder
    NUM_TYPES
};

extern const char* VOICE_FILTER_NAMES[];

class Voice {
public:
    Voice();
    
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff();
    void forceOff();
    
    float process();
    
    bool isFree() const { return state_ == VoiceState::FREE; }
    bool isActive() const { return state_ != VoiceState::FREE; }
    bool isReleasing() const { return state_ == VoiceState::RELEASING; }
    uint8_t getNote() const { return note_; }
    uint32_t getAge() const { return age_; }
    float getLevel() const { return ampEnv_.getValue(); }
    float getFrequency() const { return targetFreq_; }
    
    // Oscillators
    void setOscWaveform(int osc, Waveform wf);
    void setOscDetune(int osc, float cents);
    void setOscMix(float mix);
    
    // Filter selection and params
    void setFilterType(VoiceFilterType type);
    void setFilterCutoff(float hz);
    void setFilterResonance(float r);
    void setFilterMode(FilterMode mode);
    void setFilterEnvAmount(float amount);
    
    // Envelopes
    void setAmpADSR(float a, float d, float s, float r);
    void setFilterADSR(float a, float d, float s, float r);
    
    // Glide
    void setGlideTime(float ms);

private:
    float midiToFreq(uint8_t note);
    
    VoiceState state_;
    uint8_t note_;
    uint8_t velocity_;
    uint32_t age_;
    
    // Oscillators
    Oscillator osc_[2];
    float oscMix_;
    
    // Filter options (only SVF and Ladder per voice - both are small)
    VoiceFilterType filterType_;
    Filter svf_;
    LadderFilter ladder_;
    float filterEnvAmount_;
    
    // Envelopes
    Envelope ampEnv_;
    Envelope filterEnv_;
    
    // Glide
    SmoothedValue pitchSmooth_;
    float targetFreq_;
};

#endif
