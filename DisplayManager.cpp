#include "DisplayManager.h"
#include "SynthEngine.h"
#include <Wire.h>

extern const char* WAVEFORM_NAMES[];
extern const char* FILTER_MODE_NAMES[];

DisplayManager::DisplayManager() : 
    display_(U8G2_R0, U8X8_PIN_NONE),
    engine_(nullptr),
    displayTaskHandle_(nullptr),
    refreshDelayMs_(50),
    running_(false),
    currentPage_(DisplayPage::MAIN)
{
}

bool DisplayManager::init(SynthEngine* engine) {
    engine_ = engine;
    
    Serial.printf("[Display] Initializing on SDA:%d, SCL:%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);
    delay(100); // Give OLED time to power up
    
    if (!display_.begin()) {
        Serial.println("[Display] SH1106 Init failed");
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
        8192,  // Increased stack size
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

void DisplayManager::nextPage() {
    int page = (int)currentPage_ + 1;
    if (page >= (int)DisplayPage::NUM_PAGES) page = 0;
    currentPage_ = (DisplayPage)page;
}

void DisplayManager::prevPage() {
    int page = (int)currentPage_ - 1;
    if (page < 0) page = (int)DisplayPage::NUM_PAGES - 1;
    currentPage_ = (DisplayPage)page;
}

void DisplayManager::drawUI() {
    if (!engine_) return;
    
    switch (currentPage_) {
        case DisplayPage::MAIN: drawMainPage(); break;
        case DisplayPage::OSCILLATORS: drawOscPage(); break;
        case DisplayPage::FILTER: drawFilterPage(); break;
        case DisplayPage::EFFECTS: drawEffectsPage(); break;
        default: drawMainPage();
    }
}

void DisplayManager::drawMainPage() {
    char buf[32];
    int y = 7;
    
    display_.drawStr(0, y, "MAIN");
    snprintf(buf, sizeof(buf), "V:%d/%d", engine_->getActiveVoiceCount(), NUM_VOICES);
    display_.drawStr(60, y, buf);

    snprintf(buf, sizeof(buf), "%.1f%%", engine_->getCPUPercent());
    display_.drawStr(100, y, buf);
    
    y += 2;
    display_.drawLine(0, y, 127, y);
    y += 12;
    
    // Visual indicators
    float ampEnv = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (engine_->getVoice(i).isActive()) {
            ampEnv = max(ampEnv, engine_->getVoice(i).getLevel());
        }
    }

    display_.drawStr(0, y, "LEVEL:");
    display_.drawFrame(40, y-7, 80, 8);
    display_.drawBox(41, y-6, (int)(ampEnv * 78), 6);
    y += 14;

    display_.drawStr(0, y, "LFO:");
    float lfo = (engine_->getLFO(0).getRawValue() + 1.0f) * 0.5f;
    display_.drawFrame(40, y-7, 80, 8);
    display_.drawBox(41, y-6, (int)(lfo * 78), 6);
    y += 14;

    // Effects footer
    EffectsChain& fx = engine_->getEffects();
    display_.drawStr(0, 63, fx.isSatEnabled() ? "[S]" : " S ");
    display_.drawStr(25, 63, fx.isChorusEnabled() ? "[C]" : " C ");
    display_.drawStr(50, 63, fx.isDelayEnabled() ? "[D]" : " D ");
    display_.drawStr(75, 63, engine_->getReverb().isEnabled() ? "[R]" : " R ");
    display_.drawStr(100, 63, engine_->getCompressor().isEnabled() ? "[K]" : " K ");
}

void DisplayManager::drawOscPage() {
    char buf[32];
    display_.drawStr(0, 7, "OSCILLATORS");
    display_.drawLine(0, 9, 127, 9);
    
    Waveform wf1 = engine_->getOscWaveform(0);
    Waveform wf2 = engine_->getOscWaveform(1);
    
    display_.drawStr(0, 22, "OSC1:");
    display_.drawStr(40, 22, WAVEFORM_NAMES[(int)wf1]);
    
    display_.drawStr(0, 34, "OSC2:");
    display_.drawStr(40, 34, WAVEFORM_NAMES[(int)wf2]);

    snprintf(buf, sizeof(buf), "MIX: %.0f%%", engine_->getOscMix() * 100.0f);
    display_.drawStr(0, 46, buf);

    snprintf(buf, sizeof(buf), "DETUNE: %.1f", engine_->getOscDetune(1));
    display_.drawStr(0, 58, buf);
}

void DisplayManager::drawFilterPage() {
    char buf[32];
    display_.drawStr(0, 7, "FILTER");
    display_.drawLine(0, 9, 127, 9);

    const char* typeName = (engine_->getFilterType() == VoiceFilterType::SVF) ? "SVF" : "LADDER";
    snprintf(buf, sizeof(buf), "TYPE: %s", typeName);
    display_.drawStr(0, 22, buf);

    snprintf(buf, sizeof(buf), "MODE: %s", FILTER_MODE_NAMES[(int)engine_->getFilterMode()]);
    display_.drawStr(0, 34, buf);

    float cutoff = engine_->getFilterCutoff();
    snprintf(buf, sizeof(buf), "CUTOFF: %.0f Hz", cutoff);
    display_.drawStr(0, 42, buf);

    snprintf(buf, sizeof(buf), "RESO: %.0f%%", engine_->getFilterResonance() * 100.0f);
    display_.drawStr(0, 50, buf);

    snprintf(buf, sizeof(buf), "KBD:%.0f%% VEL:%.0f%%",
             engine_->getFilterKeyTracking() * 100.0f,
             engine_->getFilterEnvVelocity() * 100.0f);
    display_.drawStr(0, 58, buf);
}

void DisplayManager::drawEffectsPage() {
    display_.drawStr(0, 7, "EFFECTS");
    display_.drawLine(0, 9, 127, 9);
    
    EffectsChain& fx = engine_->getEffects();

    display_.drawStr(0, 22, "SAT:");
    display_.drawStr(40, 22, fx.isSatEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 34, "CHORUS:");
    display_.drawStr(50, 34, fx.isChorusEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 46, "DELAY:");
    display_.drawStr(45, 46, fx.isDelayEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 58, "REVERB:");
    display_.drawStr(50, 58, engine_->getReverb().isEnabled() ? "ON" : "OFF");
}

void DisplayManager::displayTaskWrapper(void* param) {
    DisplayManager* mgr = static_cast<DisplayManager*>(param);
    while (mgr->running_) {
        mgr->update();
        vTaskDelay(pdMS_TO_TICKS(mgr->refreshDelayMs_));
    }
    vTaskDelete(NULL);
}
