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
    lastSHPhase_(0)
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
    // Apply phase offset
    uint32_t effectivePhase = phase_ + (uint32_t)(phaseOffset_ * (float)0xFFFFFFFF);
    
    // Normalized phase 0-1
    float t = effectivePhase * PHASE_TO_FLOAT;
    
    float value = 0.0f;
    
    switch (waveform_) {
        case LFOWaveform::SINE:
            value = Wavetables::readSine(effectivePhase);
            break;
            
        case LFOWaveform::TRIANGLE:
            value = Wavetables::readTriangle(effectivePhase, 2);
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
            
        case LFOWaveform::SAMPLE_HOLD:
            // Update S&H on phase wrap
            // For block processing, we check if multiple wraps happened
            // but for simplicity we just check if the new phase is less than the old phase
            // when we add the full increment
            if (phase_ + phaseIncrement_ * samples < phase_) {
                 sampleHoldValue_ = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            }
            value = sampleHoldValue_;
            break;
            
        default:
            value = 0.0f;
    }
    
    phase_ += phaseIncrement_ * samples;
    lastValue_ = value;
    
    return value * depth_;
}

float LFO::processUnipolar() {
    return (process() + depth_) * 0.5f;
}
