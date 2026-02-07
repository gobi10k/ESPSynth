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
    currentPage_(DisplayPage::MAIN),
    selectedItem_(0),
    sdFileIndex_(0),
    sdWaveMode_(false)
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
    selectedItem_ = 0;
}

void DisplayManager::prevPage() {
    int page = (int)currentPage_ - 1;
    if (page < 0) page = (int)DisplayPage::NUM_PAGES - 1;
    currentPage_ = (DisplayPage)page;
    selectedItem_ = 0;
}

void DisplayManager::nextItem() {
    selectedItem_++;
    int maxItems = 4;
    if (currentPage_ == DisplayPage::MAIN) maxItems = 0;
    else if (currentPage_ == DisplayPage::FILTER) maxItems = 6;
    else if (currentPage_ == DisplayPage::OSCILLATORS) maxItems = 6;
    else if (currentPage_ == DisplayPage::ENVELOPES) maxItems = 8;
    else if (currentPage_ == DisplayPage::SD_BROWSER) maxItems = 2;

    if (selectedItem_ >= maxItems) selectedItem_ = 0;
}

void DisplayManager::prevItem() {
    selectedItem_--;
    int maxItems = 4;
    if (currentPage_ == DisplayPage::MAIN) maxItems = 0;
    else if (currentPage_ == DisplayPage::FILTER) maxItems = 6;
    else if (currentPage_ == DisplayPage::OSCILLATORS) maxItems = 6;
    else if (currentPage_ == DisplayPage::ENVELOPES) maxItems = 8;
    else if (currentPage_ == DisplayPage::SD_BROWSER) maxItems = 2;

    if (selectedItem_ < 0) selectedItem_ = maxItems - 1;
    if (selectedItem_ < 0) selectedItem_ = 0;
}

void DisplayManager::adjustValue(int delta) {
    if (!engine_) return;

    switch (currentPage_) {
        case DisplayPage::OSCILLATORS:
            switch (selectedItem_) {
                case 0: {
                    int wf = (int)engine_->getOscWaveform(0) + delta;
                    while (wf < 0) wf += (int)Waveform::NUM_WAVEFORMS;
                    engine_->setOscWaveform(0, (Waveform)(wf % (int)Waveform::NUM_WAVEFORMS));
                    break;
                }
                case 1: {
                    int wf = (int)engine_->getOscWaveform(1) + delta;
                    while (wf < 0) wf += (int)Waveform::NUM_WAVEFORMS;
                    engine_->setOscWaveform(1, (Waveform)(wf % (int)Waveform::NUM_WAVEFORMS));
                    break;
                }
                case 2: engine_->setOscMix(constrain(engine_->getOscMix() + delta * 0.05f, 0.0f, 1.0f)); break;
                case 3: engine_->setOscDetune(1, constrain(engine_->getOscDetune(1) + delta * 0.5f, -50.0f, 50.0f)); break;
                case 4: engine_->setUnisonVoices(constrain(engine_->getUnisonVoices() + delta, 1, NUM_VOICES)); break;
                case 5: {
                    for(int i=0; i<NUM_VOICES; i++) {
                        float m = constrain(engine_->getVoice(i).getOsc(0).getMorph() + delta * 0.05f, 0.0f, 1.0f);
                        engine_->getVoice(i).getOsc(0).setMorph(m);
                        engine_->getVoice(i).getOsc(1).setMorph(m);
                    }
                    break;
                }
            }
            break;

        case DisplayPage::ENVELOPES:
            switch (selectedItem_) {
                case 0: engine_->setAmpADSR(constrain(engine_->getAmpA() + delta * 0.01f, 0.0f, 5.0f), -1, -1, -1); break;
                case 1: engine_->setAmpADSR(-1, constrain(engine_->getAmpD() + delta * 0.01f, 0.0f, 5.0f), -1, -1); break;
                case 2: engine_->setAmpADSR(-1, -1, constrain(engine_->getAmpS() + delta * 0.05f, 0.0f, 1.0f), -1); break;
                case 3: engine_->setAmpADSR(-1, -1, -1, constrain(engine_->getAmpR() + delta * 0.01f, 0.0f, 5.0f)); break;
                case 4: engine_->setFilterADSR(constrain(engine_->getFltA() + delta * 0.01f, 0.0f, 5.0f), -1, -1, -1); break;
                case 5: engine_->setFilterADSR(-1, constrain(engine_->getFltD() + delta * 0.01f, 0.0f, 5.0f), -1, -1); break;
                case 6: engine_->setFilterADSR(-1, -1, constrain(engine_->getFltS() + delta * 0.05f, 0.0f, 1.0f), -1); break;
                case 7: engine_->setFilterADSR(-1, -1, -1, constrain(engine_->getFltR() + delta * 0.01f, 0.0f, 5.0f)); break;
            }
            break;

        case DisplayPage::FILTER:
            switch (selectedItem_) {
                case 0: {
                    int ft = (int)engine_->getFilterType() + delta;
                    while (ft < 0) ft += 2;
                    engine_->setFilterType((VoiceFilterType)(ft % 2));
                    break;
                }
                case 1: {
                    int fm = (int)engine_->getFilterMode() + delta;
                    while (fm < 0) fm += 4;
                    engine_->setFilterMode((FilterMode)(fm % 4));
                    break;
                }
                case 2: engine_->setFilterCutoff(constrain(engine_->getFilterCutoff() * (1.0f + delta * 0.05f), 20.0f, 20000.0f)); break;
                case 3: engine_->setFilterResonance(constrain(engine_->getFilterResonance() + delta * 0.05f, 0.0f, 1.0f)); break;
                case 4: engine_->setFilterKeyTracking(constrain(engine_->getFilterKeyTracking() + delta * 0.1f, 0.0f, 1.0f)); break;
                case 5: engine_->setFilterEnvVelocity(constrain(engine_->getFilterEnvVelocity() + delta * 0.1f, 0.0f, 1.0f)); break;
            }
            break;

        case DisplayPage::EFFECTS:
            {
                EffectsChain& fx = engine_->getEffects();
                switch (selectedItem_) {
                    case 0: fx.setEnabled(!fx.isSatEnabled(), fx.isChorusEnabled(), fx.isDelayEnabled()); break;
                    case 1: fx.setEnabled(fx.isSatEnabled(), !fx.isChorusEnabled(), fx.isDelayEnabled()); break;
                    case 2: fx.setEnabled(fx.isSatEnabled(), fx.isChorusEnabled(), !fx.isDelayEnabled()); break;
                    case 3: engine_->getReverb().setEnabled(!engine_->getReverb().isEnabled()); break;
                }
            }
            break;

        case DisplayPage::SD_BROWSER:
            if (selectedItem_ == 0) {
                sdWaveMode_ = !sdWaveMode_;
                sdFileIndex_ = 0;
            } else if (selectedItem_ == 1) {
                sdFileIndex_ += delta;
                if (sdFileIndex_ < 0) sdFileIndex_ = 0;
                // Clamp to max files in directory (to be implemented properly)
            }
            break;

        default: break;
    }
}

void DisplayManager::drawUI() {
    if (!engine_) return;
    
    switch (currentPage_) {
        case DisplayPage::MAIN: drawMainPage(); break;
        case DisplayPage::OSCILLATORS: drawOscPage(); break;
        case DisplayPage::FILTER: drawFilterPage(); break;
        case DisplayPage::ENVELOPES: drawEnvPage(); break;
        case DisplayPage::EFFECTS: drawEffectsPage(); break;
        case DisplayPage::SD_BROWSER: drawSDPage(); break;
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

    display_.drawStr(0, 22, selectedItem_ == 0 ? "> W1:" : "  W1:");
    display_.drawStr(35, 22, WAVEFORM_NAMES[(int)wf1]);

    display_.drawStr(64, 22, selectedItem_ == 1 ? "> W2:" : "  W2:");
    display_.drawStr(99, 22, WAVEFORM_NAMES[(int)wf2]);

    snprintf(buf, sizeof(buf), "%s MIX: %.0f%%", selectedItem_ == 2 ? ">" : " ", engine_->getOscMix() * 100.0f);
    display_.drawStr(0, 34, buf);

    snprintf(buf, sizeof(buf), "%s DET: %.1f", selectedItem_ == 3 ? ">" : " ", engine_->getOscDetune(1));
    display_.drawStr(64, 34, buf);

    snprintf(buf, sizeof(buf), "%s UNISON: %d", selectedItem_ == 4 ? ">" : " ", engine_->getUnisonVoices());
    display_.drawStr(0, 46, buf);

    snprintf(buf, sizeof(buf), "%s MORPH: %.0f%%", selectedItem_ == 5 ? ">" : " ", engine_->getVoice(0).getOsc(0).getMorph() * 100.0f);
    display_.drawStr(0, 58, buf);
}

void DisplayManager::drawEnvPage() {
    char buf[32];
    display_.drawStr(0, 7, "ENVELOPES");
    display_.drawLine(0, 9, 127, 9);

    display_.drawStr(0, 20, "AMP ADSR:");
    snprintf(buf, sizeof(buf), "%s%.1f", selectedItem_ == 0 ? ">" : "A:", engine_->getAmpA());
    display_.drawStr(0, 32, buf);
    snprintf(buf, sizeof(buf), "%s%.1f", selectedItem_ == 1 ? ">" : "D:", engine_->getAmpD());
    display_.drawStr(32, 32, buf);
    snprintf(buf, sizeof(buf), "%s%.0f", selectedItem_ == 2 ? ">" : "S:", engine_->getAmpS() * 100.0f);
    display_.drawStr(64, 32, buf);
    snprintf(buf, sizeof(buf), "%s%.1f", selectedItem_ == 3 ? ">" : "R:", engine_->getAmpR());
    display_.drawStr(96, 32, buf);

    display_.drawStr(0, 46, "FLT ADSR:");
    snprintf(buf, sizeof(buf), "%s%.1f", selectedItem_ == 4 ? ">" : "A:", engine_->getFltA());
    display_.drawStr(0, 58, buf);
    snprintf(buf, sizeof(buf), "%s%.1f", selectedItem_ == 5 ? ">" : "D:", engine_->getFltD());
    display_.drawStr(32, 58, buf);
    snprintf(buf, sizeof(buf), "%s%.0f", selectedItem_ == 6 ? ">" : "S:", engine_->getFltS() * 100.0f);
    display_.drawStr(64, 58, buf);
    snprintf(buf, sizeof(buf), "%s%.1f", selectedItem_ == 7 ? ">" : "R:", engine_->getFltR());
    display_.drawStr(96, 58, buf);
}

void DisplayManager::drawFilterPage() {
    char buf[32];
    display_.drawStr(0, 7, "FILTER");
    display_.drawLine(0, 9, 127, 9);

    const char* typeName = (engine_->getFilterType() == VoiceFilterType::SVF) ? "SVF" : "LADDER";
    snprintf(buf, sizeof(buf), "%s TYP:%s", selectedItem_ == 0 ? ">" : " ", typeName);
    display_.drawStr(0, 22, buf);

    snprintf(buf, sizeof(buf), "%s MOD:%s", selectedItem_ == 1 ? ">" : " ", FILTER_MODE_NAMES[(int)engine_->getFilterMode()]);
    display_.drawStr(64, 22, buf);

    float cutoff = engine_->getFilterCutoff();
    snprintf(buf, sizeof(buf), "%s CUTOFF: %.0f Hz", selectedItem_ == 2 ? ">" : " ", cutoff);
    display_.drawStr(0, 34, buf);

    snprintf(buf, sizeof(buf), "%s RESO: %.0f%%", selectedItem_ == 3 ? ">" : " ", engine_->getFilterResonance() * 100.0f);
    display_.drawStr(0, 46, buf);

    snprintf(buf, sizeof(buf), "%s KBD: %.0f%%", selectedItem_ == 4 ? ">" : " ", engine_->getFilterKeyTracking() * 100.0f);
    display_.drawStr(0, 58, buf);

    snprintf(buf, sizeof(buf), "%s VEL: %.0f%%", selectedItem_ == 5 ? ">" : " ", engine_->getFilterEnvVelocity() * 100.0f);
    display_.drawStr(64, 58, buf);
}

void DisplayManager::drawEffectsPage() {
    display_.drawStr(0, 7, "EFFECTS");
    display_.drawLine(0, 9, 127, 9);
    
    EffectsChain& fx = engine_->getEffects();

    display_.drawStr(0, 22, selectedItem_ == 0 ? "> SAT:" : "  SAT:");
    display_.drawStr(50, 22, fx.isSatEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 34, selectedItem_ == 1 ? "> CHORUS:" : "  CHORUS:");
    display_.drawStr(60, 34, fx.isChorusEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 46, selectedItem_ == 2 ? "> DELAY:" : "  DELAY:");
    display_.drawStr(55, 46, fx.isDelayEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 58, selectedItem_ == 3 ? "> REVERB:" : "  REVERB:");
    display_.drawStr(60, 58, engine_->getReverb().isEnabled() ? "ON" : "OFF");
}

void DisplayManager::drawSDPage() {
    display_.drawStr(0, 7, "SD BROWSER");
    display_.drawLine(0, 9, 127, 9);

    display_.drawStr(0, 22, selectedItem_ == 0 ? "> MODE:" : "  MODE:");
    display_.drawStr(50, 22, sdWaveMode_ ? "WAVES" : "PRESETS");

    display_.drawStr(0, 34, "FILE:");

    // Browse logic: Encoder 2 delta will change sdFileIndex_
    // In a real implementation we would scan the directory.
    // For now we'll show what's in the WavetableManager if in wave mode.
    if (sdWaveMode_) {
        const char* name = engine_->getWavetableManager().getWaveFileName(sdFileIndex_);
        if (name) display_.drawStr(35, 34, name);
        else display_.drawStr(35, 34, "<EMPTY>");
    } else {
        display_.drawStr(35, 34, "preset_0.sy");
    }

    display_.drawStr(0, 58, "ENC2: SCROLL, CLICK: LOAD");
}

void DisplayManager::displayTaskWrapper(void* param) {
    DisplayManager* mgr = static_cast<DisplayManager*>(param);
    while (mgr->running_) {
        mgr->update();
        vTaskDelay(pdMS_TO_TICKS(mgr->refreshDelayMs_));
    }
    vTaskDelete(NULL);
}
