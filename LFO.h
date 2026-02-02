#ifndef LFO_H
#define LFO_H

#include "Config.h"

enum class LFOWaveform : uint8_t {
    SINE = 0,
    TRIANGLE,
    SAW_UP,
    SAW_DOWN,
    SQUARE,
    SAMPLE_HOLD,
    NUM_WAVEFORMS
};

class LFO {
public:
    LFO();
    
    void setFrequency(float hz);
    void setWaveform(LFOWaveform wf);
    void setPhaseOffset(float offset);  // 0-1
    void setDepth(float depth);         // Output multiplier
    
    float getFrequency() const { return frequency_; }
    LFOWaveform getWaveform() const { return waveform_; }
    float getDepth() const { return depth_; }
    
    // Returns bipolar output (-1 to +1) * depth
    float process();
    float process(int samples);
    
    // Returns unipolar output (0 to 1) * depth
    float processUnipolar();
    
    // Get raw value without depth applied
    float getRawValue() const { return lastValue_; }
    
    void reset();
    void sync();  // Reset phase to 0

private:
    float frequency_;
    LFOWaveform waveform_;
    float phaseOffset_;
    float depth_;
    
    uint32_t phase_;
    uint32_t phaseIncrement_;
    float lastValue_;
    float sampleHoldValue_;
    uint32_t lastSHPhase_;
};

#endif
