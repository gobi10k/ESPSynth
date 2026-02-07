#include "Effects.h"
#include "MathUtils.h"
#include "Wavetables.h"
#include <math.h>
#include <string.h>

// ============================================================================
// DELAY
// ============================================================================

Delay::Delay() :
    writePos_(0),
    delaySamples_(4800),
    delayTime_(0.1f),
    feedback_(0.3f),
    mix_(0.3f)
{
    clear();
}

void Delay::setTime(float seconds) {
    delayTime_ = constrain(seconds, 0.001f, 0.25f);  // Max 250ms
    delaySamples_ = (uint16_t)(delayTime_ * SAMPLE_RATE);
    if (delaySamples_ >= MAX_DELAY_SAMPLES) {
        delaySamples_ = MAX_DELAY_SAMPLES - 1;
    }
}

void Delay::setFeedback(float fb) {
    feedback_ = constrain(fb, 0.0f, 0.95f);
}

void Delay::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

void Delay::clear() {
    memset(bufferL_, 0, sizeof(bufferL_));
    memset(bufferR_, 0, sizeof(bufferR_));
    writePos_ = 0;
}

float Delay::process(float input) {
    float left = input;
    float right = input;
    processStereo(left, right);
    return (left + right) * 0.5f;
}

void Delay::processStereo(float& left, float& right) {
    if (isnan(left) || isinf(left)) left = 0.0f;
    if (isnan(right) || isinf(right)) right = 0.0f;

    int readPos = (int)writePos_ - (int)delaySamples_;
    if (readPos < 0) readPos += MAX_DELAY_SAMPLES;
    
    float delayedL = bufferL_[readPos] / 32767.0f;
    float delayedR = bufferR_[readPos] / 32767.0f;

    float toWriteL = left + delayedL * feedback_;
    float toWriteR = right + delayedR * feedback_;

    bufferL_[writePos_] = (int16_t)(constrain(toWriteL, -1.0f, 1.0f) * 32767.0f);
    bufferR_[writePos_] = (int16_t)(constrain(toWriteR, -1.0f, 1.0f) * 32767.0f);

    writePos_++;
    if (writePos_ >= MAX_DELAY_SAMPLES) writePos_ = 0;
    
    left = left * (1.0f - mix_) + delayedL * mix_;
    right = right * (1.0f - mix_) + delayedR * mix_;
}

// ============================================================================
// SATURATION
// ============================================================================

Saturation::Saturation() :
    drive_(1.0f),
    type_(SaturationType::SOFT),
    mix_(1.0f)
{
}

void Saturation::setDrive(float drive) {
    drive_ = constrain(drive, 1.0f, 20.0f);
}

void Saturation::setType(SaturationType type) {
    type_ = type;
}

void Saturation::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

float Saturation::process(float input) {
    float left = input;
    float right = input;
    processStereo(left, right);
    return (left + right) * 0.5f;
}

void Saturation::processStereo(float& left, float& right) {
    auto saturate = [&](float in) {
        if (isnan(in) || isinf(in)) return 0.0f;
        float driven = in * drive_;
    float saturated = 0.0f;
    
    switch (type_) {
        case SaturationType::SOFT:
            // Soft saturation using fastTanh
            saturated = fastTanh(driven);
            break;
            
        case SaturationType::HARD:
            // Hard clipping
            saturated = constrain(driven, -1.0f, 1.0f);
            break;
            
        case SaturationType::FOLDBACK: {
            // Wavefolding - non-looping version for stability
            float x = driven;
            // First fold
            if (x > 1.0f) x = 2.0f - x;
            else if (x < -1.0f) x = -2.0f - x;
            // Second fold
            if (x > 1.0f) x = 2.0f - x;
            else if (x < -1.0f) x = -2.0f - x;
            saturated = x;
            break;
        }
            
        case SaturationType::BITCRUSH: {
            // Reduce bit depth
            float bits = 16.0f / drive_;  // More drive = fewer bits
            float scale = fastExp2(bits);
            saturated = (float)((int)(driven * scale)) / scale;
            saturated = constrain(saturated, -1.0f, 1.0f);
            break;
        }
            
        default:
            saturated = driven;
    }
    
        // Compensate gain
        saturated /= max(1.0f, drive_ * 0.5f);
        return in * (1.0f - mix_) + saturated * mix_;
    };

    left = saturate(left);
    right = saturate(right);
}

// ============================================================================
// CHORUS
// ============================================================================

Chorus::Chorus() :
    writePos_(0),
    rate_(0.5f),
    depth_(0.5f),
    mix_(0.5f),
    lfoPhase_(0),
    lfoIncrement_(0)
{
    clear();
    setRate(0.5f);
}

void Chorus::setRate(float hz) {
    rate_ = constrain(hz, 0.1f, 5.0f);
    lfoIncrement_ = (uint32_t)(rate_ * PHASE_INCREMENT_MULTIPLIER);
}

void Chorus::setDepth(float d) {
    depth_ = constrain(d, 0.0f, 1.0f);
}

void Chorus::setMix(float m) {
    mix_ = constrain(m, 0.0f, 1.0f);
}

void Chorus::clear() {
    memset(bufferL_, 0, sizeof(bufferL_));
    memset(bufferR_, 0, sizeof(bufferR_));
    writePos_ = 0;
}

float Chorus::process(float input) {
    float left = input;
    float right = input;
    processStereo(left, right);
    return (left + right) * 0.5f;
}

void Chorus::processStereo(float& left, float& right) {
    if (isnan(left) || isinf(left)) left = 0.0f;
    if (isnan(right) || isinf(right)) right = 0.0f;

    bufferL_[writePos_] = (int16_t)(constrain(left, -1.0f, 1.0f) * 32000.0f);
    bufferR_[writePos_] = (int16_t)(constrain(right, -1.0f, 1.0f) * 32000.0f);
    
    float lfoL = Wavetables::readSine(lfoPhase_);
    float lfoR = Wavetables::readSine(lfoPhase_ + 0x40000000); // 90 deg offset
    lfoPhase_ += lfoIncrement_;
    
    float baseDelay = 0.007f * SAMPLE_RATE;
    float modAmount = depth_ * 0.003f * SAMPLE_RATE;
    
    auto readDelay = [&](int16_t* buf, float lfoVal) {
        float delaySamples = baseDelay + lfoVal * modAmount;
        delaySamples = constrain(delaySamples, 1.0f, (float)CHORUS_BUFFER_SIZE - 2.0f);

        float readPosF = (float)writePos_ - delaySamples;
        if (readPosF < 0.0f) readPosF += (float)CHORUS_BUFFER_SIZE;

        int readPos0 = (int)readPosF;
        int readPos1 = (readPos0 + 1) % CHORUS_BUFFER_SIZE;
        float frac = readPosF - (float)readPos0;

        return (buf[readPos0] * (1.0f - frac) + buf[readPos1] * frac) / 32000.0f;
    };

    float delayedL = readDelay(bufferL_, lfoL);
    float delayedR = readDelay(bufferR_, lfoR);
    
    writePos_ = (writePos_ + 1) % CHORUS_BUFFER_SIZE;
    
    left = left * (1.0f - mix_) + delayedL * mix_;
    right = right * (1.0f - mix_) + delayedR * mix_;
}

// ============================================================================
// EFFECTS CHAIN
// ============================================================================

EffectsChain::EffectsChain() :
    satEnabled_(false),
    chorusEnabled_(false),
    delayEnabled_(false)
{
}

void EffectsChain::setEnabled(bool sat, bool chr, bool dly) {
    satEnabled_ = sat;
    chorusEnabled_ = chr;
    delayEnabled_ = dly;
}

float EffectsChain::process(float input) {
    float left = input;
    float right = input;
    processStereo(left, right);
    return (left + right) * 0.5f;
}

void EffectsChain::processStereo(float& left, float& right) {
    if (satEnabled_) {
        saturation.processStereo(left, right);
    }
    
    if (chorusEnabled_) {
        chorus.processStereo(left, right);
    }
    
    if (delayEnabled_) {
        delay.processStereo(left, right);
    }
}
