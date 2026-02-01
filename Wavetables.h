#ifndef WAVETABLES_H
#define WAVETABLES_H

#include "Config.h"

// ============================================================================
// WAVEFORM TYPES
// ============================================================================

enum class Waveform : uint8_t {
    SINE = 0,
    SAW,
    SQUARE,
    TRIANGLE,
    PULSE,       // Variable pulse width
    SUPERSAW,    // Multiple detuned saws
    NOISE,       // White noise
    NUM_WAVEFORMS
};

extern const char* WAVEFORM_NAMES[];

// ============================================================================
// WAVETABLES NAMESPACE
// ============================================================================

namespace Wavetables {

    extern float sawTables[NUM_OCTAVE_TABLES][WAVETABLE_SIZE];
    extern float squareTables[NUM_OCTAVE_TABLES][WAVETABLE_SIZE];
    extern float triangleTables[NUM_OCTAVE_TABLES][WAVETABLE_SIZE];
    extern float sinTable[WAVETABLE_SIZE];
    extern float octaveFreqLimits[NUM_OCTAVE_TABLES];

    void init();
    int tableIndexForFreq(float freq);
    float readSine(uint32_t phase);
    float readSaw(uint32_t phase, int tableIndex);
    float readSquare(uint32_t phase, int tableIndex);
    float readTriangle(uint32_t phase, int tableIndex);

}

#endif
