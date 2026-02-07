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
    
    diffPos1_ = diffPos2_ = 0;
    updateDecayCoefficients();
}

void FDNReverb::reset() {
    for (int i = 0; i < 4; i++) {
        memset(delayLines_[i], 0, sizeof(delayLines_[i]));
        writePos_[i] = 0;
    }
    memset(preDelayBuffer_, 0, sizeof(preDelayBuffer_));
    preDelayPos_ = 0;
    memset(diffBuf1_, 0, sizeof(diffBuf1_));
    memset(diffBuf2_, 0, sizeof(diffBuf2_));
    diffPos1_ = diffPos2_ = 0;
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
    float l, r;
    processStereo(input, input, l, r);
    return (l + r) * 0.5f;
}

void FDNReverb::processStereo(float inL, float inR, float& outL, float& outR) {
    if (!enabled_) {
        outL = inL;
        outR = inR;
        return;
    }
    
    if (isnan(inL) || isinf(inL)) inL = 0.0f;
    if (isnan(inR) || isinf(inR)) inR = 0.0f;

    float monoInput = (inL + inR) * 0.5f;

    // Input Diffusion
    auto diffuse = [](float input, int16_t* buf, uint16_t& pos, int len, float coeff) {
        int readPos = (int)pos - len;
        if (readPos < 0) readPos += 256;
        float delayed = buf[readPos] / 32000.0f;
        float output = -coeff * input + delayed;
        buf[pos] = (int16_t)(constrain(input + coeff * output, -1.0f, 1.0f) * 32000.0f);
        pos = (pos + 1) % 256;
        return output;
    };

    monoInput = diffuse(monoInput, diffBuf1_, diffPos1_, 113, 0.6f);
    monoInput = diffuse(monoInput, diffBuf2_, diffPos2_, 199, 0.6f);

    // Pre-delay using safe index math
    int preReadPos = (int)preDelayPos_ - (int)preDelayTime_;
    if (preReadPos < 0) preReadPos += PREDELAY_MAX;

    int16_t preDelayed = preDelayBuffer_[preReadPos];
    float clampedInput = monoInput;
    if (clampedInput > 1.0f) clampedInput = 1.0f;
    else if (clampedInput < -1.0f) clampedInput = -1.0f;
    preDelayBuffer_[preDelayPos_] = (int16_t)(clampedInput * 32000.0f);
    preDelayPos_++;
    if (preDelayPos_ >= PREDELAY_MAX) preDelayPos_ = 0;
    
    float preDelayedF = preDelayed / 32000.0f;
    
    // Read from delay lines and apply damping - Unrolled
    float outputs[4];
    int rp;

    rp = (int)writePos_[0] - (int)delayTimes_[0];
    if (rp < 0) rp += FDN_MAX_DELAY;
    outputs[0] = delayLines_[0][rp] * 3.125e-5f; // 1/32000
    dampState_[0] = dampState_[0] * damping_ + outputs[0] * dampingCoef_;
    outputs[0] = dampState_[0];

    rp = (int)writePos_[1] - (int)delayTimes_[1];
    if (rp < 0) rp += FDN_MAX_DELAY;
    outputs[1] = delayLines_[1][rp] * 3.125e-5f;
    dampState_[1] = dampState_[1] * damping_ + outputs[1] * dampingCoef_;
    outputs[1] = dampState_[1];

    rp = (int)writePos_[2] - (int)delayTimes_[2];
    if (rp < 0) rp += FDN_MAX_DELAY;
    outputs[2] = delayLines_[2][rp] * 3.125e-5f;
    dampState_[2] = dampState_[2] * damping_ + outputs[2] * dampingCoef_;
    outputs[2] = dampState_[2];

    rp = (int)writePos_[3] - (int)delayTimes_[3];
    if (rp < 0) rp += FDN_MAX_DELAY;
    outputs[3] = delayLines_[3][rp] * 3.125e-5f;
    dampState_[3] = dampState_[3] * damping_ + outputs[3] * dampingCoef_;
    outputs[3] = dampState_[3];
    
    // Hadamard mixing (efficient orthogonal)
    float m0 = 0.5f * (outputs[0] + outputs[1] + outputs[2] + outputs[3]);
    float m1 = 0.5f * (outputs[0] - outputs[1] + outputs[2] - outputs[3]);
    float m2 = 0.5f * (outputs[0] + outputs[1] - outputs[2] - outputs[3]);
    float m3 = 0.5f * (outputs[0] - outputs[1] - outputs[2] + outputs[3]);
    
    // Write back with feedback - Unrolled
    float ig = 0.25f;
    float tw;

    tw = m0 * feedbackGain_ + preDelayedF * ig;
    if (tw > 1.0f) tw = 1.0f; else if (tw < -1.0f) tw = -1.0f;
    delayLines_[0][writePos_[0]] = (int16_t)(tw * 32000.0f);
    if (++writePos_[0] >= FDN_MAX_DELAY) writePos_[0] = 0;

    tw = m1 * feedbackGain_ + preDelayedF * ig;
    if (tw > 1.0f) tw = 1.0f; else if (tw < -1.0f) tw = -1.0f;
    delayLines_[1][writePos_[1]] = (int16_t)(tw * 32000.0f);
    if (++writePos_[1] >= FDN_MAX_DELAY) writePos_[1] = 0;

    tw = m2 * feedbackGain_ + preDelayedF * ig;
    if (tw > 1.0f) tw = 1.0f; else if (tw < -1.0f) tw = -1.0f;
    delayLines_[2][writePos_[2]] = (int16_t)(tw * 32000.0f);
    if (++writePos_[2] >= FDN_MAX_DELAY) writePos_[2] = 0;

    tw = m3 * feedbackGain_ + preDelayedF * ig;
    if (tw > 1.0f) tw = 1.0f; else if (tw < -1.0f) tw = -1.0f;
    delayLines_[3][writePos_[3]] = (int16_t)(tw * 32000.0f);
    if (++writePos_[3] >= FDN_MAX_DELAY) writePos_[3] = 0;
    
    // Stereo Output: split the 4 channels into 2 pairs
    float wetL = (outputs[0] + outputs[1]) * 0.5f;
    float wetR = (outputs[2] + outputs[3]) * 0.5f;
    
    outL = inL * (1.0f - mix_) + wetL * mix_;
    outR = inR * (1.0f - mix_) + wetR * mix_;
}
