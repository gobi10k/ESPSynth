#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include "Config.h"
#include "Voice.h"
#include "LFO.h"
#include "Effects.h"
#include "Synthesis.h"
#include "ModMatrix.h"
#include "Arpeggiator.h"
#include "Compressor.h"
#include "SmoothedValue.h"
#include "Reverb.h"
#include "Granular.h"
#include "Euclidean.h"
#include "Resonator.h"
#include "CombFilter.h"

constexpr uint8_t NUM_VOICES = 2;

class SynthEngine {
public:
    SynthEngine();
    
    bool init();
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Voice management
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void allNotesOff();
    
    // Pitch bend (-8192 to +8191)
    void setPitchBend(int16_t value);
    void setPitchBendRange(uint8_t semitones);
    
    // Global oscillator params (applied to all voices)
    void setOscWaveform(int osc, Waveform wf);
    void setOscDetune(int osc, float cents);
    void setOscMix(float mix);
    void setPulseWidth(int osc, float pw);
    
    Waveform getOscWaveform(int osc) const { return oscWaveforms_[osc]; }
    float getOscDetune(int osc) const { return oscDetune_[osc]; }
    
    // Global filter
    void setFilterCutoff(float hz);
    void setFilterResonance(float r);
    void setFilterMode(FilterMode mode);
    void setFilterEnvAmount(float amount);
    void setFilterType(VoiceFilterType type);
    
    float getFilterCutoff() const { return filterCutoff_; }
    float getFilterResonance() const { return filterReso_; }
    FilterMode getFilterMode() const { return filterMode_; }
    VoiceFilterType getFilterType() const { return filterType_; }
    
    // Envelopes
    void setAmpADSR(float a, float d, float s, float r);
    void setFilterADSR(float a, float d, float s, float r);
    
    // Glide
    void setGlideTime(float ms);
    
    // LFOs
    LFO& getLFO(int index);
    static constexpr int NUM_LFOS = 2;
    
    // Mod matrix
    ModMatrix& getModMatrix() { return modMatrix_; }
    
    // Arpeggiator
    Arpeggiator& getArp() { return arp_; }
    
    // Global Resonator (single instance for master chain)
    ResonatorBank& getResonator() { return resonator_; }
    
    // Global Comb (single instance for master chain)
    CombFilter& getComb() { return comb_; }
    
    // Granular exciter
    GranularExciter& getGranular() { return granular_; }
    
    // Effects
    EffectsChain& getEffects() { return effects_; }
    Compressor& getCompressor() { return compressor_; }
    AMod& getAMod() { return aMod_; }
    Wavefolder& getWavefolder() { return wavefolder_; }
    FDNReverb& getReverb() { return reverb_; }
    
    // Master
    void setMasterVolume(float vol);
    float getMasterVolume() const { return masterVolume_.getTarget(); }
    
    // Voice info
    uint8_t getActiveVoiceCount() const;
    Voice& getVoice(int i) { return voices_[i]; }

private:
    void processBlock();
    int allocateVoice(uint8_t note);
    int findVoiceForNote(uint8_t note);
    static void audioTaskWrapper(void* param);
    
    // Voices
    Voice voices_[NUM_VOICES];
    
    // Global params (copied to voices)
    Waveform oscWaveforms_[2];
    float oscDetune_[2];
    float oscMix_;
    float pulseWidth_[2];
    
    float filterCutoff_;
    float filterReso_;
    FilterMode filterMode_;
    VoiceFilterType filterType_;
    float filterEnvAmount_;
    
    float ampA_, ampD_, ampS_, ampR_;
    float fltA_, fltD_, fltS_, fltR_;
    float glideTime_;
    
    // Pitch bend
    SmoothedValue pitchBend_;
    uint8_t pitchBendRange_;
    
    // LFOs
    LFO lfos_[NUM_LFOS];
    
    // Mod matrix
    ModMatrix modMatrix_;
    
    // Arpeggiator
    Arpeggiator arp_;
    bool lastArpGate_;
    
    // Global Resonator and Comb (single instances, post-mix)
    ResonatorBank resonator_;
    CombFilter comb_;
    bool resonatorEnabled_;
    bool combEnabled_;
    
    // Granular exciter
    GranularExciter granular_;
    float granularMix_;
    
    // Effects
    EffectsChain effects_;
    Compressor compressor_;
    AMod aMod_;
    Wavefolder wavefolder_;
    FDNReverb reverb_;
    
    // Master
    SmoothedValue masterVolume_;
    
    // Runtime
    volatile bool running_;
    TaskHandle_t audioTaskHandle_;
};

#endif
