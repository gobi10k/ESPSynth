# ESP32 Polyphonic Synthesizer v5 - Documentation

## 1. Overview
The ESP32 Synthesizer v5 is a high-performance, polyphonic "Resonant Spectral Engine" designed for standalone or MIDI-controlled operation. It features a unified architecture, rich synthesis capabilities, and an optimized master effects chain.

## 2. Architecture
- **Unified Synth Engine:** Monolithic engine managing voice allocation, global modulation, and effects.
- **Polyphonic Voice:** 4-voice polyphony with voice stealing. Each voice is independent with dual oscillators and dual filters.
- **Task Distribution:**
  - **Core 1 (Audio Task):** High-priority real-time DSP execution (48kHz).
  - **Core 0 (Main Loop):** Hardware scanning, MIDI processing, preset management, and UI updates.
- **Safety Guards:** Automatic `isnan`/`isinf` detection and state reset in all recursive DSP loops.

## 3. Synthesis Engine
### Oscillators
- **Dual Oscillators per Voice:** Independently configurable.
- **8 Waveforms:** Sine, Saw, Square, Triangle, Pulse (PWM), Supersaw (7 stacked saws), Noise (White), and Ramp Down.
- **Wavetable Morphing:** Seamless blending between Sine, Saw, Square, and Triangle waves.
- **Unison:** 1-4 stacked copies per voice with adjustable detune (up to 16 virtual oscillators at full polyphony).
- **Pitch Glide:** Smooth portamento between notes.

### Filters
- **Dual Filter Topology:** Selectable per-voice between:
  - **SVF (State Variable Filter):** Chamberlin topology (LP, HP, BP, Notch).
  - **Moog Ladder Filter:** 4-pole approximation with nonlinear saturation.
- **Keyboard Tracking:** Cutoff frequency tracks the note pitch for consistent tonal character.
- **Envelope Modulation:** Dedicated ADSR envelope for filter cutoff.
- **Velocity Scaling:** Filter envelope depth responds dynamically to note velocity.

### Advanced Modes
- **FM Synthesis:** Oscillator 1 modulates Oscillator 0.
- **Ring Modulation:** Balanced multiplication of both oscillators.
- **Oscillator Sync:** Hard-sync Oscillator 1 to Oscillator 0.

## 4. Master Effects Chain
The post-mix signal passes through a comprehensive effect suite:
1. **Resonator Bank:** Global modal resonator with tracking.
2. **Comb Filter:** High-feedback physical modeling filter.
3. **Saturation:** Multiple modes (Soft, Hard, Foldback, Bitcrush).
4. **Chorus:** Stereo-width modulation effect.
5. **Delay:** 250ms delay with feedback and sync.
6. **FDN Reverb:** Pseudo-stereo Feedback Delay Network reverb with high density.
7. **Compressor:** Dynamics processor with adjustable threshold/ratio/attack.
8. **Soft Clipper:** Final output safety using `fastTanh`.

## 5. Hardware Integration
- **Display:** SH1106 OLED (128x64) via I2C (GPIO 21, 22).
- **Audio Output:** PCM5102 DAC via I2S (BCK:26, WS:25, DATA:27).
- **MIDI Input:** Hardware UART (RX:16, TX:17).
- **Analog Inputs:** 4 Potentiometers on ADC1 (GPIO 32, 33, 34, 35) for Cutoff, Resonance, Volume, and Effects.
- **Control Interface:**
  - **Encoder 1 (Nav):** Page switching and item selection (A:36, B:39, SW:15).
  - **Encoder 2 (Value):** Parameter adjustment (A:14, B:12, SW:13).
- **Storage:** Micro SD Card (SPI: 5, 18, 19, 23) for preset management.

## 6. MIDI & Preset System
- **Presets:** 16 internal slots + unlimited SD card storage.
- **MIDI CC Support:**
  - 1: Mod Wheel (LFO Depth)
  - 7: Master Volume
  - 10: Global Pan
  - 12: Osc Mix
  - 14/15: Osc Waveforms
  - 16: Osc Detune
  - 20: Synth Mode
  - 21: FM Amount
  - 22: Filter Type
  - 23: Key Tracking
  - 24: Filter Velocity
  - 25: Unison Voices
  - 26: Unison Detune
  - 27: Wave Morph
  - 71/74: Filter Cutoff/Reso
  - 70/72/73/75: Envelope ADSR
  - 91/93/94: FX Send (Reverb, Chorus, Delay)
- **Program Change:** Messages 0-15 load the corresponding internal preset.

## 7. Performance Optimizations
- **Control Rate Updates:** Heavy modulation (LFOs, Mod Matrix) is calculated once per block (128 samples) for 128x efficiency.
- **Fast DSP Math:** custom `MathUtils.h` providing optimized approximations for `exp`, `tanh`, and `log`.
- **Cached Increments:** Oscillator phase increments are cached to eliminate redundant float math.
- **Heartbeat Monitor:** Core 0 monitors Core 1 liveness via a volatile block counter to ensure system stability.

## 8. SD Card Setup & Troubleshooting
The SD card system is critical for long-term preset storage. If initialization fails, check the following:

### Hardware Requirements
- **Formatting:** Must be **FAT32** or **FAT16**. ExFAT is not supported by the standard Arduino SD library.
- **Partitioning:** Use a **Master Boot Record (MBR)** partition scheme. GPT partitions may not be recognized.
- **Wiring (VSPI):**
  - **CS:** GPIO 5
  - **SCK:** GPIO 18
  - **MISO:** GPIO 19
  - **MOSI:** GPIO 23
- **Voltage:** The ESP32 is a 3.3V device. Ensure your SD card module has proper level shifters if powering from 5V, or ideally, use a 3.3V native module.

### Software Configuration
- **Directory Structure:** The synth expects a `/presets/` directory at the root of the card.
- **File Format:** Presets are stored as binary files with the `.sy` extension.
- **Auto-Initialization:** On first successful boot with a blank SD card, the synth will automatically:
  1. Create a `welcome.txt` file.
  2. Create the `/presets/` directory.
  3. Generate 3 initial test presets.

### Troubleshooting Steps
1. **Card Detection:** If you see `[SD] Card NOT detected`, check the CS pin wiring and ensure the card is fully inserted.
2. **Handshake Failures:** If initialization fails at 400kHz, it usually indicates signal integrity issues. Keep SPI wires as short as possible (< 10cm).
3. **Dedicated Bus:** This project uses a dedicated VSPI instance to prevent conflicts with the I2S audio task. Ensure no other peripherals are sharing these pins.
4. **Manual Creation:** If the synth cannot create the directory, you can manually create a folder named `presets` on your PC and try again.
