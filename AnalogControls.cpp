#include "AnalogControls.h"
#include "SynthEngine.h"
#include <Arduino.h>

AnalogControls::AnalogControls() :
    engine_(nullptr)
{
    for (int i = 0; i < NUM_POTS; i++) {
        historyIndex_[i] = 0;
        lastValues_[i] = -1;
        currentValues_[i] = 0;
        potSums_[i] = 0;
        for (int j = 0; j < HISTORY_SIZE; j++) {
            potHistory_[i][j] = 0;
        }
    }
}

void AnalogControls::init(SynthEngine* engine) {
    engine_ = engine;

    analogReadResolution(12);  // 0-4095
    analogSetAttenuation(ADC_11db); // 0-3.3V

    // pinMode(..., ANALOG) is not valid on ESP32, INPUT or no pinMode is used for ADC.
    pinMode(POT_CUTOFF_PIN, INPUT);
    pinMode(POT_RESO_PIN, INPUT);
    pinMode(POT_VOLUME_PIN, INPUT);
    pinMode(POT_EFFECT_PIN, INPUT);
}

void AnalogControls::readPot(int index, int pin) {
    int raw = analogRead(pin);

    // Smooth using running sum moving average
    potSums_[index] -= potHistory_[index][historyIndex_[index]];
    potHistory_[index][historyIndex_[index]] = raw;
    potSums_[index] += raw;

    historyIndex_[index] = (historyIndex_[index] + 1) % HISTORY_SIZE;
    currentValues_[index] = potSums_[index] / HISTORY_SIZE;
}

void AnalogControls::update() {
    if (!engine_) return;

    readPot(0, POT_CUTOFF_PIN);
    readPot(1, POT_RESO_PIN);
    readPot(2, POT_VOLUME_PIN);
    readPot(3, POT_EFFECT_PIN);

    // Apply changes with deadzone
    for (int i = 0; i < NUM_POTS; i++) {
        if (abs(currentValues_[i] - lastValues_[i]) > DEADZONE) {
            float normalized = currentValues_[i] / 4095.0f;

            switch (i) {
                case 0: // Cutoff: 20Hz to 12000Hz (exponential-ish)
                    engine_->setFilterCutoff(20.0f + (normalized * normalized) * 11980.0f);
                    break;
                case 1: // Resonance
                    engine_->setFilterResonance(normalized);
                    break;
                case 2: // Volume
                    engine_->setMasterVolume(normalized);
                    break;
                case 3: // Effect Mix (using Reverb as primary)
                    engine_->getReverb().setMix(normalized);
                    if (normalized > 0.05f) engine_->getReverb().setEnabled(true);
                    else engine_->getReverb().setEnabled(false);
                    break;
            }
            lastValues_[i] = currentValues_[i];
        }
    }
}
