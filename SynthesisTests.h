#ifndef SYNTHESIS_TESTS_H
#define SYNTHESIS_TESTS_H

#include "Config.h"
#include "Oscillator.h"
#include "Filter.h"
#include "MoogFilter.h"
#include "Envelope.h"
#include "ModMatrix.h"
#include "Synthesis.h"
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
        Serial.println("--- ALL TESTS COMPLETED ---\n");
    }

private:
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
