#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Config.h"
#include <U8g2lib.h>

class SynthEngine;

enum class DisplayPage : uint8_t {
    MAIN = 0,
    OSCILLATORS,
    FILTER,
    ENVELOPES,
    LFO,
    MOD_MATRIX,
    ARP,
    EFFECTS,
    MIXER,
    SD_BROWSER,
    NUM_PAGES
};

class DisplayManager {
public:
    DisplayManager();
    
    bool init(SynthEngine* engine);
    void start();
    void stop();
    void update();
    void setRefreshRate(uint8_t fps);

    void setLoading(bool loading) { loadingFlag_ = loading; }
    void setPage(DisplayPage page) { currentPage_ = page; }
    void nextPage();
    void prevPage();

    void nextItem();
    void prevItem();
    void adjustValue(int delta);

    DisplayPage getCurrentPage() const { return currentPage_; }
    bool isWaveMode() const { return sdWaveMode_; }
    int getSDFileIndex() const { return sdFileIndex_; }
    int getSDSlot() const { return sdSlot_; }

private:
    void drawUI();
    void drawMainPage();
    void drawOscPage();
    void drawFilterPage();
    void drawEnvPage();
    void drawLFOPage();
    void drawModPage();
    void drawArpPage();
    void drawEffectsPage();
    void drawMixerPage();
    void drawSDPage();

    static void displayTaskWrapper(void* param);
    
    U8G2_SH1106_128X64_NONAME_1_HW_I2C display_;
    SynthEngine* engine_;
    TaskHandle_t displayTaskHandle_;
    uint16_t refreshDelayMs_;
    volatile bool running_;
    DisplayPage currentPage_;
    int8_t selectedItem_;
    int8_t modSlotIndex_;
    int8_t sdFileIndex_;
    int8_t sdSlot_;
    bool sdWaveMode_; // true = waves, false = presets
    bool loadingFlag_;
};

#endif
