#ifndef SYNTHESIS_H
#define SYNTHESIS_H

#include "Config.h"
#include "Oscillator.h"

// ============================================================================
// RING MODULATOR
// ============================================================================

class RingMod {
public:
    RingMod();
    
    void setFrequency(float hz);
    void setMix(float mix);  // 0 = dry, 1 = ring mod only
    
    float getFrequency() const { return frequency_; }
    float getMix() const { return mix_; }
    
    float process(float input);

private:
    float frequency_;
    float mix_;
    uint32_t phase_;
    uint32_t phaseIncrement_;
};

// ============================================================================
// AMPLITUDE MODULATION (AM)
// ============================================================================

class AMod {
public:
    AMod();
    
    void setFrequency(float hz);
    void setDepth(float depth);  // 0-1
    
    float getFrequency() const { return frequency_; }
    float getDepth() const { return depth_; }
    
    float process(float input);

private:
    float frequency_;
    float depth_;
    uint32_t phase_;
    uint32_t phaseIncrement_;
};

// ============================================================================
// FM PAIR (Carrier + Modulator)
// ============================================================================

class FMPair {
public:
    FMPair();
    
    void setCarrierFreq(float hz);
    void setModFreq(float hz);
    void setModIndex(float index);    // FM depth (0-10 typical)
    void setRatio(float ratio);       // Mod/Carrier ratio
    void setFeedback(float fb);       // Modulator self-feedback
    
    float getCarrierFreq() const { return carrierFreq_; }
    float getModIndex() const { return modIndex_; }
    float getRatio() const { return ratio_; }
    
    // Set both frequencies maintaining ratio
    void setFrequency(float hz);
    
    float process();

private:
    float carrierFreq_;
    float modFreq_;
    float modIndex_;
    float ratio_;
    float feedback_;
    float lastModOut_;
    
    uint32_t carrierPhase_;
    uint32_t modPhase_;
    uint32_t carrierInc_;
    uint32_t modInc_;
};

// ============================================================================
// OSCILLATOR SYNC
// ============================================================================

class SyncOsc {
public:
    SyncOsc();
    
    void setMasterFreq(float hz);
    void setSlaveFreq(float hz);
    void setSlaveWaveform(Waveform wf);
    void setMix(float mix);  // 0 = master, 1 = slave
    
    float getMasterFreq() const { return masterFreq_; }
    float getSlaveFreq() const { return slaveFreq_; }
    
    float process();

private:
    float masterFreq_;
    float slaveFreq_;
    Waveform slaveWaveform_;
    float mix_;
    
    uint32_t masterPhase_;
    uint32_t slavePhase_;
    uint32_t masterInc_;
    uint32_t slaveInc_;
    uint32_t lastMasterPhase_;
    int tableIndex_;
};

// ============================================================================
// WAVEFOLDER
// ============================================================================

class Wavefolder {
public:
    Wavefolder();
    
    void setFolds(float folds);  // 1-8 number of folds
    void setSymmetry(float sym); // -1 to +1
    void setMix(float mix);
    
    float getFolds() const { return folds_; }
    
    float process(float input);

private:
    float folds_;
    float symmetry_;
    float mix_;
};

#endif
