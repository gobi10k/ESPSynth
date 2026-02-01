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
    running_(false)
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

void DisplayManager::drawUI() {
    if (!engine_) return;
    
    char buf[32];
    int y = 7;
    
    // Row 1: Title + Voice Count
    display_.drawStr(0, y, "ESP32 SYNTH");
    snprintf(buf, sizeof(buf), "V:%d/%d", engine_->getActiveVoiceCount(), NUM_VOICES);
    display_.drawStr(60, y, buf);

    // Heap info for stability monitoring
    snprintf(buf, sizeof(buf), "%dK", ESP.getFreeHeap() / 1024);
    display_.drawStr(105, y, buf);
    
    y += 2;
    display_.drawLine(0, y, 127, y);
    y += 8;
    
    // Row 2: Oscillators
    Waveform wf1 = engine_->getOscWaveform(0);
    Waveform wf2 = engine_->getOscWaveform(1);
    snprintf(buf, sizeof(buf), "OSC: %s / %s", WAVEFORM_NAMES[(int)wf1], WAVEFORM_NAMES[(int)wf2]);
    display_.drawStr(0, y, buf);
    y += 9;
    
    // Row 3: Filter
    float cutoff = engine_->getFilterCutoff();
    if (cutoff >= 1000.0f) {
        snprintf(buf, sizeof(buf), "FLT:%s %.1fk R%.0f%%", 
                 FILTER_MODE_NAMES[(int)engine_->getFilterMode()],
                 cutoff / 1000.0f,
                 engine_->getFilterResonance() * 100);
    } else {
        snprintf(buf, sizeof(buf), "FLT:%s %.0f R%.0f%%", 
                 FILTER_MODE_NAMES[(int)engine_->getFilterMode()],
                 cutoff,
                 engine_->getFilterResonance() * 100);
    }
    display_.drawStr(0, y, buf);
    y += 9;
    
    // Row 4: Envelopes (visual bars from first active voice)
    float ampEnv = 0.0f;
    float filterEnv = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (engine_->getVoice(i).isActive()) {
            ampEnv = engine_->getVoice(i).getLevel();
            filterEnv = engine_->getVoice(i).getFilterEnvValue();
            break;
        }
    }

    display_.drawStr(0, y, "A:");
    int ampBar = (int)(ampEnv * 25);
    display_.drawFrame(12, y - 6, 27, 7);
    if (ampBar > 0) display_.drawBox(13, y - 5, ampBar, 5);
    
    display_.drawStr(45, y, "F:");
    int fltBar = (int)(filterEnv * 25);
    display_.drawFrame(55, y - 6, 27, 7);
    if (fltBar > 0) display_.drawBox(56, y - 5, fltBar, 5);
    
    // LFO indicator
    float lfoVal = engine_->getLFO(0).getRawValue();
    display_.drawStr(90, y, "L:");
    int lfoX = 100 + (int)(lfoVal * 10);
    display_.drawBox(lfoX, y - 5, 3, 5);
    y += 9;
    
    // Row 5: Effects status
    snprintf(buf, sizeof(buf), "FX:");
    display_.drawStr(0, y, buf);
    
    int fxX = 18;
    EffectsChain& fx = engine_->getEffects();
    if (fx.isSatEnabled()) display_.drawStr(fxX, y, "[S]");
    fxX += 20;
    if (fx.isChorusEnabled()) display_.drawStr(fxX, y, "[C]");
    fxX += 20;
    if (fx.isDelayEnabled()) display_.drawStr(fxX, y, "[D]");
    fxX += 20;
    if (engine_->getReverb().isEnabled()) display_.drawStr(fxX, y, "[R]");
}

void DisplayManager::displayTaskWrapper(void* param) {
    DisplayManager* mgr = static_cast<DisplayManager*>(param);
    while (mgr->running_) {
        mgr->update();
        vTaskDelay(pdMS_TO_TICKS(mgr->refreshDelayMs_));
    }
    vTaskDelete(NULL);
}
