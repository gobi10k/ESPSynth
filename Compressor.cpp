#include "Compressor.h"
#include "MathUtils.h"
#include <math.h>

Compressor::Compressor() :
    thresholdDb_(-12.0f),
    thresholdLin_(0.25f),
    ratio_(4.0f),
    attackCoef_(0.0f),
    releaseCoef_(0.0f),
    makeupGain_(1.0f),
    kneeWidth_(6.0f),
    kneeHalf_(3.0f),
    kneeInv_(0.0833f),
    ratioMinusOne_(-0.75f),
    envelope_(0.0f),
    gainReductionDb_(0.0f),
    enabled_(false)
{
    setThreshold(-12.0f);
    setRatio(4.0f);
    setAttack(10.0f);
    setRelease(100.0f);
    setMakeupGain(0.0f);
    setKnee(6.0f);
}

void Compressor::setThreshold(float dB) {
    thresholdDb_ = constrain(dB, -60.0f, 0.0f);
    thresholdLin_ = powf(10.0f, thresholdDb_ / 20.0f);
}

void Compressor::setRatio(float ratio) {
    ratio_ = constrain(ratio, 1.0f, 20.0f);
    ratioMinusOne_ = (1.0f / ratio_) - 1.0f;
}

void Compressor::setAttack(float ms) {
    ms = constrain(ms, 0.1f, 100.0f);
    attackCoef_ = expf(-1.0f / (ms * 0.001f * SAMPLE_RATE));
}

void Compressor::setRelease(float ms) {
    ms = constrain(ms, 10.0f, 1000.0f);
    releaseCoef_ = expf(-1.0f / (ms * 0.001f * SAMPLE_RATE));
}

void Compressor::setMakeupGain(float dB) {
    dB = constrain(dB, 0.0f, 24.0f);
    makeupGain_ = powf(10.0f, dB / 20.0f);
}

void Compressor::setKnee(float dB) {
    kneeWidth_ = constrain(dB, 0.0f, 12.0f);
    kneeHalf_ = kneeWidth_ * 0.5f;
    kneeInv_ = (kneeWidth_ > 0.0f) ? (1.0f / (2.0f * kneeWidth_)) : 0.0f;
}

float Compressor::process(float input) {
    float out = input;
    processStereo(input, input, out, out);
    return out;
}

void Compressor::processStereo(float inL, float inR, float& outL, float& outR) {
    if (!enabled_) {
        outL = inL;
        outR = inR;
        return;
    }

    if (isnan(inL) || isinf(inL)) inL = 0.0f;
    if (isnan(inR) || isinf(inR)) inR = 0.0f;
    
    // Get input level (sidechain: max of L/R)
    float inputAbs = max(fabsf(inL), fabsf(inR));
    
    // Convert to dB using fast log2
    float inputDb = (inputAbs > 0.00001f) ? 6.0206f * fastLog2(inputAbs) : -100.0f;
    
    // Calculate gain reduction with soft knee
    float overDb = inputDb - thresholdDb_;
    float gainDb = 0.0f;
    
    if (overDb > -kneeHalf_ && overDb < kneeHalf_) {
        // Soft knee region
        float kneeInput = overDb + kneeHalf_;
        gainDb = ratioMinusOne_ * kneeInput * kneeInput * kneeInv_;
    } else if (overDb >= kneeHalf_) {
        // Above knee - full compression
        gainDb = ratioMinusOne_ * overDb;
    }
    
    // Smooth the gain reduction (envelope follower)
    float targetEnv = -gainDb;
    if (targetEnv > envelope_) {
        envelope_ = attackCoef_ * envelope_ + (1.0f - attackCoef_) * targetEnv;
    } else {
        envelope_ = releaseCoef_ * envelope_ + (1.0f - releaseCoef_) * targetEnv;
    }
    
    gainReductionDb_ = envelope_;
    
    // Apply gain reduction
    float gainLin = fastExp(-envelope_ * 0.115129f) * makeupGain_;
    
    outL = inL * gainLin;
    outR = inR * gainLin;
}
