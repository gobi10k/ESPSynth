#include "Effects.h"
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
    memset(buffer_, 0, sizeof(buffer_));
    writePos_ = 0;
}

float Delay::process(float input) {
    // Read from delay line (convert int16 to float)
    uint16_t readPos = (writePos_ + MAX_DELAY_SAMPLES - delaySamples_) % MAX_DELAY_SAMPLES;
    float delayed = buffer_[readPos] / 32767.0f;
    
    // Write input + feedback to delay line (convert float to int16)
    float toWrite = input + delayed * feedback_;
    toWrite = constrain(toWrite, -1.0f, 1.0f);
    buffer_[writePos_] = (int16_t)(toWrite * 32767.0f);
    writePos_ = (writePos_ + 1) % MAX_DELAY_SAMPLES;
    
    // Mix dry/wet
    return input * (1.0f - mix_) + delayed * mix_;
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
    float driven = input * drive_;
    float saturated = 0.0f;
    
    switch (type_) {
        case SaturationType::SOFT:
            // Soft saturation using tanh
            saturated = tanhf(driven);
            break;
            
        case SaturationType::HARD:
            // Hard clipping
            saturated = constrain(driven, -1.0f, 1.0f);
            break;
            
        case SaturationType::FOLDBACK:
            // Wavefolding
            while (driven > 1.0f || driven < -1.0f) {
                if (driven > 1.0f) driven = 2.0f - driven;
                if (driven < -1.0f) driven = -2.0f - driven;
            }
            saturated = driven;
            break;
            
        case SaturationType::BITCRUSH: {
            // Reduce bit depth
            float bits = 16.0f / drive_;  // More drive = fewer bits
            float scale = powf(2.0f, bits);
            saturated = roundf(driven * scale) / scale;
            saturated = constrain(saturated, -1.0f, 1.0f);
            break;
        }
            
        default:
            saturated = driven;
    }
    
    // Compensate gain
    saturated /= max(1.0f, drive_ * 0.5f);
    
    return input * (1.0f - mix_) + saturated * mix_;
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
    memset(buffer_, 0, sizeof(buffer_));
    writePos_ = 0;
}

float Chorus::process(float input) {
    // Safety check
    if (isnan(input) || isinf(input)) input = 0.0f;
    
    // Clamp input
    if (input > 1.0f) input = 1.0f;
    if (input < -1.0f) input = -1.0f;
    
    // Write to buffer (convert to int16)
    buffer_[writePos_] = (int16_t)(input * 32000.0f);
    
    // LFO for modulated delay time
    float lfoValue = sinf(lfoPhase_ * PHASE_TO_FLOAT * 2.0f * M_PI);
    lfoPhase_ += lfoIncrement_;
    
    // Delay time: 5-12ms modulated by LFO
    float baseDelay = 0.007f * SAMPLE_RATE;  // 7ms center
    float modAmount = depth_ * 0.003f * SAMPLE_RATE;  // Up to 3ms mod
    float delaySamples = baseDelay + lfoValue * modAmount;
    
    // Safety clamp
    if (delaySamples < 1.0f) delaySamples = 1.0f;
    if (delaySamples > CHORUS_BUFFER_SIZE - 2) delaySamples = CHORUS_BUFFER_SIZE - 2;
    
    // Read with linear interpolation
    float readPosF = (float)writePos_ - delaySamples;
    if (readPosF < 0) readPosF += CHORUS_BUFFER_SIZE;
    
    int readPos0 = (int)readPosF;
    if (readPos0 < 0) readPos0 = 0;
    if (readPos0 >= CHORUS_BUFFER_SIZE) readPos0 = CHORUS_BUFFER_SIZE - 1;
    
    int readPos1 = (readPos0 + 1) % CHORUS_BUFFER_SIZE;
    float frac = readPosF - floorf(readPosF);
    
    // Convert int16 to float and interpolate
    float s0 = buffer_[readPos0] / 32000.0f;
    float s1 = buffer_[readPos1] / 32000.0f;
    float delayed = s0 * (1.0f - frac) + s1 * frac;
    
    writePos_ = (writePos_ + 1) % CHORUS_BUFFER_SIZE;
    
    return input * (1.0f - mix_) + delayed * mix_;
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
    float signal = input;
    
    if (satEnabled_) {
        signal = saturation.process(signal);
    }
    
    if (chorusEnabled_) {
        signal = chorus.process(signal);
    }
    
    if (delayEnabled_) {
        signal = delay.process(signal);
    }
    
    return signal;
}
