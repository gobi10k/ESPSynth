#include "Voice.h"
#include "MathUtils.h"
#include <math.h>

const char* VOICE_FILTER_NAMES[] = {"SVF", "LADR"};
const char* VOICE_SYNTH_MODE_NAMES[] = {"STD", "FM", "SYNC", "RING"};

Voice::Voice() :
    state_(VoiceState::FREE),
    note_(0),
    velocity_(0),
    age_(0),
    oscMix_(0.5f),
    synthMode_(VoiceSynthMode::STANDARD),
    fmAmount_(1.0f),
    prevPhase_(0.0f),
    filterType_(VoiceFilterType::SVF),
    filterEnvAmount_(0.5f),
    filterEnvVelocity_(0.5f),
    filterKeyTracking_(0.5f),
    globalFilterMod_(0.0f),
    globalPitchMod_(0.0f),
    targetFreq_(440.0f),
    pan_(0.0f)
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
    velScalar_ = velocity / 127.0f;
    age_ = 0;
    state_ = VoiceState::ACTIVE;
    
    targetFreq_ = midiToFreq(note);
    svf_.setKeyFreq(targetFreq_);
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
    // Reset filter integrator state so a resonant stolen voice does not
    // inject its accumulated energy (click/burst) into the new note's attack.
    svf_.reset();
    ladder_.reset();
}

float Voice::process() {
    if (state_ == VoiceState::FREE) {
        return 0.0f;
    }
    
    age_++;
    
    // Smooth frequency only if needed
    float freq = targetFreq_;
    if (pitchSmooth_.getSmoothTime() > 0.0f) {
        freq = pitchSmooth_.process();
        osc_[0].setFrequency(freq);
        osc_[1].setFrequency(freq);
    }
    
    float oscOutput = 0.0f;
    
    switch (synthMode_) {
        case VoiceSynthMode::STANDARD: {
            float osc0 = osc_[0].process();
            float osc1 = osc_[1].process();
            oscOutput = (osc0 * (1.0f - oscMix_)) + (osc1 * oscMix_);
            break;
        }
        case VoiceSynthMode::FM: {
            // Oscillator 1 modulates Oscillator 0
            float modulator = osc_[1].process();
            oscOutput = osc_[0].processWithFM(modulator, fmAmount_);
            break;
        }
        case VoiceSynthMode::RING: {
            float osc0 = osc_[0].process();
            float osc1 = osc_[1].process();
            oscOutput = osc0 * osc1;
            break;
        }
        case VoiceSynthMode::SYNC: {
            // Oscillator 0 resets Oscillator 1 phase
            float osc0 = osc_[0].process();
            if (osc_[0].getPhase() < prevPhase_) {
                osc_[1].sync();
            }
            prevPhase_ = osc_[0].getPhase();
            float osc1 = osc_[1].process();
            oscOutput = (osc0 * (1.0f - oscMix_)) + (osc1 * oscMix_);
            break;
        }
        default:
            oscOutput = osc_[0].process();
    }

    float sample = oscOutput;

    // Apply filter with envelope modulation
    float filterEnvVal = filterEnv_.process();

    // Update filter coefficients every 8 samples for performance
    if ((age_ & 0x07) == 0) {
        // Apply velocity scaling to filter envelope amount
        float velocityMod = 1.0f - filterEnvVelocity_ + (velScalar_ * filterEnvVelocity_);
        float effectiveEnvAmount = filterEnvAmount_ * velocityMod;

        float cutoffMod = (filterEnvVal * effectiveEnvAmount + globalFilterMod_) * 5000.0f;
        if (filterType_ == VoiceFilterType::SVF) {
            svf_.updateCoefficients(cutoffMod);
        } else {
            ladder_.updateCoefficients(cutoffMod);
        }
    }

    if (filterType_ == VoiceFilterType::SVF) {
        sample = svf_.process(sample);
    } else {
        sample = ladder_.process(sample);
    }

    // Apply amplitude envelope
    float ampEnvVal = ampEnv_.process();
    if (isnan(ampEnvVal) || isinf(ampEnvVal)) {
        ampEnvVal = 0.0f;
    }
    
    sample *= ampEnvVal * velScalar_;

    // Safety check
    if (isnan(sample) || isinf(sample)) {
        sample = 0.0f;
    }
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;

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

void Voice::setSynthMode(VoiceSynthMode mode) {
    synthMode_ = mode;
}

void Voice::setFMAmount(float amount) {
    fmAmount_ = amount;
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

void Voice::setFilterEnvVelocity(float amount) {
    filterEnvVelocity_ = constrain(amount, 0.0f, 1.0f);
}

void Voice::setFilterKeyTracking(float amount) {
    filterKeyTracking_ = constrain(amount, 0.0f, 1.0f);
    svf_.setKeyTracking(filterKeyTracking_);
    ladder_.setKeyTracking(filterKeyTracking_);
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

void Voice::updateBlockParams() {
    if (state_ == VoiceState::FREE) return;

    // Apply global modulations once per block
    osc_[0].setPitchMod(globalPitchMod_);
    osc_[1].setPitchMod(globalPitchMod_);

    // Ensure oscillators are at right base frequency
    float freq = (pitchSmooth_.getSmoothTime() > 0.0f) ? pitchSmooth_.getCurrent() : targetFreq_;
    osc_[0].setFrequency(freq);
    osc_[1].setFrequency(freq);
}
