#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// I2S Audio (PCM5102 DAC)
constexpr uint8_t I2S_BCK_PIN      = 26;
constexpr uint8_t I2S_WS_PIN       = 25;
constexpr uint8_t I2S_DATA_OUT_PIN = 27;
constexpr uint8_t I2S_MCK_PIN      = 0;

// I2C Display (SH1106 OLED)
constexpr uint8_t I2C_SDA_PIN = 19;
constexpr uint8_t I2C_SCL_PIN = 18;

// ============================================================================
// AUDIO CONFIGURATION
// ============================================================================

constexpr uint32_t SAMPLE_RATE        = 48000;
constexpr uint8_t  AUDIO_BIT_DEPTH    = 16;
constexpr uint16_t DMA_BUFFER_COUNT   = 4;
constexpr uint16_t DMA_BUFFER_SAMPLES = 64;

// ============================================================================
// PHASE ACCUMULATOR
// ============================================================================

constexpr uint32_t PHASE_BITS     = 32;
constexpr uint32_t PHASE_MAX      = 0xFFFFFFFF;
constexpr float    PHASE_TO_FLOAT = 1.0f / (float)(1ULL << 32);
constexpr float    PHASE_INCREMENT_MULTIPLIER = 89478.485f;

// ============================================================================
// WAVETABLE CONFIGURATION
// ============================================================================

// Reduced to 256 samples - still good with linear interpolation
constexpr uint16_t WAVETABLE_SIZE    = 256;
constexpr uint16_t WAVETABLE_MASK    = WAVETABLE_SIZE - 1;
constexpr uint8_t  WAVETABLE_BITS    = 8;   // log2(256)

// 4 octave tables for practical range
constexpr uint8_t  NUM_OCTAVE_TABLES = 4;

// Memory: 4 octaves × 3 waveforms × 256 samples × 4 bytes = 12,288 bytes
// Plus sine: 256 × 4 = 1,024 bytes
// Total wavetables: ~13KB

// ============================================================================
// SYNTH CONFIGURATION
// ============================================================================

constexpr uint8_t MAX_OSCILLATORS = 4;

// ============================================================================
// MIDI CONFIGURATION
// ============================================================================

constexpr uint8_t MIDI_RX_PIN = 16;
constexpr uint8_t MIDI_TX_PIN = 17;

#endif
