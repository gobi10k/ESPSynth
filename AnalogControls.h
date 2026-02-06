#ifndef ANALOG_CONTROLS_H
#define ANALOG_CONTROLS_H

#include "Config.h"

class SynthEngine;

class AnalogControls {
public:
    AnalogControls();

    void init(SynthEngine* engine);
    void update();

private:
    void readPot(int index, int pin);

    SynthEngine* engine_;

    static constexpr int NUM_POTS = 4;
    static constexpr int DEADZONE = 40;  // Increased to prevent jitter
    static constexpr int HISTORY_SIZE = 32; // Increased for better smoothing

    int potHistory_[NUM_POTS][HISTORY_SIZE];
    int historyIndex_[NUM_POTS];
    int lastValues_[NUM_POTS];
    int currentValues_[NUM_POTS];
};

#endif
