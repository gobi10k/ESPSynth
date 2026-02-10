#include "SynthesisTests.h"
#include "Reverb.h"
#include "Resonator.h"
#include "CombFilter.h"
#include "Granular.h"
#include "Effects.h"
#include "ModMatrix.h"
#include "Oscillator.h"
#include "Filter.h"
#include "MoogFilter.h"
#include "Envelope.h"
#include "Synthesis.h"
#include "Voice.h"
#include "Compressor.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

// Helper: time N iterations of a lambda, return microseconds per iteration
template<typename F>
static float benchmarkUs(F func, int iterations) {
    esp_task_wdt_reset();
    yield();
    uint32_t t0 = micros();
    for (int i = 0; i < iterations; i++) {
        func(i);
        // Feed every 500 iterations if it's a long benchmark
        if ((i & 0x1FF) == 0) esp_task_wdt_reset();
    }
    uint32_t elapsed = micros() - t0;
    return (float)elapsed / (float)iterations;
}

void SynthesisTests::runAll() {
    Serial.println("\n========================================");
    Serial.println("   SYNTHESIS TESTS & CRASH DIAGNOSTICS");
    Serial.println("========================================\n");

    Serial.println("--- Unit Tests ---");
    testOscillator();
    testFilter();
    testMoogFilter();
    testEnvelope();
    testFM();
    testSync();
    testRingMod();
    testModMatrix();

    Serial.println("\n--- Stability Stress Tests (100k samples) ---");
    testSaturationStress();
    testChorusStress();
    testReverbStress();
    testResonatorStress();
    testCombStress();
    testGranularStress();

    Serial.println("\n--- NEW: Crash Diagnostic Tests ---");
    testMemoryPressure();
    testTimingBudget();
    testFullChainWorstCase();
    testNaNPropagation();
    testVoiceLifecycle();
    testFeedbackAccumulation();
    testGranularProcessor();
    testReverbFreeze();
    testResonatorProfiles();
    testSpectralStability();

    Serial.println("\n========================================");
    Serial.println("   ALL TESTS COMPLETED");
    Serial.println("========================================\n");
}

// ============================================================================
// TIMING BUDGET - The most important crash diagnostic
// ============================================================================

void SynthesisTests::testTimingBudget() {
    Serial.println("\n>>> TIMING BUDGET (cost per sample in us) <<<");
    
    const float blockDurationUs = (DMA_BUFFER_SAMPLES * 1000000.0f) / SAMPLE_RATE;
    const float perSampleBudget = blockDurationUs / DMA_BUFFER_SAMPLES;
    Serial.printf("  Block budget: %.0f us (%d samples @ %d Hz)\n", 
                  blockDurationUs, DMA_BUFFER_SAMPLES, SAMPLE_RATE);
    Serial.printf("  Per-sample budget: %.2f us\n\n", perSampleBudget);
    
    const int N = 10000;
    float totalWorstCase = 0.0f;
    
    // --- Oscillator costs ---
    {
        Oscillator osc;
        osc.setFrequency(440.0f);
        
        osc.setWaveform(Waveform::SINE);
        float tSine = benchmarkUs([&](int) { osc.process(); }, N);
        
        osc.setWaveform(Waveform::SAW);
        float tSaw = benchmarkUs([&](int) { osc.process(); }, N);

        osc.setWaveform(Waveform::SUPERSAW);
        osc.setSupersawDetune(0.5f);
        float tSuper = benchmarkUs([&](int) { osc.process(); }, N);

        osc.setWaveform(Waveform::PULSE);
        float tPulse = benchmarkUs([&](int) { osc.process(); }, N);

        Serial.printf("  Osc Sine:     %.2f us\n", tSine);
        Serial.printf("  Osc Saw:      %.2f us\n", tSaw);
        Serial.printf("  Osc Supersaw: %.2f us  << 7 saws!\n", tSuper);
        Serial.printf("  Osc Pulse:    %.2f us\n", tPulse);
        totalWorstCase += tSuper * 2; // Two oscs per voice, worst case supersaw
    }

    // --- Filter costs ---
    {
        Filter svf;
        svf.setCutoff(1000.0f);
        svf.setResonance(0.7f);
        float tSVF = benchmarkUs([&](int) { svf.process(0.5f); }, N);

        LadderFilter ladder;
        ladder.setCutoff(1000.0f);
        ladder.setResonance(0.7f);
        float tLadder = benchmarkUs([&](int) { ladder.process(0.5f); }, N);

        Serial.printf("  Filter SVF:   %.2f us\n", tSVF);
        Serial.printf("  Filter Moog:  %.2f us\n", tLadder);
        totalWorstCase += max(tSVF, tLadder);
    }

    // --- Envelope costs ---
    {
        Envelope env;
        env.setADSR(0.01f, 0.1f, 0.5f, 0.3f);
        env.gate(true);
        float tEnv = benchmarkUs([&](int) { env.process(); }, N);
        Serial.printf("  Envelope:     %.2f us (x2 per voice)\n", tEnv);
        totalWorstCase += tEnv * 2;
    }

    // --- Per-voice total x4 ---
    float voiceTotal = totalWorstCase;
    totalWorstCase *= 4;
    Serial.printf("\n  >> Per voice total:  %.2f us\n", voiceTotal);
    Serial.printf("  >> 4 voices total:   %.2f us\n\n", totalWorstCase);

    // --- Effects costs ---
    {
        Saturation sat;
        sat.setDrive(5.0f);
        sat.setType(SaturationType::FOLDBACK);
        float l = 0.5f, r = 0.3f;
        float tSat = benchmarkUs([&](int) { sat.processStereo(l, r); }, N);

        Chorus cho;
        cho.setDepth(0.8f);
        cho.setMix(0.5f);
        float tCho = benchmarkUs([&](int) { cho.processStereo(l, r); }, N);

        Delay dly;
        dly.setTime(0.1f);
        dly.setFeedback(0.5f);
        float tDly = benchmarkUs([&](int) { dly.processStereo(l, r); }, N);

        Serial.printf("  Saturation:   %.2f us\n", tSat);
        Serial.printf("  Chorus:       %.2f us\n", tCho);
        Serial.printf("  Delay:        %.2f us\n", tDly);
        totalWorstCase += tSat + tCho + tDly;
    }

    // --- Resonator + Comb ---
    {
        ResonatorBank rb;
        rb.setFrequency(110.0f);
        rb.setResonance(20.0f);
        rb.setMix(0.5f);
        float tReso = benchmarkUs([&](int) { rb.process(0.5f); }, N);

        CombFilter cb;
        cb.setPitch(110.0f);
        cb.setFeedback(0.8f);
        float tComb = benchmarkUs([&](int) { cb.process(0.5f); }, N);

        Serial.printf("  Resonator:    %.2f us\n", tReso);
        Serial.printf("  Comb:         %.2f us\n", tComb);
        totalWorstCase += tReso + tComb;
    }

    // --- Granular ---
    {
        GranularExciter ge;
        ge.setDensity(50.0f);
        ge.setDuration(50.0f);
        ge.setFreeRunning(true);
        // Warm it up
        for (int i = 0; i < 1000; i++) ge.process(0.1f);
        float tGran = benchmarkUs([&](int) { ge.process(0.5f); }, N);
        Serial.printf("  Granular:     %.2f us\n", tGran);
        totalWorstCase += tGran;
    }

    // --- Reverb ---
    {
        FDNReverb rv;
        rv.setEnabled(true);
        rv.setDecay(3.0f);
        rv.setMix(0.5f);
        float ol, or2;
        float tRev = benchmarkUs([&](int) { rv.processStereo(0.5f, 0.3f, ol, or2); }, N);
        Serial.printf("  Reverb FDN:   %.2f us\n", tRev);
        totalWorstCase += tRev;
    }

    // --- Compressor ---
    {
        Compressor comp;
        comp.setEnabled(true);
        comp.setThreshold(-12.0f);
        float ol, or2;
        float tComp = benchmarkUs([&](int) { comp.processStereo(0.5f, 0.3f, ol, or2); }, N);
        Serial.printf("  Compressor:   %.2f us\n", tComp);
        totalWorstCase += tComp;
    }

    // --- Summary ---
    float worstPct = (totalWorstCase / perSampleBudget) * 100.0f;
    Serial.printf("\n  ============================================\n");
    Serial.printf("  WORST CASE TOTAL: %.2f us / %.2f us budget\n", totalWorstCase, perSampleBudget);
    Serial.printf("  CPU LOAD:         %.1f%%\n", worstPct);
    if (worstPct > 90.0f) {
        Serial.printf("  *** DANGER: >90%% CPU - will crash under load! ***\n");
        Serial.printf("  *** Consider: fewer voices, skip effects, reduce supersaw ***\n");
    } else if (worstPct > 75.0f) {
        Serial.printf("  ** WARNING: >75%% CPU - may glitch with all features active **\n");
    } else {
        Serial.printf("  OK: Headroom looks sufficient.\n");
    }
    Serial.printf("  ============================================\n");
}

// ============================================================================
// FULL CHAIN WORST CASE - Simulates the actual processBlock path
// ============================================================================

void SynthesisTests::testFullChainWorstCase() {
    Serial.print("Full chain worst-case (4 supersaw voices + all FX)... ");
    
    const float blockDurationUs = (DMA_BUFFER_SAMPLES * 1000000.0f) / SAMPLE_RATE;
    
    // Create 4 voices with supersaw (most expensive waveform)
    Voice voices[4];
    GlobalVoiceParams params;
    params.oscWaveforms[0] = Waveform::SUPERSAW;
    params.oscWaveforms[1] = Waveform::SUPERSAW;
    params.oscDetune[0] = 0.0f;
    params.oscDetune[1] = 7.0f;
    params.oscCoarse[0] = 0;
    params.oscCoarse[1] = 0;
    params.oscSupersawDetune[0] = 0.5f;
    params.oscSupersawDetune[1] = 0.5f;
    params.oscMix = 0.5f;
    params.pulseWidth[0] = 0.5f;
    params.pulseWidth[1] = 0.5f;
    params.morph[0] = 0.0f;
    params.morph[1] = 0.0f;
    params.synthMode = VoiceSynthMode::STANDARD;
    params.fmAmount = 1.0f;
    params.filterType = VoiceFilterType::LADDER;
    params.filterCutoff = 2000.0f;
    params.filterReso = 0.7f;
    params.filterMode = FilterMode::LOWPASS;
    params.filterEnvAmount = 0.5f;
    params.filterEnvVelocity = 0.5f;
    params.filterKeyTracking = 0.5f;
    params.ampA = 0.01f; params.ampD = 0.1f; params.ampS = 0.7f; params.ampR = 0.3f;
    params.fltA = 0.01f; params.fltD = 0.2f; params.fltS = 0.3f; params.fltR = 0.5f;
    params.glideTime = 0.0f;
    params.legato = false;

    uint8_t notes[] = {48, 55, 60, 67};
    for (int v = 0; v < 4; v++) {
        voices[v].applyParams(params);
        voices[v].noteOn(notes[v], 127, false);
    }

    // Effects chain
    EffectsChain effects;
    effects.setEnabled(true, true, true); // sat + chorus + delay
    effects.saturation.setDrive(3.0f);
    effects.chorus.setDepth(0.5f);
    effects.delay.setTime(0.1f);
    effects.delay.setFeedback(0.5f);

    ResonatorBank reso;
    reso.setFrequency(110.0f);
    reso.setResonance(20.0f);
    reso.setMix(0.3f);

    CombFilter comb;
    comb.setPitch(110.0f);
    comb.setFeedback(0.7f);
    comb.setMix(0.3f);

    GranularExciter gran;
    gran.setDensity(30.0f);
    gran.setDuration(50.0f);
    gran.setFreeRunning(true);

    FDNReverb reverb;
    reverb.setEnabled(true);
    reverb.setDecay(3.0f);
    reverb.setMix(0.3f);

    Compressor comp;
    comp.setEnabled(true);
    comp.setThreshold(-12.0f);
    comp.setRatio(4.0f);

    // Run the full chain for multiple blocks and measure
    uint32_t maxBlockTime = 0;
    uint32_t totalBlockTime = 0;
    const int numBlocks = 100;

    for (int block = 0; block < numBlocks; block++) {
        uint32_t t0 = micros();

        // Feed watchdog every few blocks
        if (block % 10 == 0) {
            esp_task_wdt_reset();
            vTaskDelay(1);
        }

        for (int i = 0; i < DMA_BUFFER_SAMPLES; i++) {
            float left = 0.0f, right = 0.0f;
            for (int v = 0; v < 4; v++) {
                if (voices[v].isActive()) {
                    float s = voices[v].process();
                    left += s * 0.5f;
                    right += s * 0.5f;
                }
            }
            left *= 0.5f;
            right *= 0.5f;

            // Granular
            float mono = (left + right) * 0.5f;
            float granOut = gran.process(mono);
            left = granOut;
            right = granOut;

            // Resonator + Comb
            mono = (left + right) * 0.5f;
            float resoOut = reso.process(mono);
            left += resoOut - mono;
            right += resoOut - mono;

            mono = (left + right) * 0.5f;
            float combOut = comb.process(mono);
            left += combOut - mono;
            right += combOut - mono;

            // Effects chain
            effects.processStereo(left, right);

            // Reverb
            float wl, wr;
            reverb.processStereo(left, right, wl, wr);

            // Compressor
            float cl, cr;
            comp.processStereo(wl, wr, cl, cr);

            // Output (just store to prevent optimization)
            volatile int16_t outL = (int16_t)(cl * 32767.0f);
            volatile int16_t outR = (int16_t)(cr * 32767.0f);
            (void)outL; (void)outR;
        }

        uint32_t elapsed = micros() - t0;
        totalBlockTime += elapsed;
        if (elapsed > maxBlockTime) maxBlockTime = elapsed;
    }

    float avgBlock = (float)totalBlockTime / numBlocks;
    float avgPct = (avgBlock / blockDurationUs) * 100.0f;
    float maxPct = ((float)maxBlockTime / blockDurationUs) * 100.0f;

    Serial.printf("\n    Avg block: %.0f us (%.1f%%)\n", avgBlock, avgPct);
    Serial.printf("    Max block: %lu us (%.1f%%)\n", maxBlockTime, maxPct);
    
    if (maxPct > 100.0f) {
        Serial.printf("    *** FAIL: Block overrun detected! Max %.1f%% > 100%% ***\n", maxPct);
        Serial.println("    *** This WILL cause watchdog crashes under load ***");
    } else if (maxPct > 85.0f) {
        Serial.printf("    ** WARNING: Tight margin (%.1f%%). Spikes will cause glitches **\n", maxPct);
    } else {
        Serial.println("    PASSED");
    }
}

// ============================================================================
// NaN PROPAGATION TEST
// ============================================================================

void SynthesisTests::testNaNPropagation() {
    Serial.print("NaN/Inf propagation test... ");
    bool passed = true;
    
    float nan = 0.0f / 0.0f;
    float inf = 1.0f / 0.0f;
    float negInf = -1.0f / 0.0f;
    
    // Test each effect with NaN/Inf input
    auto checkOutput = [&](const char* name, float output) {
        if (isnan(output) || isinf(output)) {
            Serial.printf("\n    FAIL: %s produced NaN/Inf from bad input", name);
            passed = false;
        }
    };

    // Filter
    {
        Filter f;
        f.setCutoff(1000.0f);
        f.setResonance(0.9f);
        checkOutput("SVF(NaN)", f.process(nan));
        f.reset();
        checkOutput("SVF(Inf)", f.process(inf));
    }

    // Moog
    {
        LadderFilter f;
        f.setCutoff(1000.0f);
        f.setResonance(0.9f);
        checkOutput("Moog(NaN)", f.process(nan));
        f.reset();
        checkOutput("Moog(Inf)", f.process(inf));
    }

    // Resonator
    {
        ResonatorBank rb;
        rb.setFrequency(110.0f);
        rb.setResonance(30.0f);
        rb.setMix(1.0f);
        checkOutput("Resonator(NaN)", rb.process(nan));
        rb.reset();
        checkOutput("Resonator(Inf)", rb.process(inf));
    }

    // Comb
    {
        CombFilter cb;
        cb.setPitch(110.0f);
        cb.setFeedback(0.9f);
        checkOutput("Comb(NaN)", cb.process(nan));
        cb.reset();
        checkOutput("Comb(Inf)", cb.process(inf));
    }

    // Reverb
    {
        FDNReverb rv;
        rv.setEnabled(true);
        rv.setDecay(5.0f);
        rv.setMix(1.0f);
        float ol, or2;
        rv.processStereo(nan, nan, ol, or2);
        checkOutput("Reverb L(NaN)", ol);
        checkOutput("Reverb R(NaN)", or2);
        rv.reset();
        rv.processStereo(inf, negInf, ol, or2);
        checkOutput("Reverb L(Inf)", ol);
        checkOutput("Reverb R(Inf)", or2);
    }

    // Compressor
    {
        Compressor comp;
        comp.setEnabled(true);
        float ol, or2;
        comp.processStereo(nan, nan, ol, or2);
        checkOutput("Compressor(NaN)", ol);
        comp.processStereo(inf, inf, ol, or2);
        checkOutput("Compressor(Inf)", ol);
    }

    // Granular processor mode
    {
        GranularExciter ge;
        ge.setDensity(20.0f);
        ge.setFreeRunning(true);
        checkOutput("Granular(NaN)", ge.process(nan));
        checkOutput("Granular(Inf)", ge.process(inf));
    }

    Serial.println(passed ? "PASSED" : "\n    One or more stages leaked NaN/Inf!");
}

// ============================================================================
// VOICE LIFECYCLE - Checks for zombie voices
// ============================================================================

void SynthesisTests::testVoiceLifecycle() {
    Serial.print("Voice lifecycle test... ");
    bool passed = true;
    
    Voice v;
    GlobalVoiceParams p;
    p.oscWaveforms[0] = Waveform::SAW;
    p.oscWaveforms[1] = Waveform::SAW;
    p.oscDetune[0] = 0.0f;
    p.oscDetune[1] = 7.0f;
    p.oscCoarse[0] = 0;
    p.oscCoarse[1] = 0;
    p.oscSupersawDetune[0] = 0.5f;
    p.oscSupersawDetune[1] = 0.5f;
    p.oscMix = 0.5f;
    p.pulseWidth[0] = 0.5f;
    p.pulseWidth[1] = 0.5f;
    p.morph[0] = 0.0f;
    p.morph[1] = 0.0f;
    p.synthMode = VoiceSynthMode::STANDARD;
    p.fmAmount = 1.0f;
    p.filterType = VoiceFilterType::SVF;
    p.filterCutoff = 2000.0f;
    p.filterReso = 0.3f;
    p.filterMode = FilterMode::LOWPASS;
    p.filterEnvAmount = 0.5f;
    p.filterEnvVelocity = 0.5f;
    p.filterKeyTracking = 0.0f;
    p.ampA = 0.001f;
    p.ampD = 0.01f;
    p.ampS = 0.5f;
    p.ampR = 0.05f; // Short release - 50ms
    p.fltA = 0.001f;
    p.fltD = 0.01f;
    p.fltS = 0.3f;
    p.fltR = 0.05f;
    p.glideTime = 0.0f;
    p.legato = false;

    v.applyParams(p);

    // Test 1: Note on → process → note off → should eventually go FREE
    v.noteOn(60, 100, false);
    
    // Process through attack+decay
    for (int i = 0; i < 4800; i++) {
        v.process();
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
    }
    
    if (!v.isActive()) {
        Serial.printf("\n    FAIL: Voice died during sustain");
        passed = false;
    }

    v.noteOff();

    // Process through release (50ms = 2400 samples, add margin)
    int samplesAfterOff = 0;
    while (v.isActive() && samplesAfterOff < 48000) { // 1 second max
        v.process();
        samplesAfterOff++;
        if ((samplesAfterOff & 0x3FF) == 0) esp_task_wdt_reset();
    }

    if (v.isActive()) {
        Serial.printf("\n    FAIL: Voice still active after %d samples post-noteOff (zombie!)", samplesAfterOff);
        passed = false;
    } else {
        // Good - check the output is actually silent
        float silence = v.process();
        if (fabsf(silence) > 0.0001f) {
            Serial.printf("\n    FAIL: FREE voice producing output: %.6f", silence);
            passed = false;
        }
    }

    // Test 2: Rapid retrigger (voice stealing scenario)
    for (int cycle = 0; cycle < 50; cycle++) {
        esp_task_wdt_reset();
        v.applyParams(p);
        v.noteOn(48 + (cycle % 24), 127, false);
        for (int i = 0; i < 480; i++) v.process(); // 10ms
        v.noteOff();
        for (int i = 0; i < 240; i++) v.process(); // 5ms (won't finish release)
        v.forceOff(); // Simulate voice steal
    }
    
    // After forceOff, must be FREE
    if (v.isActive()) {
        Serial.printf("\n    FAIL: Voice active after forceOff");
        passed = false;
    }

    Serial.println(passed ? "PASSED" : "");
}

// ============================================================================
// RESONATOR PROFILES - Verifies harmonic noticeability
// ============================================================================

void SynthesisTests::testResonatorProfiles() {
    Serial.print("Testing Resonator profiles... ");
    ResonatorBank rb;
    rb.setMix(1.0f);
    rb.setResonance(30.0f);
    rb.setFrequency(100.0f);

    bool passed = true;
    for (int p = 0; p < (int)ResonatorProfile::NUM_PROFILES; p++) {
        rb.setProfile((ResonatorProfile)p);
        rb.reset();

        // Excite with impulse and measure energy
        float energy = 0.0f;
        for (int i = 0; i < 4800; i++) {
            float in = (i == 0) ? 1.0f : 0.0f;
            float out = rb.process(in);
            energy += out * out;
            if (SAFE_CHECK(out)) {
                passed = false;
                break;
            }
        }

        if (energy < 0.001f) {
            Serial.printf("\n    FAIL: Profile %d has no output energy", p);
            passed = false;
        }
    }
    Serial.println(passed ? "PASSED" : "");
}

// ============================================================================
// SPECTRAL STABILITY - Checks for high-Q runaway
// ============================================================================

void SynthesisTests::testSpectralStability() {
    Serial.print("Testing Spectral stability (High-Q sweep)... ");
    ResonatorBank rb;
    rb.setMix(1.0f);
    rb.setResonance(50.0f); // Max resonance

    bool passed = true;
    for (float f = 100.0f; f < 2000.0f; f += 200.0f) {
        esp_task_wdt_reset();
        rb.setFrequency(f);
        for (int i = 0; i < 1000; i++) {
            float in = fastRandFloat01(*(uint32_t*)&f); // Pseudo-random
            float out = rb.process(in);
            if (SAFE_CHECK(out) || fabsf(out) > 2.0f) {
                passed = false;
                break;
            }
        }
    }
    Serial.println(passed ? "PASSED" : "");
}

// ============================================================================
// FEEDBACK ACCUMULATION - Tests for runaway feedback
// ============================================================================

void SynthesisTests::testFeedbackAccumulation() {
    Serial.print("Feedback accumulation test (extreme settings)... ");
    bool passed = true;

    // Resonator at extreme Q
    {
        ResonatorBank rb;
        rb.setFrequency(110.0f);
        rb.setResonance(50.0f); // Max Q
        rb.setDamping(0.01f);   // Very low damping
        rb.setMix(1.0f);

        float maxAbs = 0.0f;
        for (int i = 0; i < 100000; i++) {
            if ((i & 0x3FF) == 0) esp_task_wdt_reset();
            float in = (i % 48000 == 0) ? 1.0f : 0.0f;
            float out = rb.process(in);
            if (isnan(out) || isinf(out)) {
                Serial.printf("\n    FAIL: Resonator exploded at sample %d", i);
                passed = false;
                break;
            }
            float a = fabsf(out);
            if (a > maxAbs) maxAbs = a;
            if (a > 100.0f) {
                Serial.printf("\n    FAIL: Resonator output runaway: %.1f at sample %d", out, i);
                passed = false;
                break;
            }
        }
        if (passed) Serial.printf("(Reso peak: %.2f) ", maxAbs);
    }

    // Comb at extreme feedback
    {
        CombFilter cb;
        cb.setPitch(55.0f);
        cb.setFeedback(0.99f);
        cb.setDamping(0.0f);
        cb.setMix(1.0f);

        float maxAbs = 0.0f;
        for (int i = 0; i < 100000; i++) {
            if ((i & 0x3FF) == 0) esp_task_wdt_reset();
            float in = (i == 0) ? 1.0f : 0.0f;
            float out = cb.process(in);
            if (isnan(out) || isinf(out)) {
                Serial.printf("\n    FAIL: Comb exploded at sample %d", i);
                passed = false;
                break;
            }
            float a = fabsf(out);
            if (a > maxAbs) maxAbs = a;
        }
        if (passed) Serial.printf("(Comb peak: %.2f) ", maxAbs);
    }

    // Stacked: resonator → comb → reverb with feedback
    {
        ResonatorBank rb;
        rb.setFrequency(110.0f);
        rb.setResonance(30.0f);
        rb.setMix(0.7f);

        CombFilter cb;
        cb.setPitch(110.0f);
        cb.setFeedback(0.9f);
        cb.setMix(0.5f);

        FDNReverb rv;
        rv.setEnabled(true);
        rv.setDecay(5.0f);
        rv.setMix(0.5f);

        for (int i = 0; i < 100000; i++) {
            if ((i & 0x3FF) == 0) esp_task_wdt_reset();
            float in = (i % 48000 == 0) ? 0.5f : 0.0f;
            float x = rb.process(in);
            x = cb.process(x);
            float l, r;
            rv.processStereo(x, x, l, r);
            
            if (isnan(l) || isinf(l) || fabsf(l) > 50.0f) {
                Serial.printf("\n    FAIL: Stacked chain exploded at sample %d (val: %.1f)", i, l);
                passed = false;
                break;
            }
        }
    }

    Serial.println(passed ? "PASSED" : "");
}

// ============================================================================
// MEMORY PRESSURE
// ============================================================================

void SynthesisTests::testMemoryPressure() {
    Serial.println("Memory pressure report:");
    
    size_t freeHeap = ESP.getFreeHeap();
    size_t minFreeHeap = ESP.getMinFreeHeap();
    size_t totalPsram = ESP.getPsramSize();
    size_t freePsram = ESP.getFreePsram();
    
    Serial.printf("  Free heap:     %u bytes (min ever: %u)\n", freeHeap, minFreeHeap);
    if (totalPsram > 0) {
        Serial.printf("  PSRAM:         %u / %u bytes free\n", freePsram, totalPsram);
    }
    
    // Check stack high-water mark for current task
    UBaseType_t stackHWM = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("  Stack HWM:     %u bytes remaining (current task)\n", stackHWM * 4);
    
    // Estimate SynthEngine size
    Serial.printf("  sizeof(Voice):         %u bytes\n", sizeof(Voice));
    Serial.printf("  sizeof(FDNReverb):     %u bytes\n", sizeof(FDNReverb));
    Serial.printf("  sizeof(EffectsChain):  %u bytes\n", sizeof(EffectsChain));
    Serial.printf("  sizeof(ResonatorBank): %u bytes\n", sizeof(ResonatorBank));
    Serial.printf("  sizeof(GranularExciter): %u bytes\n", sizeof(GranularExciter));
    
    if (freeHeap < 20000) {
        Serial.println("  *** WARNING: Very low heap! Allocations may fail ***");
    }
    if (stackHWM < 512) {
        Serial.println("  *** WARNING: Stack nearly full! Risk of stack overflow crash ***");
    }
    Serial.println("  OK");
}

// ============================================================================
// GRANULAR PROCESSOR MODE
// ============================================================================

void SynthesisTests::testGranularProcessor() {
    Serial.print("Granular processor mode... ");
    bool passed = true;

    GranularExciter ge;
    ge.setDensity(30.0f);
    ge.setDuration(50.0f);
    ge.setFreeRunning(true);
    ge.setGranularMix(0.5f);

    // Feed it a sine wave and check output isn't silent or exploded
    float maxOutput = 0.0f;
    bool anyNonZero = false;
    
    for (int i = 0; i < 96000; i++) { // 2 seconds
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float input = 0.5f * sinf(i * 2.0f * M_PI * 440.0f / SAMPLE_RATE);
        float out = ge.process(input);
        
        if (isnan(out) || isinf(out)) {
            Serial.printf("\n    FAIL: NaN/Inf at sample %d", i);
            passed = false;
            break;
        }
        
        float a = fabsf(out);
        if (a > maxOutput) maxOutput = a;
        if (a > 0.001f) anyNonZero = true;
        
        if (a > 5.0f) {
            Serial.printf("\n    FAIL: Output explosion: %.2f at sample %d", out, i);
            passed = false;
            break;
        }
    }

    if (passed && !anyNonZero) {
        Serial.printf("\n    FAIL: Granular produced only silence");
        passed = false;
    }

    if (passed) {
        Serial.printf("peak=%.3f ", maxOutput);
    }
    Serial.println(passed ? "PASSED" : "");
}

// ============================================================================
// REVERB FREEZE
// ============================================================================

void SynthesisTests::testReverbFreeze() {
    Serial.print("Reverb freeze stability... ");
    bool passed = true;

    FDNReverb rv;
    rv.setEnabled(true);
    rv.setDecay(2.0f);
    rv.setMix(1.0f);

    // Feed some signal
    for (int i = 0; i < 24000; i++) {
        float in = (i < 4800) ? (0.5f * sinf(i * 0.1f)) : 0.0f;
        float l, r;
        rv.processStereo(in, in, l, r);
    }

    // Engage freeze
    rv.setFreeze(true);

    // Run for 5 seconds frozen - output should sustain and not explode
    float firstFrozenLevel = 0.0f;
    bool gotLevel = false;
    
    for (int i = 0; i < 240000; i++) { // 5 seconds
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float l, r;
        rv.processStereo(0.0f, 0.0f, l, r);
        
        if (isnan(l) || isinf(l)) {
            Serial.printf("\n    FAIL: Frozen reverb produced NaN/Inf at sample %d", i);
            passed = false;
            break;
        }

        float level = fabsf(l) + fabsf(r);
        
        // Capture initial frozen level
        if (!gotLevel && i > 1000) {
            firstFrozenLevel = level;
            gotLevel = true;
        }

        // Check it hasn't grown significantly (max 3dB growth = 1.4x)
        if (gotLevel && level > firstFrozenLevel * 1.5f + 0.01f) {
            Serial.printf("\n    FAIL: Frozen reverb growing! Initial: %.4f, Now: %.4f at sample %d",
                          firstFrozenLevel, level, i);
            passed = false;
            break;
        }
    }

    // Check frozen level isn't silent
    if (passed && firstFrozenLevel < 0.001f) {
        Serial.printf("\n    FAIL: Frozen reverb is silent (level: %.6f)", firstFrozenLevel);
        passed = false;
    }

    // Disengage freeze - should decay normally
    rv.setFreeze(false);
    float postThawLevel = 0.0f;
    for (int i = 0; i < 240000; i++) {
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float l, r;
        rv.processStereo(0.0f, 0.0f, l, r);
        postThawLevel = fabsf(l) + fabsf(r);
    }
    
    if (postThawLevel > 0.01f) {
        Serial.printf("\n    WARNING: Reverb tail still audible after 5s post-thaw: %.4f", postThawLevel);
    }

    Serial.println(passed ? "PASSED" : "");
}

// ============================================================================
// ORIGINAL TESTS (preserved)
// ============================================================================

void SynthesisTests::testReverbStress() {
    Serial.print("Stress testing Reverb... ");
    FDNReverb* rv = new FDNReverb();
    rv->setEnabled(true);
    rv->setDecay(5.0f);
    rv->setSize(0.9f);
    rv->setMix(1.0f);

    bool passed = true;
    for (int i = 0; i < 100000; i++) {
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float in = (i % 48000 == 0) ? 1.0f : 0.0f;
        float out = rv->process(in);
        if (isnan(out) || isinf(out)) {
            passed = false;
            break;
        }
    }
    delete rv;
    Serial.println(passed ? "PASSED" : "FAILED (Instability detected)");
}

void SynthesisTests::testResonatorStress() {
    Serial.print("Stress testing Resonator... ");
    ResonatorBank* rb = new ResonatorBank();
    rb->setFrequency(110.0f);
    rb->setResonance(40.0f);
    rb->setDamping(0.01f);
    rb->setMix(1.0f);

    bool passed = true;
    for (int i = 0; i < 100000; i++) {
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float in = (float)rand() / RAND_MAX * 0.1f;
        float out = rb->process(in);
        if (isnan(out) || isinf(out) || fabsf(out) > 10.0f) {
            passed = false;
            break;
        }
    }
    delete rb;
    Serial.println(passed ? "PASSED" : "FAILED (Exploded)");
}

void SynthesisTests::testCombStress() {
    Serial.print("Stress testing Comb... ");
    CombFilter* cb = new CombFilter();
    cb->setPitch(55.0f);
    cb->setFeedback(0.99f);
    cb->setMix(1.0f);

    bool passed = true;
    for (int i = 0; i < 100000; i++) {
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float in = (i == 0) ? 1.0f : 0.0f;
        float out = cb->process(in);
        if (isnan(out) || isinf(out)) {
            passed = false;
            break;
        }
    }
    delete cb;
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testGranularStress() {
    Serial.print("Stress testing Granular... ");
    GranularExciter* ge = new GranularExciter();
    ge->setDensity(100.0f);
    ge->setDuration(200.0f);
    ge->setFreeRunning(true);

    bool passed = true;
    for (int i = 0; i < 100000; i++) {
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float out = ge->process();
        if (isnan(out) || isinf(out)) {
            passed = false;
            break;
        }
    }
    delete ge;
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testSaturationStress() {
    Serial.print("Stress testing Saturation... ");
    Saturation* sat = new Saturation();
    sat->setDrive(10.0f);
    sat->setType(SaturationType::FOLDBACK);

    bool passed = true;
    for (int i = 0; i < 100000; i++) {
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float in = (float)rand() / RAND_MAX * 10.0f;
        float out = sat->process(in);
        if (isnan(out) || isinf(out)) {
            passed = false;
            break;
        }
    }
    delete sat;
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testChorusStress() {
    Serial.print("Stress testing Chorus... ");
    Chorus* cho = new Chorus();
    cho->setDepth(1.0f);
    cho->setMix(1.0f);

    bool passed = true;
    for (int i = 0; i < 100000; i++) {
        if ((i & 0x3FF) == 0) esp_task_wdt_reset();
        float in = (float)rand() / RAND_MAX;
        float out = cho->process(in);
        if (isnan(out) || isinf(out)) {
            passed = false;
            break;
        }
    }
    delete cho;
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testModMatrix() {
    Serial.print("Testing ModMatrix... ");
    ModMatrix mm;
    mm.setSourceValue(ModSource::LFO1, 0.5f);
    mm.setSlot(0, ModSource::LFO1, ModDest::FILTER_CUTOFF, 1.0f);
    mm.process();
    float val = mm.getModulation(ModDest::FILTER_CUTOFF);

    bool passed = (fabsf(val - 0.5f) < 0.001f);
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testOscillator() {
    Serial.print("Testing Oscillator... ");
    Oscillator osc;
    osc.setFrequency(440.0f);
    osc.setWaveform(Waveform::SINE);

    bool passed = true;
    for (int i = 0; i < 1000; i++) {
        float sample = osc.process();
        if (isnan(sample) || isinf(sample) || sample < -1.1f || sample > 1.1f) {
            passed = false;
            break;
        }
    }
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testFM() {
    Serial.print("Testing FM synthesis... ");
    Oscillator carrier;
    Oscillator modulator;
    carrier.setFrequency(440.0f);
    modulator.setFrequency(220.0f);

    bool passed = true;
    for (int i = 0; i < 1000; i++) {
        float modOut = modulator.process();
        float sample = carrier.processWithFM(modOut, 10.0f);
        if (isnan(sample) || isinf(sample) || fabsf(sample) > 1.1f) {
            passed = false;
            break;
        }
    }
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testSync() {
    Serial.print("Testing Oscillator Sync... ");
    SyncOsc sync;
    sync.setMasterFreq(100.0f);
    sync.setSlaveFreq(250.0f);

    bool passed = true;
    for (int i = 0; i < 1000; i++) {
        float sample = sync.process();
        if (isnan(sample) || isinf(sample) || fabsf(sample) > 1.1f) {
            passed = false;
            break;
        }
    }
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testRingMod() {
    Serial.print("Testing Ring Modulation... ");
    RingMod rm;
    rm.setFrequency(1000.0f);

    bool passed = true;
    for (int i = 0; i < 1000; i++) {
        float sample = rm.process(0.5f * sinf(i * 0.1f));
        if (isnan(sample) || isinf(sample) || fabsf(sample) > 1.1f) {
            passed = false;
            break;
        }
    }
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testFilter() {
    Serial.print("Testing SVF Filter... ");
    Filter filter;
    filter.setCutoff(1000.0f);
    filter.setResonance(0.5f);

    bool passed = true;
    for (int i = 0; i < 1000; i++) {
        float sample = filter.process(0.5f);
        if (isnan(sample) || isinf(sample) || fabsf(sample) > 10.0f) {
            passed = false;
            break;
        }
    }
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testMoogFilter() {
    Serial.print("Testing Moog Ladder Filter... ");
    LadderFilter filter;
    filter.setCutoff(1000.0f);
    filter.setResonance(0.5f);

    bool passed = true;
    for (int i = 0; i < 1000; i++) {
        float sample = filter.process(0.5f);
        if (isnan(sample) || isinf(sample) || fabsf(sample) > 10.0f) {
            passed = false;
            break;
        }
    }
    Serial.println(passed ? "PASSED" : "FAILED");
}

void SynthesisTests::testEnvelope() {
    Serial.print("Testing Envelope... ");
    Envelope env;
    env.setADSR(0.01f, 0.1f, 0.5f, 0.1f);

    bool passed = true;
    env.gate(true);
    for (int i = 0; i < 1000; i++) {
        float val = env.process();
        if (isnan(val) || isinf(val) || val < 0.0f || val > 1.0f) {
            passed = false;
            break;
        }
    }
    Serial.println(passed ? "PASSED" : "FAILED");
}
