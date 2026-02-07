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
#include <Arduino.h>

void SynthesisTests::runAll() {
    Serial.println("\n--- RUNNING SYNTHESIS TESTS ---");
    testOscillator();
    testFilter();
    testMoogFilter();
    testEnvelope();
    testFM();
    testSync();
    testRingMod();
    testModMatrix();
    testMorph();

    Serial.println("\n--- RUNNING STRESS TESTS (100k samples) ---");
    testSaturationStress();
    testChorusStress();
    testReverbStress();
    testResonatorStress();
    testCombStress();
    testGranularStress();

    Serial.println("--- ALL TESTS COMPLETED ---\n");
}

void SynthesisTests::testReverbStress() {
    Serial.print("Stress testing Reverb... ");
    FDNReverb* rv = new FDNReverb();
    rv->setEnabled(true);
    rv->setDecay(5.0f);
    rv->setSize(0.9f);
    rv->setMix(1.0f);

    bool passed = true;
    for (int i = 0; i < 100000; i++) {
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

void SynthesisTests::testMorph() {
    Serial.print("Testing Wavetable Morph... ");
    uint32_t phase = 0;
    bool passed = true;
    for (float m = 0.0f; m <= 1.0f; m += 0.1f) {
        float sample = Wavetables::readMorph(phase, m, 2);
        if (isnan(sample) || isinf(sample) || sample < -1.1f || sample > 1.1f) {
            passed = false;
            break;
        }
        phase += 1000000;
    }
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
        float in = (float)rand() / RAND_MAX * 10.0f; // High gain
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
