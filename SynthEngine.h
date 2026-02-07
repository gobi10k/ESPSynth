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
#include <driver/i2s_std.h>
#include <atomic>

constexpr uint8_t NUM_VOICES = 4;

class AudioProfiler {
public:
    void startSample() {
        startTime_ = micros();
    }

    void endSample() {
        uint32_t elapsed = micros() - startTime_;
        totalTime_ += elapsed;
        sampleCount_++;
        if (elapsed > maxTime_) maxTime_ = elapsed;
        if (elapsed < minTime_) minTime_ = elapsed;

        // Update rolling average
        float blockDurationUs = (DMA_BUFFER_SAMPLES * 1000000.0f) / SAMPLE_RATE;
        lastCpuLoad_ = (elapsed / blockDurationUs) * 100.0f;
    }

    float getCPUPercent() const { return lastCpuLoad_; }

    void printStats() {
        if (sampleCount_ == 0) {
            Serial.println("Profiler: No data collected");
            return;
        }
        uint32_t avgTime = totalTime_ / sampleCount_;
        float blockDurationUs = (DMA_BUFFER_SAMPLES * 1000000.0f) / SAMPLE_RATE;
        float cpuPercent = (avgTime / blockDurationUs) * 100.0f;

        Serial.printf("Audio CPU: %.1f%% (avg: %luus, max: %luus, min: %luus) over %lu blocks\n",
                      cpuPercent, avgTime, maxTime_, minTime_, sampleCount_);

        if (maxTime_ > blockDurationUs) {
            Serial.printf("WARNING: Task Overflow! Max time %luus exceeds block duration %.0fus\n",
                          maxTime_, blockDurationUs);
        }
        reset();
    }

    void reset() {
        totalTime_ = 0;
        sampleCount_ = 0;
        maxTime_ = 0;
        minTime_ = 0xFFFFFFFF;
    }

private:
    uint32_t startTime_;
    uint32_t totalTime_ = 0;
    uint32_t sampleCount_ = 0;
    uint32_t maxTime_ = 0;
    uint32_t minTime_ = 0xFFFFFFFF;
    float lastCpuLoad_ = 0.0f;
};

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
    void setSustainPedal(bool active);
    
    // Pitch bend (-8192 to +8191)
    void setPitchBend(int16_t value);
    void setPitchBendRange(uint8_t semitones);
    
    // Global oscillator params (applied to all voices)
    void setOscWaveform(int osc, Waveform wf);
    void setOscDetune(int osc, float cents);
    void setOscCoarse(int osc, int8_t semitones);
    void setOscMix(float mix);
    void setPulseWidth(int osc, float pw);
    void setOscMorph(int osc, float morph);
    
    Waveform getOscWaveform(int osc) const { return pendingParams_.oscWaveforms[osc]; }
    float getOscDetune(int osc) const { return pendingParams_.oscDetune[osc]; }
    float getOscMix() const { return pendingParams_.oscMix; }
    float getOscMorph(int osc) const { return pendingParams_.morph[osc]; }

    // Synthesis mode
    void setSynthMode(VoiceSynthMode mode);
    void setFMAmount(float amount);
    VoiceSynthMode getSynthMode() const { return pendingParams_.synthMode; }
    float getFMAmount() const { return pendingParams_.fmAmount; }
    
    // Global filter
    void setFilterCutoff(float hz);
    void setFilterResonance(float r);
    void setFilterMode(FilterMode mode);
    void setFilterEnvAmount(float amount);
    void setFilterEnvVelocity(float amount);
    void setFilterKeyTracking(float amount);
    void setFilterType(VoiceFilterType type);
    
    float getFilterCutoff() const { return pendingParams_.filterCutoff; }
    float getFilterResonance() const { return pendingParams_.filterReso; }
    FilterMode getFilterMode() const { return pendingParams_.filterMode; }
    VoiceFilterType getFilterType() const { return pendingParams_.filterType; }
    float getFilterEnvAmount() const { return pendingParams_.filterEnvAmount; }
    float getFilterEnvVelocity() const { return pendingParams_.filterEnvVelocity; }
    float getFilterKeyTracking() const { return pendingParams_.filterKeyTracking; }
    
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
    
    // Euclidean Sequencer
    EuclideanSequencer& getEuclidean() { return euclideanSeq_; }
    void setEuclideanEnabled(bool en) { euclideanEnabled_ = en; }
    bool isEuclideanEnabled() const { return euclideanEnabled_; }

    // Global Resonator (single instance for master chain)
    ResonatorBank& getResonator() { return resonator_; }
    void setResonatorEnabled(bool en) { resonatorEnabled_ = en; }
    bool isResonatorEnabled() const { return resonatorEnabled_; }
    
    // Global Comb (single instance for master chain)
    CombFilter& getComb() { return comb_; }
    void setCombEnabled(bool en) { combEnabled_ = en; }
    bool isCombEnabled() const { return combEnabled_; }
    
    // Granular exciter
    GranularExciter& getGranular() { return granular_; }
    void setGranularEnabled(bool en) { granularEnabled_ = en; }
    bool isGranularEnabled() const { return granularEnabled_; }
    void setGranularMix(float mix);
    
    // Effects
    EffectsChain& getEffects() { return effects_; }
    Compressor& getCompressor() { return compressor_; }
    AMod& getAMod() { return aMod_; }
    Wavefolder& getWavefolder() { return wavefolder_; }
    FDNReverb& getReverb() { return reverb_; }
    
    // Master
    void setMasterVolume(float vol);
    float getMasterVolume() const { return masterVolume_.getTarget(); }
    void setGlobalPan(float pan) { globalPan_ = pan; }
    float getGlobalPan() const { return globalPan_; }
    
    // Profiler
    void printCPUStats() { profiler_.printStats(); }
    float getCPUPercent() { return profiler_.getCPUPercent(); }
    uint32_t getBlockCount() const { return blockCounter_; }

    // Voice info
    uint8_t getActiveVoiceCount() const;
    Voice& getVoice(int i) { return voices_[i]; }

    static float midiToFreq(uint8_t note) {
        return 440.0f * powf(2.0f, (note - 69) / 12.0f);
    }

    void setEuclideanNote(uint8_t note) { euclideanNote_ = note; }

private:
    void processBlock();
    void noteOnInternal(uint8_t note, uint8_t velocity);
    int allocateVoice(uint8_t note);
    int findVoiceForNote(uint8_t note);
    static void audioTaskWrapper(void* param);
    
    // Voices
    Voice voices_[NUM_VOICES];
    
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
    
    // Euclidean
    EuclideanSequencer euclideanSeq_;
    bool euclideanEnabled_;
    uint8_t euclideanNote_;

    // Global Resonator and Comb (single instances, post-mix)
    ResonatorBank resonator_;
    CombFilter comb_;
    bool resonatorEnabled_;
    bool combEnabled_;
    
    // Granular exciter
    GranularExciter granular_;
    float granularMix_;
    bool granularEnabled_;
    
    // Effects
    EffectsChain effects_;
    Compressor compressor_;
    AMod aMod_;
    Wavefolder wavefolder_;
    FDNReverb reverb_;
    
    // Master
    SmoothedValue masterVolume_;
    float globalPan_;
    
    // Runtime
    float currentVelocity_;
    volatile bool running_;
    bool sustainPedalActive_ = false;
    bool notesSustained_[128] = {false};
    volatile uint32_t blockCounter_ = 0;
    float voicePanL_[NUM_VOICES];
    float voicePanR_[NUM_VOICES];
    TaskHandle_t audioTaskHandle_;
    i2s_chan_handle_t tx_handle_;
    AudioProfiler profiler_;
    int16_t blockBuffer_[DMA_BUFFER_SAMPLES * 2];

    // Thread-safe parameters
    GlobalVoiceParams activeParams_;
    GlobalVoiceParams pendingParams_;
    std::atomic<bool> paramsDirty_{false};
};

#endif
