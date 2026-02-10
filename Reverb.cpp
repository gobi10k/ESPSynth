#include "Reverb.h"
#include <math.h>
#include <string.h>

FDNReverb::FDNReverb() :
    feedbackGain_(0.5f),
    dampingCoef_(0.5f),
    inputGain_(0.125f),  // 0.25 * 0.5 for better headroom
    preDelayPos_(0),
    preDelaySamples_(240),
    preDelayMs_(5.0f),
    decayTime_(1.5f),
    roomSize_(0.5f),
    damping_(0.5f),
    mix_(0.3f),
    diffCoeff1_(0.7f),  // Improved diffusion coefficients
    diffCoeff2_(0.6f),
    enabled_(false),
    frozen_(false)
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
        dampState_[i] = 0.0f;
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
    
    // Size scaling with more natural curve
    float scale = 0.3f + roomSize_ * 0.7f;
    scale = scale * scale;  // Quadratic for more natural scaling
    
    for (int i = 0; i < 4; i++) {
        delayTimes_[i] = (uint16_t)(FDN_DELAYS[i] * scale);
        if (delayTimes_[i] < 20) delayTimes_[i] = 20;
        if (delayTimes_[i] >= FDN_MAX_DELAY) delayTimes_[i] = FDN_MAX_DELAY - 1;
    }
    
    updateDecayCoefficients();
}

void FDNReverb::setDamping(float damp) {
    damping_ = constrain(damp, 0.0f, 1.0f);
    dampingCoef_ = 1.0f - damping_;
    updateInternalCoefficients();
}

void FDNReverb::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

void FDNReverb::setPreDelay(float ms) {
    preDelayMs_ = ms;
    preDelaySamples_ = (uint16_t)(ms * SAMPLE_RATE / 1000.0f);
    if (preDelaySamples_ >= PREDELAY_MAX) preDelaySamples_ = PREDELAY_MAX - 1;
}

void FDNReverb::updateDecayCoefficients() {
    float avgDelay = 0.0f;
    for (int i = 0; i < 4; i++) {
        avgDelay += delayTimes_[i];
    }
    avgDelay *= 0.25f;  // Faster than / 4.0f
    
    float samplesForRT60 = decayTime_ * SAMPLE_RATE;
    float loopsForRT60 = samplesForRT60 / avgDelay;
    
    feedbackGain_ = powf(0.001f, 1.0f / loopsForRT60);
    if (feedbackGain_ > 0.99f) feedbackGain_ = 0.99f;
    if (feedbackGain_ < 0.01f) feedbackGain_ = 0.01f;
    
    updateInternalCoefficients();
}

void FDNReverb::updateInternalCoefficients() {
    // Adjust input gain based on feedback to prevent clipping
    inputGain_ = 0.25f * (1.0f - feedbackGain_ * 0.8f);
    
    // Update diffusion coefficients based on damping
    diffCoeff1_ = 0.7f * (1.0f - damping_ * 0.3f);
    diffCoeff2_ = 0.6f * (1.0f - damping_ * 0.3f);
}

float FDNReverb::process(float input) {
    float l, r;
    processStereo(input, input, l, r);
    return (l + r) * 0.5f;
}

void FDNReverb::processStereo(float inL, float inR, float& outL, float& outR, bool liteMode) {
    if (!enabled_) {
        outL = inL;
        outR = inR;
        return;
    }
    
    // Early exit for invalid inputs
    if (SAFE_CHECK(inL) || SAFE_CHECK(inR)) {
        outL = inL;
        outR = inR;
        return;
    }

    // Mono mix with early-out for silence
    float monoInput = (inL + inR) * 0.5f;
    if (fabsf(monoInput) < 1e-6f && !frozen_) {
        outL = inL;
        outR = inR;
        return;
    }

    // Input Diffusion (Optimized)
    const float inv32768 = 1.0f/32768.0f;
    float diffOut2 = monoInput;

    if (!liteMode) {
        // Diffusion 1
        uint16_t rp_d1 = (diffPos1_ - 113) & DIFF_MASK;
        float diff1 = diffBuf1_[rp_d1] * inv32768;
        float diffOut1 = -diffCoeff1_ * monoInput + diff1;
        float toWriteD1 = monoInput + diffCoeff1_ * diffOut1;
        if (toWriteD1 > 1.0f) toWriteD1 = 1.0f; else if (toWriteD1 < -1.0f) toWriteD1 = -1.0f;
        diffBuf1_[diffPos1_] = (int16_t)(toWriteD1 * 32767.0f);
        diffPos1_ = (diffPos1_ + 1) & DIFF_MASK;

        // Diffusion 2
        uint16_t rp_d2 = (diffPos2_ - 199) & DIFF_MASK;
        float diff2 = diffBuf2_[rp_d2] * inv32768;
        float toWriteD2 = diffOut1 + diffCoeff2_ * (-diffCoeff2_ * diffOut1 + diff2);
        if (toWriteD2 > 1.0f) toWriteD2 = 1.0f; else if (toWriteD2 < -1.0f) toWriteD2 = -1.0f;
        diffOut2 = -diffCoeff2_ * diffOut1 + diff2;
        diffBuf2_[diffPos2_] = (int16_t)(toWriteD2 * 32767.0f);
        diffPos2_ = (diffPos2_ + 1) & DIFF_MASK;
    }
    
    monoInput = diffOut2;

    // Pre-delay (Optimized)
    uint16_t rp_pd = (preDelayPos_ - preDelaySamples_) & PREDELAY_MASK;
    float preDelayed = preDelayBuffer_[rp_pd] * inv32768;
    
    preDelayBuffer_[preDelayPos_] = (int16_t)(diffOut2 * 32767.0f);
    preDelayPos_ = (preDelayPos_ + 1) & PREDELAY_MASK;
    
    float preDelayedF = frozen_ ? 0.0f : preDelayed;
    float currentFeedback = frozen_ ? 1.0f : feedbackGain_;
    float currentDamping = frozen_ ? 0.0f : damping_;
    float currentDampCoef = frozen_ ? 1.0f : dampingCoef_;
    
    // Read from delay lines (Unrolled)
    const float inv32768 = 1.0f/32768.0f;
    float outputs[4];
    
    uint32_t rp0 = (writePos_[0] - delayTimes_[0]) & DELAY_MASK;
    float d0 = delayLines_[0][rp0] * inv32768;
    dampState_[0] = dampState_[0] * currentDamping + d0 * currentDampCoef;
    outputs[0] = dampState_[0];

    uint32_t rp1 = (writePos_[1] - delayTimes_[1]) & DELAY_MASK;
    float d1 = delayLines_[1][rp1] * inv32768;
    dampState_[1] = dampState_[1] * currentDamping + d1 * currentDampCoef;
    outputs[1] = dampState_[1];

    uint32_t rp2 = (writePos_[2] - delayTimes_[2]) & DELAY_MASK;
    float d2 = delayLines_[2][rp2] * inv32768;
    dampState_[2] = dampState_[2] * currentDamping + d2 * currentDampCoef;
    outputs[2] = dampState_[2];

    uint32_t rp3 = (writePos_[3] - delayTimes_[3]) & DELAY_MASK;
    float d3 = delayLines_[3][rp3] * inv32768;
    dampState_[3] = dampState_[3] * currentDamping + d3 * currentDampCoef;
    outputs[3] = dampState_[3];

    // Optimized Hadamard mixing (Unrolled)
    float s01 = outputs[0] + outputs[1];
    float s23 = outputs[2] + outputs[3];
    float d01 = outputs[0] - outputs[1];
    float d23 = outputs[2] - outputs[3];

    float m[4];
    m[0] = (s01 + s23) * 0.5f;
    m[1] = (d01 + d23) * 0.5f;
    m[2] = (s01 - s23) * 0.5f;
    m[3] = (d01 - d23) * 0.5f;
    
    // Write back with feedback
    float feedbackInput = preDelayedF * inputGain_;
    
    // Optimized Hadamard Mixing & Writeback (Unrolled)
    float c0 = m[0] * currentFeedback + feedbackInput;
    if (c0 > 1.0f) c0 = 1.0f; else if (c0 < -1.0f) c0 = -1.0f;
    delayLines_[0][writePos_[0]] = (int16_t)(c0 * 32767.0f);
    writePos_[0] = (writePos_[0] + 1) & DELAY_MASK;

    float c1 = m[1] * currentFeedback + feedbackInput;
    if (c1 > 1.0f) c1 = 1.0f; else if (c1 < -1.0f) c1 = -1.0f;
    delayLines_[1][writePos_[1]] = (int16_t)(c1 * 32767.0f);
    writePos_[1] = (writePos_[1] + 1) & DELAY_MASK;

    float c2 = m[2] * currentFeedback + feedbackInput;
    if (c2 > 1.0f) c2 = 1.0f; else if (c2 < -1.0f) c2 = -1.0f;
    delayLines_[2][writePos_[2]] = (int16_t)(c2 * 32767.0f);
    writePos_[2] = (writePos_[2] + 1) & DELAY_MASK;

    float c3 = m[3] * currentFeedback + feedbackInput;
    if (c3 > 1.0f) c3 = 1.0f; else if (c3 < -1.0f) c3 = -1.0f;
    delayLines_[3][writePos_[3]] = (int16_t)(c3 * 32767.0f);
    writePos_[3] = (writePos_[3] + 1) & DELAY_MASK;
    
    // Improved stereo output with subtle cross-feed
    float wetL = outputs[0] * 0.6f + outputs[1] * 0.4f + outputs[2] * 0.15f;
    float wetR = outputs[2] * 0.4f + outputs[3] * 0.6f + outputs[0] * 0.15f;
    
    // Wet/dry mix with optimized calculation
    float wetMix = mix_;
    float dryMix = 1.0f - wetMix;
    
    outL = inL * dryMix + wetL * wetMix;
    outR = inR * dryMix + wetR * wetMix;
}