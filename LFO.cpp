#include "LFO.h"
#include "Wavetables.h"
#include <math.h>

LFO::LFO() :
    frequency_(1.0f),
    waveform_(LFOWaveform::SINE),
    phaseOffset_(0.0f),
    depth_(1.0f),
    phase_(0),
    phaseIncrement_(0),
    lastValue_(0.0f),
    sampleHoldValue_(0.0f),
    lastSHPhase_(0),
    randState_(0xBEEF1234u + (uint32_t)(uintptr_t)this)
{
    setFrequency(1.0f);
}

void LFO::setFrequency(float hz) {
    frequency_ = constrain(hz, 0.01f, 50.0f);
    phaseIncrement_ = (uint32_t)(frequency_ * PHASE_INCREMENT_MULTIPLIER);
}

void LFO::setWaveform(LFOWaveform wf) {
    waveform_ = wf;
}

void LFO::setPhaseOffset(float offset) {
    phaseOffset_ = fmodf(offset, 1.0f);
}

void LFO::setDepth(float depth) {
    depth_ = constrain(depth, 0.0f, 1.0f);
}

void LFO::reset() {
    phase_ = 0;
    lastValue_ = 0.0f;
}

void LFO::sync() {
    phase_ = 0;
}

float LFO::process() {
    return process(1);
}

float LFO::process(int samples) {
    float value = 0.0f;
    const uint32_t phaseOffsetFixed = (uint32_t)(phaseOffset_ * (float)0xFFFFFFFF);

    for (int i = 0; i < samples; i++) {
        uint32_t ep = phase_ + phaseOffsetFixed;
        float t = ep * PHASE_TO_FLOAT;
        switch (waveform_) {
            case LFOWaveform::SINE:
                value = Wavetables::readSine(ep);
                break;
            case LFOWaveform::TRIANGLE:
                value = Wavetables::readTriangle(ep, 2);
                break;
            case LFOWaveform::SAW_UP:
                value = 2.0f * t - 1.0f;
                break;
            case LFOWaveform::SAW_DOWN:
                value = 1.0f - 2.0f * t;
                break;
            case LFOWaveform::SQUARE:
                value = (t < 0.5f) ? 1.0f : -1.0f;
                break;
            case LFOWaveform::SAMPLE_HOLD: {
                uint32_t next = phase_ + phaseIncrement_;
                if (next < phase_) {
                    randState_ ^= randState_ << 13;
                    randState_ ^= randState_ >> 17;
                    randState_ ^= randState_ << 5;
                    sampleHoldValue_ = (float)(int32_t)randState_ / (float)INT32_MAX;
                }
                value = sampleHoldValue_;
                break;
            }
            default:
                value = 0.0f;
        }
        phase_ += phaseIncrement_;
    }

    lastValue_ = value;
    return value * depth_;
}

float LFO::processUnipolar() {
    return (process() + depth_) * 0.5f;
}
