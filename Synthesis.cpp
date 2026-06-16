#include "Synthesis.h"
#include "Wavetables.h"
#include <math.h>

// ============================================================================
// RING MODULATOR
// ============================================================================

RingMod::RingMod() :
    frequency_(440.0f),
    mix_(1.0f),
    phase_(0),
    phaseIncrement_(0)
{
    setFrequency(440.0f);
}

void RingMod::setFrequency(float hz) {
    frequency_ = constrain(hz, 1.0f, 5000.0f);
    phaseIncrement_ = (uint32_t)(frequency_ * PHASE_INCREMENT_MULTIPLIER);
}

void RingMod::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

float RingMod::process(float input) {
    // Ring mod = input * carrier
    float carrier = Wavetables::readSine(phase_);
    phase_ += phaseIncrement_;
    
    float ringMod = input * carrier;
    return input * (1.0f - mix_) + ringMod * mix_;
}

// ============================================================================
// AMPLITUDE MODULATION
// ============================================================================

AMod::AMod() :
    frequency_(5.0f),
    depth_(0.5f),
    phase_(0),
    phaseIncrement_(0)
{
    setFrequency(5.0f);
}

void AMod::setFrequency(float hz) {
    frequency_ = constrain(hz, 0.1f, 1000.0f);
    phaseIncrement_ = (uint32_t)(frequency_ * PHASE_INCREMENT_MULTIPLIER);
}

void AMod::setDepth(float depth) {
    depth_ = constrain(depth, 0.0f, 1.0f);
}

float AMod::process(float input) {
    // AM = input * (1 - depth + depth * (carrier + 1) / 2)
    // This keeps the signal unipolar modulated
    float carrier = Wavetables::readSine(phase_);
    phase_ += phaseIncrement_;
    
    float modulator = 1.0f - depth_ + depth_ * (carrier + 1.0f) * 0.5f;
    return input * modulator;
}

// ============================================================================
// FM PAIR
// ============================================================================

FMPair::FMPair() :
    carrierFreq_(440.0f),
    modFreq_(440.0f),
    modIndex_(1.0f),
    ratio_(1.0f),
    feedback_(0.0f),
    lastModOut_(0.0f),
    carrierPhase_(0),
    modPhase_(0),
    carrierInc_(0),
    modInc_(0)
{
    setFrequency(440.0f);
}

void FMPair::setCarrierFreq(float hz) {
    carrierFreq_ = constrain(hz, 20.0f, 10000.0f);
    carrierInc_ = (uint32_t)(carrierFreq_ * PHASE_INCREMENT_MULTIPLIER);
}

void FMPair::setModFreq(float hz) {
    modFreq_ = constrain(hz, 0.5f, 10000.0f);
    modInc_ = (uint32_t)(modFreq_ * PHASE_INCREMENT_MULTIPLIER);
    ratio_ = modFreq_ / carrierFreq_;
}

void FMPair::setModIndex(float index) {
    modIndex_ = constrain(index, 0.0f, 20.0f);
}

void FMPair::setRatio(float ratio) {
    ratio_ = constrain(ratio, 0.125f, 16.0f);
    modFreq_ = carrierFreq_ * ratio_;
    modInc_ = (uint32_t)(modFreq_ * PHASE_INCREMENT_MULTIPLIER);
}

void FMPair::setFeedback(float fb) {
    feedback_ = constrain(fb, 0.0f, 1.0f);
}

void FMPair::setFrequency(float hz) {
    carrierFreq_ = constrain(hz, 20.0f, 10000.0f);
    carrierInc_ = (uint32_t)(carrierFreq_ * PHASE_INCREMENT_MULTIPLIER);
    modFreq_ = carrierFreq_ * ratio_;
    modInc_ = (uint32_t)(modFreq_ * PHASE_INCREMENT_MULTIPLIER);
}

float FMPair::process() {
    // Modulator with feedback
    // Explicitly handle sign and wrap-around safely
    int32_t fbOffset = (int32_t)(lastModOut_ * feedback_ * (float)modInc_ * 2.0f);
    uint32_t modPhaseEff = (uint32_t)((int64_t)modPhase_ + fbOffset);
    float modOut = Wavetables::readSine(modPhaseEff);
    lastModOut_ = modOut;
    modPhase_ += modInc_;
    
    // FM: modulator affects carrier phase
    int32_t fmOffset = (int32_t)(modOut * modIndex_ * (float)carrierInc_);
    uint32_t carrierPhaseEff = (uint32_t)((int64_t)carrierPhase_ + fmOffset);
    float carrierOut = Wavetables::readSine(carrierPhaseEff);
    carrierPhase_ += carrierInc_;
    
    return carrierOut;
}

// ============================================================================
// OSCILLATOR SYNC
// ============================================================================

SyncOsc::SyncOsc() :
    masterFreq_(110.0f),
    slaveFreq_(220.0f),
    slaveWaveform_(Waveform::SAW),
    mix_(1.0f),
    masterPhase_(0),
    slavePhase_(0),
    masterInc_(0),
    slaveInc_(0),
    lastMasterPhase_(0),
    tableIndex_(2)
{
    setMasterFreq(110.0f);
    setSlaveFreq(220.0f);
}

void SyncOsc::setMasterFreq(float hz) {
    masterFreq_ = constrain(hz, 20.0f, 5000.0f);
    masterInc_ = (uint32_t)(masterFreq_ * PHASE_INCREMENT_MULTIPLIER);
}

void SyncOsc::setSlaveFreq(float hz) {
    slaveFreq_ = constrain(hz, 20.0f, 10000.0f);
    slaveInc_ = (uint32_t)(slaveFreq_ * PHASE_INCREMENT_MULTIPLIER);
    tableIndex_ = Wavetables::tableIndexForFreq(slaveFreq_);
}

void SyncOsc::setSlaveWaveform(Waveform wf) {
    slaveWaveform_ = wf;
}

void SyncOsc::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

float SyncOsc::process() {
    // Check for master phase wrap (sync point)
    if (masterPhase_ < lastMasterPhase_) {
        slavePhase_ = 0;  // Hard sync
    }
    lastMasterPhase_ = masterPhase_;
    
    // Generate master (always sine for simplicity)
    float master = Wavetables::readSine(masterPhase_);
    masterPhase_ += masterInc_;
    
    // Generate slave
    float slave = 0.0f;
    switch (slaveWaveform_) {
        case Waveform::SINE:
            slave = Wavetables::readSine(slavePhase_);
            break;
        case Waveform::SAW:
            slave = Wavetables::readSaw(slavePhase_, tableIndex_);
            break;
        case Waveform::SQUARE:
            slave = Wavetables::readSquare(slavePhase_, tableIndex_);
            break;
        case Waveform::TRIANGLE:
            slave = Wavetables::readTriangle(slavePhase_, tableIndex_);
            break;
        default:
            slave = Wavetables::readSaw(slavePhase_, tableIndex_);
    }
    slavePhase_ += slaveInc_;
    
    return master * (1.0f - mix_) + slave * mix_;
}

// ============================================================================
// WAVEFOLDER
// ============================================================================

Wavefolder::Wavefolder() :
    folds_(1.0f),
    symmetry_(0.0f),
    mix_(1.0f)
{
}

void Wavefolder::setFolds(float folds) {
    folds_ = constrain(folds, 1.0f, 8.0f);
}

void Wavefolder::setSymmetry(float sym) {
    symmetry_ = constrain(sym, -1.0f, 1.0f);
}

void Wavefolder::setMix(float mix) {
    mix_ = constrain(mix, 0.0f, 1.0f);
}

float Wavefolder::process(float input) {
    // Apply asymmetric gain
    float signal = input + symmetry_ * 0.5f;
    signal *= folds_;
    
    // Folding using triangle wave reflection
    // Maps any value to [-1, 1] through folding
    float folded = signal;
    
    // Efficient folding using fmod and reflection
    folded = fmodf(folded + 1.0f, 4.0f) - 1.0f;
    if (folded > 1.0f) folded = 2.0f - folded;
    if (folded < -1.0f) folded = -2.0f - folded;
    
    return input * (1.0f - mix_) + folded * mix_;
}
