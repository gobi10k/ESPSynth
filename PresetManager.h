#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include "Config.h"
#include "Wavetables.h"
#include "Filter.h"
#include "Arpeggiator.h"
#include "Voice.h"
#include "SDManager.h"

constexpr uint8_t NUM_PRESETS = 16;
constexpr uint32_t PRESET_MAGIC = 0x53594E54;  // "SYNT"

// Packed preset data structure
struct __attribute__((packed)) PresetData {
    uint32_t magic;
    uint8_t version;
    char name[15];
    
    // Oscillators (7 bytes)
    uint8_t osc1Wave;
    uint8_t osc2Wave;
    int8_t osc1Detune;
    int8_t osc2Detune;
    uint8_t oscMix;
    uint8_t osc1PW;
    uint8_t osc2PW;
    
    // Filter (5 bytes)
    uint16_t filterCutoff;
    uint8_t filterReso;
    uint8_t filterMode;
    int8_t filterEnvAmount;
    
    // Amp envelope (7 bytes)
    uint16_t ampAttack;
    uint16_t ampDecay;
    uint8_t ampSustain;
    uint16_t ampRelease;
    
    // Filter envelope (7 bytes)
    uint16_t filterAttack;
    uint16_t filterDecay;
    uint8_t filterSustain;
    uint16_t filterRelease;
    
    // LFOs (8 bytes)
    uint8_t lfo1Wave;
    uint16_t lfo1Rate;
    uint8_t lfo1Depth;
    uint8_t lfo2Wave;
    uint16_t lfo2Rate;
    uint8_t lfo2Depth;
    
    // Arpeggiator (6 bytes)
    uint8_t arpMode;
    uint8_t arpDivision;
    uint16_t arpTempo;
    uint8_t arpGate;
    uint8_t arpOctaves;
    
    // Effects Core (9 bytes)
    uint8_t effectFlags;  // bit0=sat, bit1=chorus, bit2=delay, bit3=reverb, bit4=comp, bit5=res, bit6=comb, bit7=gran
    uint8_t satDrive;
    uint8_t satType;
    uint8_t chorusRate;
    uint8_t chorusDepth;
    uint16_t delayTime;
    uint8_t delayFeedback;
    uint8_t delayMix;
    
    // Extra Voice Params (10 bytes)
    uint8_t filterType;
    uint8_t synthMode;
    uint8_t fmAmount;
    uint8_t filterEnvVel;
    uint8_t filterKeyTrack;
    int8_t globalPan;
    uint16_t glideTime;
    int8_t osc1Coarse;
    int8_t osc2Coarse;

    // Reverb Params (5 bytes)
    uint8_t revDecay;
    uint8_t revSize;
    uint8_t revDamp;
    uint8_t revMix;
    uint8_t revPre;

    // Compressor Params (5 bytes)
    int8_t compThresh;
    uint8_t compRatio;
    uint8_t compAttack;
    uint8_t compRelease;
    uint8_t compMakeup;

    // Resonator Params (6 bytes)
    uint8_t resProfile;
    uint8_t resReso;
    uint8_t resDamp;
    uint8_t resBright;
    uint8_t resMix;
    uint8_t resReserved;

    // Comb Params (5 bytes)
    uint16_t combPitch;
    uint8_t combFB;
    uint8_t combDamp;
    uint8_t combMix;

    // Granular Params (5 bytes)
    uint8_t granDensity;
    uint16_t granDuration;
    uint8_t granMix;
    uint8_t granSource;

    // New parameters (4 bytes)
    uint8_t osc1SupersawDetune;
    uint8_t osc2SupersawDetune;
    uint8_t legato;
    uint8_t reserved1;

    uint8_t reserved[15];
    
    uint32_t checksum;
};

static_assert(sizeof(PresetData) == 128, "PresetData must be exactly 128 bytes");

class PresetManager {
public:
    PresetManager();
    
    void begin();
    
    bool savePreset(uint8_t slot, const PresetData& preset);
    bool loadPreset(uint8_t slot, PresetData& preset);
    bool deletePreset(uint8_t slot);
    
    bool isSlotUsed(uint8_t slot);
    const char* getPresetName(uint8_t slot);
    
    // Factory presets
    void loadFactoryPresets();
    static PresetData getInitPreset();

    // SD Card storage
    bool savePresetToSD(const char* filename, const PresetData& preset, SDManager& sd);
    bool loadPresetFromSD(const char* filename, PresetData& preset, SDManager& sd);

private:
    uint32_t calculateChecksum(const PresetData& preset);
    
    PresetData presetCache_[NUM_PRESETS];
    bool slotUsed_[NUM_PRESETS];
};

#endif
