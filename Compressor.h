#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "Config.h"

/**
 * Simple feed-forward compressor
 * Soft-knee design for musical response
 */
class Compressor {
public:
    Compressor();
    
    void setThreshold(float dB);    // -60 to 0 dB
    void setRatio(float ratio);     // 1:1 to 20:1
    void setAttack(float ms);       // 0.1 to 100 ms
    void setRelease(float ms);      // 10 to 1000 ms
    void setMakeupGain(float dB);   // 0 to 24 dB
    void setKnee(float dB);         // Soft knee width
    
    float getThreshold() const { return thresholdDb_; }
    float getRatio() const { return ratio_; }
    float getGainReduction() const { return gainReductionDb_; }
    
    float process(float input);
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

private:
    float thresholdDb_;
    float thresholdLin_;
    float ratio_;
    float attackCoef_;
    float releaseCoef_;
    float makeupGain_;
    float kneeWidth_;
    
    float envelope_;
    float gainReductionDb_;
    bool enabled_;
};

#endif
