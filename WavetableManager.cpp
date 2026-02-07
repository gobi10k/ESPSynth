#include "WavetableManager.h"
#include <Arduino.h>
#include <string.h>

WavetableManager::WavetableManager() : sd_(nullptr), waveFileCount_(0) {
    for (int i = 0; i < MAX_CACHED_TABLES; i++) {
        cache_[i].data = nullptr;
        cache_[i].inUse = false;
        cache_[i].lastUsed = 0;
        cache_[i].name[0] = '\0';
    }
}

WavetableManager::~WavetableManager() {
    for (int i = 0; i < MAX_CACHED_TABLES; i++) {
        if (cache_[i].data) {
            free(cache_[i].data);
        }
    }
}

bool WavetableManager::init(SDManager* sd) {
    sd_ = sd;
    if (!sd_ || !sd_->isAvailable()) return false;

    if (!sd_->exists("/waves")) {
        sd_->mkdir("/waves");
    }

    scanWaves();
    return true;
}

void WavetableManager::scanWaves() {
    if (!sd_ || !sd_->isAvailable()) return;

    waveFileCount_ = 0;
    File root = SD.open("/waves");
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file && waveFileCount_ < 20) {
        if (!file.isDirectory()) {
            strncpy(waveFiles_[waveFileCount_], file.name(), 31);
            waveFiles_[waveFileCount_][31] = '\0';
            waveFileCount_++;
        }
        file = root.openNextFile();
    }
}

const char* WavetableManager::getWaveFileName(int index) const {
    if (index >= 0 && index < waveFileCount_) {
        return waveFiles_[index];
    }
    return nullptr;
}

float* WavetableManager::getWavetable(const char* name, uint16_t& size) {
    size = CUSTOM_TABLE_SIZE;

    int idx = findInCache(name);
    if (idx >= 0) {
        cache_[idx].lastUsed = millis();
        return cache_[idx].data;
    }

    // Load from SD
    char path[64];
    snprintf(path, sizeof(path), "/waves/%s", name);

    File file = SD.open(path);
    if (!file) {
        Serial.printf("[WT] Failed to open: %s\n", path);
        return nullptr;
    }

    size_t fileSize = file.size();
    size_t samplesInFile = fileSize / sizeof(float);

    idx = findEmptySlot();
    if (idx < 0) {
        idx = findLRUSlot();
    }

    if (!cache_[idx].data) {
        cache_[idx].data = (float*)malloc(CUSTOM_TABLE_SIZE * sizeof(float));
    }

    if (!cache_[idx].data) {
        Serial.println("[WT] Malloc failed");
        file.close();
        return nullptr;
    }

    // Read and potentially resize (simple truncation or zero pad for now)
    size_t toRead = min((size_t)CUSTOM_TABLE_SIZE, samplesInFile);
    file.read((uint8_t*)cache_[idx].data, toRead * sizeof(float));

    // Zero pad if file is smaller
    if (toRead < CUSTOM_TABLE_SIZE) {
        for (size_t i = toRead; i < CUSTOM_TABLE_SIZE; i++) {
            cache_[idx].data[i] = 0.0f;
        }
    }

    file.close();

    strncpy(cache_[idx].name, name, 31);
    cache_[idx].inUse = true;
    cache_[idx].lastUsed = millis();

    Serial.printf("[WT] Loaded %s into slot %d\n", name, idx);
    return cache_[idx].data;
}

int WavetableManager::findInCache(const char* name) {
    for (int i = 0; i < MAX_CACHED_TABLES; i++) {
        if (cache_[i].inUse && strcmp(cache_[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int WavetableManager::findEmptySlot() {
    for (int i = 0; i < MAX_CACHED_TABLES; i++) {
        if (!cache_[i].inUse) return i;
    }
    return -1;
}

int WavetableManager::findLRUSlot() {
    int oldestIdx = 0;
    uint32_t oldestTime = 0xFFFFFFFF;
    for (int i = 0; i < MAX_CACHED_TABLES; i++) {
        if (cache_[i].lastUsed < oldestTime) {
            oldestTime = cache_[i].lastUsed;
            oldestIdx = i;
        }
    }
    return oldestIdx;
}
