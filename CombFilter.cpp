#include "CombFilter.h"
#include "MathUtils.h"
#include <math.h>
#include <string.h>

const char* COMB_MODE_NAMES[] = {"FB", "FF", "AP", "K-S"};

CombFilter::CombFilter() :
    writePos_(0),
    delaySamples_(100.0f),
    feedback_(0.7f),
    damping_(0.3f),
    mode_(CombMode::FEEDBACK),
    mix_(0.5f),
    detuneRatio_(1.0f),
    dampState_(0.0f),
    allpassCoef_(0.5f),
    exciteLevel_(0.0f),
    exciteCounter_(0),
    noiseState_(44444),
    lastDelayed_(0.0f)
{
    reset();
}

void CombFilter::setPitch(float hz) {
    hz = constrain(hz, 40.0f, 5000.0f);  // 40Hz min (was 20Hz)
    delaySamples_ = SAMPLE_RATE / hz;
    delaySamples_ *= detuneRatio_;
    if (delaySamples_ >= COMB_BUFFER_SIZE - 1) {
        delaySamples_ = COMB_BUFFER_SIZE - 2;
    }
}

void CombFilter::setDelaySamples(float samples) {
    delaySamples_ = constrain(samples, 1.0f, COMB_BUFFER_SIZE - 2.0f);
}

void CombFilter::setFeedback(float fb) {
    feedback_ = constrain(fb, -0.99f, 0.99f);
}

void CombFilter::setDamping(float damp) {
    damping_ = constrain(damp, 0.0f, 0.99f);
}

void CombFilter::setMode(CombMode mode) {
    mode_ = mode;
}

void CombFilter::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

void CombFilter::setDetune(float cents) {
    detuneRatio_ = powf(2.0f, cents / 1200.0f);
}

float CombFilter::getPitch() const {
    return SAMPLE_RATE / delaySamples_;
}

void CombFilter::excite(float amplitude) {
    exciteLevel_ = amplitude;
    exciteCounter_ = (int)delaySamples_;
}

void CombFilter::reset() {
    memset(buffer_, 0, sizeof(buffer_));
    writePos_ = 0;
    dampState_ = 0.0f;
    exciteLevel_ = 0.0f;
    exciteCounter_ = 0;
    lastDelayed_ = 0.0f;
}

float CombFilter::process(float input) {
    if (isnan(input) || isinf(input)) return 0.0f;

    // Handle excitation
    if (exciteCounter_ > 0) {
        float noise = fastRandFloat(noiseState_) * exciteLevel_;
        input += noise;
        exciteCounter_--;
    }
    
    // Read with linear interpolation
    float readPos = (float)writePos_ - delaySamples_;
    if (readPos < 0.0f) readPos += (float)COMB_BUFFER_SIZE;
    
    int readIdx0 = (int)readPos;
    int readIdx1 = readIdx0 + 1;
    if (readIdx1 >= COMB_BUFFER_SIZE) readIdx1 -= COMB_BUFFER_SIZE;

    float frac = readPos - (float)readIdx0;
    
    // Convert int16 to float
    float s0 = buffer_[readIdx0] * 0.00003125f; // 1/32000
    float s1 = buffer_[readIdx1] * 0.00003125f;
    float delayed = s0 + frac * (s1 - s0);
    
    // Damping filter
    if (damping_ > 0.0f) {
        dampState_ = dampState_ * damping_ + delayed * (1.0f - damping_);
        delayed = dampState_;
    }
    
    float output = 0.0f;
    float toWrite = 0.0f;
    
    switch (mode_) {
        case CombMode::FEEDBACK:
            output = input + feedback_ * delayed;
            toWrite = output;
            break;
            
        case CombMode::FEEDFORWARD:
            output = input + feedback_ * delayed;
            toWrite = input;
            break;
            
        case CombMode::ALLPASS:
            output = delayed + allpassCoef_ * (input - delayed);
            toWrite = input + feedback_ * delayed;
            break;
            
        case CombMode::KARPLUS_STRONG:
            output = input + feedback_ * delayed;
            float averaged = (delayed + lastDelayed_) * 0.5f;
            lastDelayed_ = delayed;
            toWrite = input + feedback_ * averaged;
            break;
    }
    
    // Soft clip and write
    if (toWrite > 1.0f) toWrite = 1.0f;
    if (toWrite < -1.0f) toWrite = -1.0f;
    buffer_[writePos_] = (int16_t)(toWrite * 32000.0f);
    writePos_ = (writePos_ + 1) % COMB_BUFFER_SIZE;
    
    return input * (1.0f - mix_) + output * mix_;
}
