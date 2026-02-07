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
    RAMP_DOWN,   // Inverted saw
    MORPH,       // Morph between sine, saw, square, triangle
    SD_TABLE,    // SD-loaded custom wavetable
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

    inline float readRampDown(uint32_t phase, int tableIndex) {
        return -readSaw(phase, tableIndex);
    }

    /**
     * Morph between Sine, Saw, Square, Triangle
     * morph: 0.0=Sine, 0.33=Saw, 0.66=Square, 1.0=Triangle
     */
    inline float readMorph(uint32_t phase, float morph, int tableIndex) {
        morph = constrain(morph, 0.0f, 1.0f);
        if (morph < 0.333f) {
            float t = morph * 3.0f;
            return lerp(readSine(phase), readSaw(phase, tableIndex), t);
        } else if (morph < 0.666f) {
            float t = (morph - 0.333f) * 3.0f;
            return lerp(readSaw(phase, tableIndex), readSquare(phase, tableIndex), t);
        } else {
            float t = (morph - 0.666f) * 3.0f;
            return lerp(readSquare(phase, tableIndex), readTriangle(phase, tableIndex), t);
        }
    }

    inline float readCustom(uint32_t phase, float* table, uint16_t size) {
        if (!table) return 0.0f;
        uint16_t mask = size - 1;
        // Fast log2 for common powers of 2
        uint8_t bits = 11; // Default for 2048
        if (size == 1024) bits = 10;
        else if (size == 4096) bits = 12;
        else if (size != 2048) bits = (uint8_t)log2(size);

        uint32_t tablePos = phase >> (PHASE_BITS - bits);
        uint32_t fracBits = (phase >> (PHASE_BITS - bits - 16)) & 0xFFFF;
        uint16_t idx0 = tablePos & mask;
        uint16_t idx1 = (tablePos + 1) & mask;
        float frac = fracBits * (1.0f / 65536.0f);

        return lerp(table[idx0], table[idx1], frac);
    }

    inline float readCustomMorph(uint32_t phase, float* tableA, float* tableB, float morph, uint16_t size) {
        if (!tableA) return 0.0f;
        if (!tableB || morph <= 0.0f) return readCustom(phase, tableA, size);
        if (morph >= 1.0f) return readCustom(phase, tableB, size);

        return lerp(readCustom(phase, tableA, size), readCustom(phase, tableB, size), morph);
    }

}

#endif
