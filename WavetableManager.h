#ifndef WAVETABLE_MANAGER_H
#define WAVETABLE_MANAGER_H

#include "Config.h"
#include "SDManager.h"

constexpr uint8_t MAX_CACHED_TABLES = 4;
constexpr uint16_t CUSTOM_TABLE_SIZE = 2048;

struct CachedWavetable {
    char name[32];
    float* data;
    bool inUse;
    uint32_t lastUsed;
};

class WavetableManager {
public:
    WavetableManager();
    ~WavetableManager();

    bool init(SDManager* sd);

    // Load a wavetable from SD into cache
    float* getWavetable(const char* name, uint16_t& size);

    // List available files in /waves
    void scanWaves();
    int getWaveFileCount() const { return waveFileCount_; }
    const char* getWaveFileName(int index) const;

private:
    SDManager* sd_;
    CachedWavetable cache_[MAX_CACHED_TABLES];

    char waveFiles_[20][32]; // Max 20 files for browsing
    int waveFileCount_;

    int findInCache(const char* name);
    int findEmptySlot();
    int findLRUSlot();
};

#endif
