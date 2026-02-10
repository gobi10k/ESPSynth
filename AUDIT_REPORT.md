# ESP32 Synth v5 — Final Audit Report

## Summary

After a thorough review of all 38 source files, the codebase is in **very good shape** overall. The architecture is well-structured, memory-conscious, and performance-aware. Below are the issues found, categorized by severity.

---

## CRITICAL — Bugs that will cause audible problems or crashes

### 1. `applyPreset()` calls non-existent method `setGranularMix` on GranularExciter
**File:** `esp32_synth.ino` line 242  
**Issue:** `synth.getGranular().setGranularMix(...)` — `GranularExciter` has no `setGranularMix()` method. The correct call is `synth.setGranularMix(...)`.  
**Impact:** Compile error.

### 2. `Voice::noteOn()` glide logic is inverted
**File:** `Voice.cpp` lines 28–33  
**Issue:** When `glide=true` (meaning we WANT glide), it calls `setTarget()` which smooths. When `glide=false` (meaning we DON'T want glide), it calls `setImmediate()` which snaps. This is correct. BUT — the fallback `pitchSmooth_.getCurrent() == 0.0f` check means the very first note will always snap (correct), but after `forceOff()` which sets immediate to 0.0f, the next non-glide note will also accidentally glide from 0 Hz. This causes a pitch sweep from silence on voice re-use after stealing.  
**Fix:** Initialize `pitchSmooth_` to a sensible default and reset it properly in `forceOff()`.

### 3. `Envelope::process()` decay overshoot target can go negative
**File:** `Envelope.h` lines 50–55  
**Issue:** `float overshootTarget = sustainLevel_ * 0.95f - 0.01f;` — when sustain is 0, this becomes `-0.01f`, meaning the envelope will decay past zero and the `<= sustainLevel_` check (which is 0) will immediately trigger the sustain stage at a negative value. Then sustain snaps to 0, causing a tiny discontinuity/click.  
**Fix:** Clamp the overshoot target to `max(sustainLevel_ * 0.95f - 0.01f, -0.001f)` and ensure the transition is clean.

### 4. `Resonator` uses `y1_`/`y2_` arrays that are never written to
**File:** `Resonator.h` declares `y1_[MAX_RESONATORS]`, `y2_[MAX_RESONATORS]` but `processResonator()` uses Direct Form II Transposed which only uses `x1_`, `x2_`. The `y1_`/`y2_` waste 48 bytes of RAM.  
**Impact:** Wasted memory (not a crash, but dead weight for distribution).

### 5. `PresetData` static_assert may break with padding differences
**File:** `PresetManager.h`  
**Issue:** The struct uses `__attribute__((packed))` which should prevent padding, but the `static_assert(sizeof(PresetData) == 128)` could fail on certain compiler versions if the attribute isn't respected. Current count of fields actually sums to ~125 bytes + the 15 reserved, so this is fine as-is, but the reserved field size should be validated.

---

## HIGH — Audio quality improvements

### 6. SVF Filter: no DC blocker
**File:** `Filter.cpp`  
**Issue:** The Chamberlin SVF can accumulate DC offset over time, especially with modulation. A simple DC blocker on the output would prevent sub-sonic rumble.

### 7. Compressor gain smoothing can click on enable/disable
**File:** `Compressor.cpp`  
**Issue:** When `enabled_` toggles, the compressor instantly bypasses or engages with no crossfade. If gain reduction was -6dB and you disable, you get a +6dB jump.  
**Fix:** Add a smooth transition on enable/disable.

### 8. Moog filter: resonance feedback too hot at high settings
**File:** `MoogFilter.cpp` line 53  
**Issue:** `feedback = resonance_ * 4.0f` — the basic Moog uses 4x which is correct for self-oscillation, but the `fastPolyClip` in the feedback path (which saturates at ±0.667) combined with the final `fastPolyClip` creates excessive gain reduction at moderate resonance. The ladder filter uses 3.5x which is better calibrated.

### 9. Pulse waveform aliasing
**File:** `Oscillator.h` (process() method)  
**Issue:** The pulse wave uses a naive threshold comparison (`t < effectivePW ? 1 : -1`) followed by a simple 0.3/0.7 filter. This generates significant aliasing at higher frequencies. For a "polished" synth, a polyBLEP or at minimum a slightly better anti-aliasing filter would help.

### 10. Reverb pre-delay conversion is asymmetric
**File:** `Reverb.h`  
**Issue:** `getPreDelay()` returns `preDelayTime_ * 1000.0f / SAMPLE_RATE` but `setPreDelay()` takes ms and converts with `ms * SAMPLE_RATE / 1000.0f`. The getter/setter are consistent but the applyPreset code passes `p.revPre` directly as ms (a uint8_t 0–255), meaning max pre-delay is ~255ms which exceeds `PREDELAY_MAX` (2400 samples = 50ms). Values above 50 will be clamped silently.  
**Fix:** Document the actual range or increase buffer.

---

## MEDIUM — Performance optimizations

### 11. `Voice::process()` filter envelope runs at 1/4 rate but still calls `filterEnv_.process()` once per 4 samples
**File:** `Voice.cpp` line 106  
**Issue:** The filter envelope only processes every 4th sample (`age_ & 0x03`), but it only calls `process()` once when it does. This means the envelope effectively runs at 12kHz, which is fine, but the value used between updates is stale. For the amp envelope (line 121) it runs every sample which is correct. This is a reasonable tradeoff but could cause audible stepping on fast filter sweeps.

### 12. `processBlock()` pitch bend processes entire buffer then takes last value
**File:** `SynthEngine.cpp` line 439  
**Issue:** `for (int i = 0; i < DMA_BUFFER_SAMPLES; i++) pitchBend_.process();` processes 128 times but only the final value is used. This is wasteful — just compute the target directly or process once with the equivalent time step.

### 13. Granular window: BLACKMAN uses HANN table
**File:** `Granular.cpp` line 146  
**Issue:** The `GrainWindow::BLACKMAN` case falls through to `HANN`, using the Hann window table. If you're advertising Blackman as an option, it should use actual Blackman coefficients or be removed/renamed.

### 14. `Arpeggiator::process()` velocity index can be wrong
**File:** `Arpeggiator.cpp` line 182  
**Issue:** `int velIdx = stepIndex_ % numHeld_` — but `advanceStep()` has already been conceptualized to run after `getNextNote()`, and `stepIndex_` is the *next* step, not current. The velocity should be captured from the same index used by `getNextNote()` before advancing.

---

## LOW — Code quality / polish

### 15. `SDManager` leaks SPIClass on re-init
**File:** `SDManager.cpp` line 27  
Already handles `delete spiBus_` before re-creating. Good.

### 16. `TWO_PI` macro conflicts with Arduino's built-in
**File:** `MathUtils.h`  
`#define TWO_PI 6.28318530717958647693f` — Arduino already defines `TWO_PI`. This can cause redefinition warnings. Should use `#ifndef TWO_PI`.

### 17. `WavetableManager` uses `SD.open()` directly instead of through `SDManager`
**File:** `WavetableManager.cpp`  
Bypasses the abstraction layer. Not a bug but inconsistent.

### 18. `Voice` missing getters for osc coarse/supersaw detune
**File:** `esp32_synth.ino` lines 261–265  
`synth.getVoice(0).getOscCoarse(0)` — Voice has no `getOscCoarse()` or `getOscSupersawDetune()` methods. This would be a compile error. These should be pulled from `pendingParams_` on SynthEngine instead.

### 19. `morph_` uninitialized in Oscillator
**File:** `Oscillator.cpp`  
`morph_` is declared in the header but never initialized in the constructor. Should be `morph_(0.0f)`.

---

## Files to patch

Based on the above, here are the concrete fixes I'll apply:
1. `esp32_synth.ino` — Fix compile error on line 242, fix Voice getter calls
2. `Voice.cpp` — Fix pitch glide from zero on voice reuse  
3. `Envelope.h` — Fix decay overshoot when sustain=0
4. `MathUtils.h` — Guard TWO_PI redefinition
5. `Oscillator.cpp` — Initialize morph_
6. `Granular.cpp` — Rename/fix BLACKMAN window
7. `Resonator.h` — Remove unused y1_/y2_ arrays
8. `SynthEngine.cpp` — Optimize pitch bend processing
9. `SynthEngine.h` — Add getOscCoarse/getOscSupersawDetune/getPulseWidth accessors
10. `Arpeggiator.cpp` — Fix velocity index timing
