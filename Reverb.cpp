#include "Reverb.h"
#include <math.h>
#include <string.h>

FDNReverb::FDNReverb() :
    feedbackGain_(0.5f),
    dampingCoef_(0.5f),
    preDelayPos_(0),
    preDelayTime_(240),
    decayTime_(1.5f),
    roomSize_(0.5f),
    damping_(0.5f),
    mix_(0.3f),
    enabled_(false)
{
    reset();
    
    for (int i = 0; i < 4; i++) {
        delayTimes_[i] = FDN_DELAYS[i];
        dampState_[i] = 0.0f;
    }
    
    updateDecayCoefficients();
}

void FDNReverb::reset() {
    for (int i = 0; i < 4; i++) {
        memset(delayLines_[i], 0, sizeof(delayLines_[i]));
        writePos_[i] = 0;
    }
    memset(preDelayBuffer_, 0, sizeof(preDelayBuffer_));
    preDelayPos_ = 0;
}

void FDNReverb::setDecay(float seconds) {
    decayTime_ = constrain(seconds, 0.1f, 10.0f);
    updateDecayCoefficients();
}

void FDNReverb::setSize(float size) {
    roomSize_ = constrain(size, 0.0f, 1.0f);
    
    float scale = 0.4f + roomSize_ * 0.6f;
    for (int i = 0; i < 4; i++) {
        delayTimes_[i] = (uint16_t)(FDN_DELAYS[i] * scale);
        if (delayTimes_[i] < 10) delayTimes_[i] = 10;
        if (delayTimes_[i] >= FDN_MAX_DELAY) delayTimes_[i] = FDN_MAX_DELAY - 1;
    }
    
    updateDecayCoefficients();
}

void FDNReverb::setDamping(float damp) {
    damping_ = constrain(damp, 0.0f, 0.95f);
    dampingCoef_ = 1.0f - damping_;
}

void FDNReverb::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

void FDNReverb::setPreDelay(float ms) {
    preDelayTime_ = (uint16_t)(ms * SAMPLE_RATE / 1000.0f);
    if (preDelayTime_ >= PREDELAY_MAX) preDelayTime_ = PREDELAY_MAX - 1;
}

void FDNReverb::updateDecayCoefficients() {
    float avgDelay = 0.0f;
    for (int i = 0; i < 4; i++) {
        avgDelay += delayTimes_[i];
    }
    avgDelay /= 4.0f;
    
    float samplesForRT60 = decayTime_ * SAMPLE_RATE;
    float loopsForRT60 = samplesForRT60 / avgDelay;
    
    feedbackGain_ = powf(0.001f, 1.0f / loopsForRT60);
    if (feedbackGain_ > 0.98f) feedbackGain_ = 0.98f;
}

float FDNReverb::process(float input) {
    if (!enabled_) return input;
    
    // Pre-delay
    int16_t preDelayed = preDelayBuffer_[(preDelayPos_ - preDelayTime_ + PREDELAY_MAX) % PREDELAY_MAX];
    preDelayBuffer_[preDelayPos_] = (int16_t)(input * 32000.0f);
    preDelayPos_ = (preDelayPos_ + 1) % PREDELAY_MAX;
    
    float preDelayedF = preDelayed / 32000.0f;
    
    // Read from delay lines
    float outputs[4];
    for (int i = 0; i < 4; i++) {
        int readPos = (writePos_[i] - delayTimes_[i] + FDN_MAX_DELAY) % FDN_MAX_DELAY;
        outputs[i] = delayLines_[i][readPos] / 32000.0f;
        
        // Damping filter
        dampState_[i] = dampState_[i] * damping_ + outputs[i] * dampingCoef_;
        outputs[i] = dampState_[i];
    }
    
    // Hadamard mixing (efficient orthogonal)
    float mixed[4];
    mixed[0] = 0.5f * (outputs[0] + outputs[1] + outputs[2] + outputs[3]);
    mixed[1] = 0.5f * (outputs[0] - outputs[1] + outputs[2] - outputs[3]);
    mixed[2] = 0.5f * (outputs[0] + outputs[1] - outputs[2] - outputs[3]);
    mixed[3] = 0.5f * (outputs[0] - outputs[1] - outputs[2] + outputs[3]);
    
    // Write back with feedback
    float inputGain = 0.25f;
    for (int i = 0; i < 4; i++) {
        float toWrite = mixed[i] * feedbackGain_ + preDelayedF * inputGain;
        // Soft clip before storing
        if (toWrite > 1.0f) toWrite = 1.0f;
        if (toWrite < -1.0f) toWrite = -1.0f;
        delayLines_[i][writePos_[i]] = (int16_t)(toWrite * 32000.0f);
        writePos_[i] = (writePos_[i] + 1) % FDN_MAX_DELAY;
    }
    
    // Output sum
    float wet = (outputs[0] + outputs[1] + outputs[2] + outputs[3]) * 0.25f;
    
    return input * (1.0f - mix_) + wet * mix_;
}
