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

    // Helpers for fast reading
    inline float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    inline void phaseToIndex(uint32_t phase, uint16_t& idx0, uint16_t& idx1, float& frac) {
        uint32_t tablePos = phase >> (PHASE_BITS - WAVETABLE_BITS);
        uint32_t fracBits = (phase >> (PHASE_BITS - WAVETABLE_BITS - 16)) & 0xFFFF;
        idx0 = tablePos & WAVETABLE_MASK;
        idx1 = (tablePos + 1) & WAVETABLE_MASK;
        frac = fracBits * (1.0f / 65536.0f);
    }

    inline float readSine(uint32_t phase) {
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(sinTable[idx0], sinTable[idx1], frac);
    }

    inline float readSaw(uint32_t phase, int tableIndex) {
        if (tableIndex < 0) tableIndex = 0;
        if (tableIndex >= NUM_OCTAVE_TABLES) tableIndex = NUM_OCTAVE_TABLES - 1;
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(sawTables[tableIndex][idx0], sawTables[tableIndex][idx1], frac);
    }

    inline float readSquare(uint32_t phase, int tableIndex) {
        if (tableIndex < 0) tableIndex = 0;
        if (tableIndex >= NUM_OCTAVE_TABLES) tableIndex = NUM_OCTAVE_TABLES - 1;
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(squareTables[tableIndex][idx0], squareTables[tableIndex][idx1], frac);
    }

    inline float readTriangle(uint32_t phase, int tableIndex) {
        if (tableIndex < 0) tableIndex = 0;
        if (tableIndex >= NUM_OCTAVE_TABLES) tableIndex = NUM_OCTAVE_TABLES - 1;
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(triangleTables[tableIndex][idx0], triangleTables[tableIndex][idx1], frac);
    }

}

#endif
