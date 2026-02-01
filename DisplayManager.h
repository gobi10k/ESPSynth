#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Config.h"
#include <U8g2lib.h>

class AudioEngine;

class DisplayManager {
public:
    DisplayManager();
    
    bool init(AudioEngine* engine);
    void start();
    void stop();
    void update();
    void setRefreshRate(uint8_t fps);

private:
    void drawUI();
    static void displayTaskWrapper(void* param);
    
    U8G2_SH1106_128X64_NONAME_1_HW_I2C display_;
    AudioEngine* engine_;
    TaskHandle_t displayTaskHandle_;
    uint16_t refreshDelayMs_;
    volatile bool running_;
};

#endif
