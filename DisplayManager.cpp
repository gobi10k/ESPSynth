#include "DisplayManager.h"
#include "AudioEngine.h"
#include <Wire.h>

extern const char* WAVEFORM_NAMES[];
extern const char* FILTER_MODE_NAMES[];

DisplayManager::DisplayManager() : 
    display_(U8G2_R0, U8X8_PIN_NONE),
    engine_(nullptr),
    displayTaskHandle_(nullptr),
    refreshDelayMs_(50),
    running_(false)
{
}

bool DisplayManager::init(AudioEngine* engine) {
    engine_ = engine;
    
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);
    
    if (!display_.begin()) {
        Serial.println("[Display] Init failed");
        return false;
    }
    
    display_.setFont(u8g2_font_5x7_tf);
    display_.setContrast(200);
    
    Serial.println("[Display] Ready");
    return true;
}

void DisplayManager::start() {
    if (running_) return;
    running_ = true;
    
    xTaskCreatePinnedToCore(
        displayTaskWrapper,
        "DisplayTask",
        4096,
        this,
        1,
        &displayTaskHandle_,
        0
    );
}

void DisplayManager::stop() {
    running_ = false;
    if (displayTaskHandle_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(displayTaskHandle_);
        displayTaskHandle_ = nullptr;
    }
}

void DisplayManager::update() {
    display_.firstPage();
    do {
        drawUI();
    } while (display_.nextPage());
}

void DisplayManager::setRefreshRate(uint8_t fps) {
    fps = constrain(fps, 1, 60);
    refreshDelayMs_ = 1000 / fps;
}

void DisplayManager::drawUI() {
    if (!engine_) return;
    
    char buf[24];
    int y = 7;
    
    // Row 1: Title + Master Vol
    display_.drawStr(0, y, "ESP32 SYNTH");
    snprintf(buf, sizeof(buf), "V%.0f%%", engine_->getMasterVolume() * 100);
    display_.drawStr(100, y, buf);
    
    y += 2;
    display_.drawLine(0, y, 127, y);
    y += 8;
    
    // Row 2: Oscillators
    const Oscillator& osc1 = engine_->getOscillator(0);
    float freq = osc1.getFrequency();
    if (freq >= 1000.0f) {
        snprintf(buf, sizeof(buf), "OSC:%s %.1fk", 
                 WAVEFORM_NAMES[(int)osc1.getWaveform()], freq / 1000.0f);
    } else {
        snprintf(buf, sizeof(buf), "OSC:%s %.0fHz", 
                 WAVEFORM_NAMES[(int)osc1.getWaveform()], freq);
    }
    display_.drawStr(0, y, buf);
    y += 9;
    
    // Row 3: Filter
    Filter& flt = engine_->getFilter();
    float cutoff = flt.getCutoff();
    if (cutoff >= 1000.0f) {
        snprintf(buf, sizeof(buf), "FLT:%s %.1fk R%.0f%%", 
                 FILTER_MODE_NAMES[(int)flt.getMode()],
                 cutoff / 1000.0f,
                 flt.getResonance() * 100);
    } else {
        snprintf(buf, sizeof(buf), "FLT:%s %.0f R%.0f%%", 
                 FILTER_MODE_NAMES[(int)flt.getMode()],
                 cutoff,
                 flt.getResonance() * 100);
    }
    display_.drawStr(0, y, buf);
    y += 9;
    
    // Row 4: Envelopes (visual bars)
    display_.drawStr(0, y, "A:");
    int ampBar = (int)(engine_->getAmpEnvelope().getValue() * 25);
    display_.drawFrame(12, y - 6, 27, 7);
    if (ampBar > 0) display_.drawBox(13, y - 5, ampBar, 5);
    
    display_.drawStr(45, y, "F:");
    int fltBar = (int)(engine_->getFilterEnvelope().getValue() * 25);
    display_.drawFrame(55, y - 6, 27, 7);
    if (fltBar > 0) display_.drawBox(56, y - 5, fltBar, 5);
    
    // LFO indicator
    float lfoVal = engine_->getLFO(0).getRawValue();
    display_.drawStr(90, y, "L:");
    int lfoX = 100 + (int)(lfoVal * 10);
    display_.drawBox(lfoX, y - 5, 3, 5);
    y += 9;
    
    // Row 5: Effects status
    EffectsChain& fx = engine_->getEffects();
    snprintf(buf, sizeof(buf), "FX:");
    display_.drawStr(0, y, buf);
    
    int fxX = 18;
    // These would need proper getters, for now show placeholders
    display_.drawStr(fxX, y, "[S]");
    fxX += 20;
    display_.drawStr(fxX, y, "[C]");
    fxX += 20;
    display_.drawStr(fxX, y, "[D]");
}

void DisplayManager::displayTaskWrapper(void* param) {
    DisplayManager* mgr = static_cast<DisplayManager*>(param);
    while (mgr->running_) {
        mgr->update();
        vTaskDelay(pdMS_TO_TICKS(mgr->refreshDelayMs_));
    }
    vTaskDelete(NULL);
}
