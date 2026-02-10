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

void FDNReverb::processStereo(float inL, float inR, float& outL, float& outR) {
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

    // Input Diffusion (optimized inline)
    // Diffusion 1
    uint16_t readPos = diffPos1_ - 113;
    readPos &= DIFF_MASK;  // Fast modulo with power of 2
    float diff1 = diffBuf1_[readPos] * (1.0f/32768.0f);
    float diffOut1 = -diffCoeff1_ * monoInput + diff1;
    diffBuf1_[diffPos1_] = (int16_t)(constrain(monoInput + diffCoeff1_ * diffOut1, -1.0f, 1.0f) * 32767.0f);
    diffPos1_ = (diffPos1_ + 1) & DIFF_MASK;
    
    // Diffusion 2
    readPos = diffPos2_ - 199;
    readPos &= DIFF_MASK;
    float diff2 = diffBuf2_[readPos] * (1.0f/32768.0f);
    float diffOut2 = -diffCoeff2_ * diffOut1 + diff2;
    diffBuf2_[diffPos2_] = (int16_t)(constrain(diffOut1 + diffCoeff2_ * diffOut2, -1.0f, 1.0f) * 32767.0f);
    diffPos2_ = (diffPos2_ + 1) & DIFF_MASK;
    
    monoInput = diffOut2;

    // Pre-delay with optimized access
    readPos = preDelayPos_ - preDelaySamples_;
    readPos &= PREDELAY_MASK;
    float preDelayed = preDelayBuffer_[readPos] * (1.0f/32768.0f);
    
    // Write to pre-delay buffer
    preDelayBuffer_[preDelayPos_] = (int16_t)(monoInput * 32767.0f);
    preDelayPos_ = (preDelayPos_ + 1) & PREDELAY_MASK;
    
    float preDelayedF = frozen_ ? 0.0f : preDelayed;
    float currentFeedback = frozen_ ? 1.0f : feedbackGain_;
    float currentDamping = frozen_ ? 0.0f : damping_;
    float currentDampCoef = frozen_ ? 1.0f : dampingCoef_;
    
    // Read from delay lines with optimized access
    float outputs[4];
    uint32_t rp[4];
    
    // Calculate all read positions first
    for (int i = 0; i < 4; i++) {
        rp[i] = writePos_[i] - delayTimes_[i];
        rp[i] &= DELAY_MASK;
    }
    
    // Process all delay lines
    for (int i = 0; i < 4; i++) {
        float delayed = delayLines_[i][rp[i]] * (1.0f/32768.0f);
        dampState_[i] = dampState_[i] * currentDamping + delayed * currentDampCoef;
        outputs[i] = dampState_[i];
    }
    
    // Optimized Hadamard mixing (reduced operations)
    float sum01 = outputs[0] + outputs[1];
    float sum23 = outputs[2] + outputs[3];
    float diff01 = outputs[0] - outputs[1];
    float diff23 = outputs[2] - outputs[3];
    
    // Write back with feedback
    float feedbackInput = preDelayedF * inputGain_;
    
    for (int i = 0; i < 4; i++) {
        float mix;
        switch (i) {
            case 0: mix = (sum01 + sum23) * 0.5f; break;
            case 1: mix = (diff01 + diff23) * 0.5f; break;
            case 2: mix = (sum01 - sum23) * 0.5f; break;
            case 3: mix = (diff01 - diff23) * 0.5f; break;
        }
        
        float combined = mix * currentFeedback + feedbackInput;
        // Fast clamping
        combined = combined > 1.0f ? 1.0f : (combined < -1.0f ? -1.0f : combined);
        delayLines_[i][writePos_[i]] = (int16_t)(combined * 32767.0f);
        
        writePos_[i] = (writePos_[i] + 1) & DELAY_MASK;
    }
    
    // Improved stereo output with subtle cross-feed
    float wetL = outputs[0] * 0.6f + outputs[1] * 0.4f;
    float wetR = outputs[2] * 0.4f + outputs[3] * 0.6f;
    
    // Add subtle cross-feed for more realistic stereo image
    float cross = 0.15f;
    wetL += outputs[2] * cross;
    wetR += outputs[0] * cross;
    
    // Wet/dry mix with optimized calculation
    float wetMix = mix_;
    float dryMix = 1.0f - wetMix;
    
    outL = inL * dryMix + wetL * wetMix;
    outR = inR * dryMix + wetR * wetMix;
}