#include "Voice.h"
#include <math.h>

const char* VOICE_FILTER_NAMES[] = {"SVF", "LADR"};

Voice::Voice() :
    state_(VoiceState::FREE),
    note_(0),
    velocity_(0),
    age_(0),
    oscMix_(0.5f),
    filterType_(VoiceFilterType::SVF),
    filterEnvAmount_(0.5f),
    targetFreq_(440.0f)
{
    osc_[0].setWaveform(Waveform::SAW);
    osc_[0].setAmplitude(1.0f);
    osc_[1].setWaveform(Waveform::SAW);
    osc_[1].setAmplitude(1.0f);
    osc_[1].setDetune(7.0f);
    
    svf_.setCutoff(2000.0f);
    svf_.setResonance(0.3f);
    
    ladder_.setCutoff(2000.0f);
    ladder_.setResonance(0.3f);
    
    ampEnv_.setADSR(0.01f, 0.1f, 0.7f, 0.3f);
    filterEnv_.setADSR(0.01f, 0.2f, 0.3f, 0.5f);
    
    pitchSmooth_.setSmoothTime(0.0f);
}

float Voice::midiToFreq(uint8_t note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

void Voice::noteOn(uint8_t note, uint8_t velocity) {
    note_ = note;
    velocity_ = velocity;
    age_ = 0;
    state_ = VoiceState::ACTIVE;
    
    targetFreq_ = midiToFreq(note);
    ladder_.setKeyFreq(targetFreq_);
    
    if (pitchSmooth_.getCurrent() == 0.0f) {
        pitchSmooth_.setImmediate(targetFreq_);
    } else {
        pitchSmooth_.setTarget(targetFreq_);
    }
    
    ampEnv_.gate(true);
    filterEnv_.gate(true);
}

void Voice::noteOff() {
    if (state_ == VoiceState::ACTIVE) {
        state_ = VoiceState::RELEASING;
        ampEnv_.gate(false);
        filterEnv_.gate(false);
    }
}

void Voice::forceOff() {
    state_ = VoiceState::FREE;
    ampEnv_.gate(false);
    filterEnv_.gate(false);
    pitchSmooth_.setImmediate(0.0f);
}

float Voice::process() {
    if (state_ == VoiceState::FREE) {
        return 0.0f;
    }
    
    age_++;
    
    // Get frequency
    float freq = pitchSmooth_.process();
    if (freq < 20.0f || freq > 20000.0f || isnan(freq) || isinf(freq)) {
        freq = 440.0f;
    }
    
    // Set oscillator frequencies
    osc_[0].setFrequency(freq);
    osc_[1].setFrequency(freq);
    
    // Get oscillator output - just use osc 0 for simplicity
    float sample = osc_[0].process();
    
    // Safety check
    if (isnan(sample) || isinf(sample)) {
        sample = 0.0f;
    }
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    
    // Simple envelope
    float ampEnvVal = ampEnv_.process();
    if (isnan(ampEnvVal) || isinf(ampEnvVal)) {
        ampEnvVal = 0.0f;
    }
    
    float velScale = velocity_ / 127.0f;
    sample *= ampEnvVal * velScale;
    
    // Check if voice should be freed
    if (state_ == VoiceState::RELEASING && !ampEnv_.isActive()) {
        state_ = VoiceState::FREE;
    }
    
    return sample;
}

void Voice::setOscWaveform(int osc, Waveform wf) {
    if (osc >= 0 && osc < 2) {
        osc_[osc].setWaveform(wf);
    }
}

void Voice::setOscDetune(int osc, float cents) {
    if (osc >= 0 && osc < 2) {
        osc_[osc].setDetune(cents);
    }
}

void Voice::setOscMix(float mix) {
    oscMix_ = constrain(mix, 0.0f, 1.0f);
}

void Voice::setFilterType(VoiceFilterType type) {
    filterType_ = type;
}

void Voice::setFilterCutoff(float hz) {
    svf_.setCutoff(hz);
    ladder_.setCutoff(hz);
}

void Voice::setFilterResonance(float r) {
    svf_.setResonance(r);
    ladder_.setResonance(r);
}

void Voice::setFilterMode(FilterMode mode) {
    svf_.setMode(mode);
    LadderMode ladderMode = LadderMode::LP24;
    switch (mode) {
        case FilterMode::LOWPASS: ladderMode = LadderMode::LP24; break;
        case FilterMode::HIGHPASS: ladderMode = LadderMode::HP24; break;
        case FilterMode::BANDPASS: ladderMode = LadderMode::BP12; break;
        default: ladderMode = LadderMode::LP24;
    }
    ladder_.setMode(ladderMode);
}

void Voice::setFilterEnvAmount(float amount) {
    filterEnvAmount_ = constrain(amount, -1.0f, 1.0f);
}

void Voice::setAmpADSR(float a, float d, float s, float r) {
    ampEnv_.setADSR(a, d, s, r);
}

void Voice::setFilterADSR(float a, float d, float s, float r) {
    filterEnv_.setADSR(a, d, s, r);
}

void Voice::setGlideTime(float ms) {
    pitchSmooth_.setSmoothTime(ms);
}
