#ifndef ENVELOPE_H
#define ENVELOPE_H

#include "Config.h"

enum class EnvelopeStage : uint8_t {
    IDLE = 0,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE
};

class Envelope {
public:
    Envelope();
    
    // Times in seconds, sustain is 0-1 level
    void setAttack(float seconds);
    void setDecay(float seconds);
    void setSustain(float level);
    void setRelease(float seconds);
    void setADSR(float a, float d, float s, float r);
    
    float getAttack() const { return attackTime_; }
    float getDecay() const { return decayTime_; }
    float getSustain() const { return sustainLevel_; }
    float getRelease() const { return releaseTime_; }
    
    void gate(bool on);
    void trigger();
    
    inline float process() {
        switch (stage_) {
            case EnvelopeStage::IDLE:
                currentValue_ = 0.0f;
                break;

            case EnvelopeStage::ATTACK: {
                float overshootTarget = 1.02f;
                currentValue_ += attackCoef_ * (overshootTarget - currentValue_);
                if (currentValue_ >= 1.0f) {
                    currentValue_ = 1.0f;
                    stage_ = EnvelopeStage::DECAY;
                    targetValue_ = sustainLevel_;
                }
                break;
            }

            case EnvelopeStage::DECAY: {
                float overshootTarget = max(sustainLevel_ * 0.95f - 0.01f, -0.001f);
                currentValue_ += decayCoef_ * (overshootTarget - currentValue_);
                if (currentValue_ <= sustainLevel_ + 0.001f) {
                    currentValue_ = sustainLevel_;
                    stage_ = EnvelopeStage::SUSTAIN;
                }
                break;
            }

            case EnvelopeStage::SUSTAIN:
                currentValue_ = sustainLevel_;
                break;

            case EnvelopeStage::RELEASE: {
                float overshootTarget = -0.02f;
                currentValue_ += releaseCoef_ * (overshootTarget - currentValue_);
                if (currentValue_ <= 0.0f) {
                    currentValue_ = 0.0f;
                    stage_ = EnvelopeStage::IDLE;
                }
                break;
            }
        }
        return currentValue_;
    }

    float getValue() const { return currentValue_; }
    EnvelopeStage getStage() const { return stage_; }
    bool isActive() const { return stage_ != EnvelopeStage::IDLE; }

private:
    float calcCoef(float timeSeconds) const;
    
    float attackTime_;
    float decayTime_;
    float sustainLevel_;
    float releaseTime_;
    
    float attackCoef_;
    float decayCoef_;
    float releaseCoef_;
    
    float currentValue_;
    float targetValue_;
    EnvelopeStage stage_;
    bool gateOn_;
};

#endif
