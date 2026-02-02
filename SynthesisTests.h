#ifndef SYNTHESIS_TESTS_H
#define SYNTHESIS_TESTS_H

#include "Config.h"
#include "Oscillator.h"
#include "Filter.h"
#include "MoogFilter.h"
#include "Envelope.h"
#include "ModMatrix.h"
#include "Synthesis.h"
#include "Reverb.h"
#include "Resonator.h"
#include "CombFilter.h"
#include "Granular.h"
#include <Arduino.h>

class SynthesisTests {
public:
    static void runAll() {
        Serial.println("\n--- RUNNING SYNTHESIS TESTS ---");
        testOscillator();
        testFilter();
        testMoogFilter();
        testEnvelope();
        testFM();
        testSync();
        testRingMod();
        testModMatrix();

        Serial.println("\n--- RUNNING STRESS TESTS (100k samples) ---");
        testReverbStress();
        testResonatorStress();
        testCombStress();
        testGranularStress();

        Serial.println("--- ALL TESTS COMPLETED ---\n");
    }

private:
    static void testReverbStress() {
        Serial.print("Stress testing Reverb... ");
        FDNReverb rv;
        rv.setEnabled(true);
        rv.setDecay(5.0f);
        rv.setSize(0.9f);
        rv.setMix(1.0f);

        bool passed = true;
        for (int i = 0; i < 100000; i++) {
            float in = (i % 48000 == 0) ? 1.0f : 0.0f; // Impulse every second
            float out = rv.process(in);
            if (isnan(out) || isinf(out)) {
                passed = false;
                break;
            }
        }
        Serial.println(passed ? "PASSED" : "FAILED (Instability detected)");
    }

    static void testResonatorStress() {
        Serial.print("Stress testing Resonator... ");
        ResonatorBank rb;
        rb.setFrequency(110.0f);
        rb.setResonance(40.0f);
        rb.setDamping(0.01f);
        rb.setMix(1.0f);

        bool passed = true;
        for (int i = 0; i < 100000; i++) {
            float in = (float)rand() / RAND_MAX * 0.1f; // White noise
            float out = rb.process(in);
            if (isnan(out) || isinf(out) || fabsf(out) > 10.0f) {
                passed = false;
                break;
            }
        }
        Serial.println(passed ? "PASSED" : "FAILED (Exploded)");
    }

    static void testCombStress() {
        Serial.print("Stress testing Comb... ");
        CombFilter cb;
        cb.setPitch(55.0f);
        cb.setFeedback(0.99f);
        cb.setMix(1.0f);

        bool passed = true;
        for (int i = 0; i < 100000; i++) {
            float in = (i == 0) ? 1.0f : 0.0f;
            float out = cb.process(in);
            if (isnan(out) || isinf(out)) {
                passed = false;
                break;
            }
        }
        Serial.println(passed ? "PASSED" : "FAILED");
    }

    static void testGranularStress() {
        Serial.print("Stress testing Granular... ");
        GranularExciter ge;
        ge.setDensity(100.0f);
        ge.setDuration(200.0f);
        ge.setFreeRunning(true);

        bool passed = true;
        for (int i = 0; i < 100000; i++) {
            float out = ge.process();
            if (isnan(out) || isinf(out)) {
                passed = false;
                break;
            }
        }
        Serial.println(passed ? "PASSED" : "FAILED");
    }

    static void testModMatrix() {
        Serial.print("Testing ModMatrix... ");
        ModMatrix mm;
        mm.setSourceValue(ModSource::LFO1, 0.5f);
        mm.setSlot(0, ModSource::LFO1, ModDest::FILTER_CUTOFF, 1.0f);
        mm.process();
        float val = mm.getModulation(ModDest::FILTER_CUTOFF);

        bool passed = (fabsf(val - 0.5f) < 0.001f);
        Serial.println(passed ? "PASSED" : "FAILED");
    }

    static void testOscillator() {
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

    static void testFM() {
        Serial.print("Testing FM synthesis... ");
        Oscillator carrier;
        Oscillator modulator;
        carrier.setFrequency(440.0f);
        modulator.setFrequency(220.0f);

        bool passed = true;
        for (int i = 0; i < 1000; i++) {
            float modOut = modulator.process();
            float sample = carrier.processWithFM(modOut, 10.0f); // Deep FM
            if (isnan(sample) || isinf(sample) || fabsf(sample) > 1.1f) {
                passed = false;
                break;
            }
        }
        Serial.println(passed ? "PASSED" : "FAILED");
    }

    static void testSync() {
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

    static void testRingMod() {
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

    static void testFilter() {
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

    static void testMoogFilter() {
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

    static void testEnvelope() {
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
};

#endif
