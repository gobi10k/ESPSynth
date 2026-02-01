#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "Config.h"
#include "Oscillator.h"
#include "Envelope.h"
#include "Filter.h"
#include "LFO.h"
#include "Effects.h"
#include "ModMatrix.h"
#include "Synthesis.h"

// Synthesis modes
enum class SynthMode : uint8_t {
    STANDARD = 0,   // Normal oscillator mixing
    FM,             // FM synthesis
    SYNC,           // Oscillator sync
    RING,           // Ring modulation
    NUM_MODES
};

extern const char* SYNTH_MODE_NAMES[];

class AudioEngine {
public:
    AudioEngine();
    
    bool init();
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Synthesis mode
    void setSynthMode(SynthMode mode);
    SynthMode getSynthMode() const { return synthMode_; }
    
    // Oscillators
    Oscillator& getOscillator(int index);
    const Oscillator& getOscillator(int index) const;
    
    // Envelopes
    Envelope& getAmpEnvelope() { return ampEnv_; }
    Envelope& getFilterEnvelope() { return filterEnv_; }
    
    // Filter
    Filter& getFilter() { return filter_; }
    
    // LFOs
    LFO& getLFO(int index);
    static constexpr int NUM_LFOS = 2;
    
    // Effects
    EffectsChain& getEffects() { return effects_; }
    
    // Synthesis modules
    FMPair& getFM() { return fmPair_; }
    SyncOsc& getSync() { return syncOsc_; }
    RingMod& getRingMod() { return ringMod_; }
    AMod& getAMod() { return aMod_; }
    Wavefolder& getWavefolder() { return wavefolder_; }
    
    // Modulation
    ModMatrix& getModMatrix() { return modMatrix_; }
    
    // Master controls
    void setMasterVolume(float vol);
    float getMasterVolume() const { return masterVolume_; }
    
    void setFilterEnvAmount(float amount);
    float getFilterEnvAmount() const { return filterEnvAmount_; }
    
    // Note control
    void noteOn(float frequency, float velocity = 1.0f);
    void noteOff();

private:
    void processBlock();
    void applyModulation();
    static void audioTaskWrapper(void* param);
    
    // Synthesis mode
    SynthMode synthMode_;
    
    // Oscillators
    Oscillator oscillators_[MAX_OSCILLATORS];
    
    // Envelopes
    Envelope ampEnv_;
    Envelope filterEnv_;
    float filterEnvAmount_;
    
    // Filter
    Filter filter_;
    
    // LFOs
    LFO lfos_[NUM_LFOS];
    
    // Effects
    EffectsChain effects_;
    
    // Synthesis modules
    FMPair fmPair_;
    SyncOsc syncOsc_;
    RingMod ringMod_;
    AMod aMod_;
    Wavefolder wavefolder_;
    
    // Modulation
    ModMatrix modMatrix_;
    
    // State
    float masterVolume_;
    float currentVelocity_;
    float baseFrequency_;
    volatile bool running_;
    TaskHandle_t audioTaskHandle_;
};

#endif
