#include "MoogFilter.h"
#include "MathUtils.h"
#include <math.h>
#include <string.h>

const char* LADDER_MODE_NAMES[] = {"LP24", "LP18", "LP12", "LP6", "BP12", "HP24"};

// ============================================================================
// BASIC MOOG FILTER
// ============================================================================

MoogFilter::MoogFilter() :
    cutoffHz_(1000.0f),
    resonance_(0.0f),
    drive_(1.0f),
    cutoffMod_(0.0f),
    gMod_(0.0f),
    invGMod_(1.0f)
{
    reset();
    setCutoff(cutoffHz_);
}

void MoogFilter::setCutoff(float hz) {
    cutoffHz_ = constrain(hz, 20.0f, 20000.0f);
    updateCoefficients(0.0f);
}

void MoogFilter::updateCoefficients(float modHz) {
    float modCutoff = cutoffHz_ + modHz;
    modCutoff = constrain(modCutoff, 20.0f, 20000.0f);
    float fc = min(modCutoff, SAMPLE_RATE * 0.45f);
    gMod_ = 1.0f - fastExp(TWO_PI_INV_SR * fc);
    invGMod_ = 1.0f - gMod_;
}

void MoogFilter::setResonance(float res) {
    resonance_ = constrain(res, 0.0f, 1.0f);
}

void MoogFilter::setDrive(float drive) {
    drive_ = constrain(drive, 1.0f, 4.0f);
}

void MoogFilter::reset() {
    memset(stage_, 0, sizeof(stage_));
    memset(delay_, 0, sizeof(delay_));
}

float MoogFilter::process(float input) {
    if (isnan(input) || isinf(input)) input = 0.0f;
    
    float feedback = resonance_ * 4.0f;
    input *= drive_;
    
    // Feedback with saturation
    input -= fastPolyClip(delay_[3] * feedback);
    
    // Four cascaded one-pole lowpass filters - unrolled
    float in = fastPolyClip(input);
    stage_[0] = gMod_ * in + invGMod_ * delay_[0];
    delay_[0] = stage_[0];
    
    in = fastPolyClip(stage_[0]);
    stage_[1] = gMod_ * in + invGMod_ * delay_[1];
    delay_[1] = stage_[1];
    
    in = fastPolyClip(stage_[1]);
    stage_[2] = gMod_ * in + invGMod_ * delay_[2];
    delay_[2] = stage_[2];

    in = fastPolyClip(stage_[2]);
    stage_[3] = gMod_ * in + invGMod_ * delay_[3];
    delay_[3] = stage_[3];

    float output = stage_[3] * (1.0f + feedback * 0.3f);
    output = fastPolyClip(output);

    if (isnan(output) || isinf(output)) {
        reset();
        return 0.0f;
    }

    return output / drive_;
}

// ============================================================================
// LADDER FILTER WITH MODES
// ============================================================================

LadderFilter::LadderFilter() :
    cutoffHz_(1000.0f),
    resonance_(0.0f),
    mode_(LadderMode::LP24),
    drive_(1.0f),
    keyTracking_(0.0f),
    keyFreq_(440.0f),
    cutoffMod_(0.0f),
    gMod_(0.0f),
    invGMod_(1.0f)
{
    reset();
    setCutoff(cutoffHz_);
    updateTaps();
}

void LadderFilter::setCutoff(float hz) {
    cutoffHz_ = constrain(hz, 20.0f, 20000.0f);
    updateCoefficients(0.0f);
}

void LadderFilter::updateCoefficients(float modHz) {
    // Apply key tracking
    float keyOffset = 0.0f;
    if (keyTracking_ > 0.0f) {
        keyOffset = (keyFreq_ - 440.0f) * keyTracking_ * 2.0f;
    }

    float modCutoff = cutoffHz_ + modHz + keyOffset;
    modCutoff = constrain(modCutoff, 20.0f, 18000.0f);
    float fc = min(modCutoff, SAMPLE_RATE * 0.4f);
    gMod_ = 1.0f - fastExp(TWO_PI_INV_SR * fc);
    invGMod_ = 1.0f - gMod_;
}

void LadderFilter::setResonance(float res) {
    resonance_ = constrain(res, 0.0f, 1.0f);
}

void LadderFilter::setMode(LadderMode mode) {
    mode_ = mode;
    updateTaps();
}

void LadderFilter::setDrive(float drive) {
    drive_ = constrain(drive, 1.0f, 4.0f);
}

void LadderFilter::setKeyTracking(float amount) {
    keyTracking_ = constrain(amount, 0.0f, 1.0f);
}

void LadderFilter::updateTaps() {
    // Tap coefficients for mixing filter stages
    // taps[0] = input, taps[1-4] = stages 1-4
    memset(taps_, 0, sizeof(taps_));
    
    switch (mode_) {
        case LadderMode::LP24:
            taps_[4] = 1.0f;  // Only 4th stage output
            break;
            
        case LadderMode::LP18:
            taps_[3] = 1.0f;  // 3rd stage
            break;
            
        case LadderMode::LP12:
            taps_[2] = 1.0f;  // 2nd stage
            break;
            
        case LadderMode::LP6:
            taps_[1] = 1.0f;  // 1st stage
            break;
            
        case LadderMode::BP12:
            // Bandpass: input - 2*LP12
            taps_[0] = 1.0f;
            taps_[2] = -2.0f;
            taps_[4] = 1.0f;
            break;
            
        case LadderMode::HP24:
            // Highpass: input - LP24
            taps_[0] = 1.0f;
            taps_[1] = -4.0f;
            taps_[2] = 6.0f;
            taps_[3] = -4.0f;
            taps_[4] = 1.0f;
            break;
            
        default:
            taps_[4] = 1.0f;
    }
}

void LadderFilter::reset() {
    memset(stage_, 0, sizeof(stage_));
    memset(delay_, 0, sizeof(delay_));
}

float LadderFilter::process(float input) {
    if (isnan(input) || isinf(input)) input = 0.0f;
    
    float feedback = resonance_ * 3.5f;
    float in = input * drive_;
    
    in = fastPolyClip(in * 0.5f) * 2.0f;
    
    float fb = delay_[3] * feedback;
    fb = fastPolyClip(fb);
    in -= fb;
    
    // Four stages - unrolled
    float stageIn = in;
    stageIn = fastPolyClip(stageIn);
    stage_[0] = gMod_ * stageIn + invGMod_ * delay_[0];
    delay_[0] = stage_[0];

    stageIn = stage_[0];
    stageIn = fastPolyClip(stageIn);
    stage_[1] = gMod_ * stageIn + invGMod_ * delay_[1];
    delay_[1] = stage_[1];

    stageIn = stage_[1];
    stageIn = fastPolyClip(stageIn);
    stage_[2] = gMod_ * stageIn + invGMod_ * delay_[2];
    delay_[2] = stage_[2];

    stageIn = stage_[2];
    stageIn = fastPolyClip(stageIn);
    stage_[3] = gMod_ * stageIn + invGMod_ * delay_[3];
    delay_[3] = stage_[3];

    float output = taps_[0] * in + taps_[1] * stage_[0] + taps_[2] * stage_[1] + taps_[3] * stage_[2] + taps_[4] * stage_[3];
    output *= (1.0f + feedback * 0.15f);
    
    output = fastPolyClip(output);
    
    if (isnan(output) || isinf(output)) {
        reset();
        return 0.0f;
    }

    return output / drive_;
}
