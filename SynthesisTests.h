#ifndef SYNTHESIS_TESTS_H
#define SYNTHESIS_TESTS_H

#include <Arduino.h>

class SynthesisTests {
public:
    static void runAll();

    // Original unit tests
    static void testOscillator();
    static void testFilter();
    static void testMoogFilter();
    static void testEnvelope();
    static void testFM();
    static void testSync();
    static void testRingMod();
    static void testModMatrix();

    // Original stress tests
    static void testReverbStress();
    static void testResonatorStress();
    static void testCombStress();
    static void testGranularStress();
    static void testChorusStress();
    static void testSaturationStress();

    // === NEW: Load & crash diagnostic tests ===
    
    // Measures per-sample cost of each DSP component in microseconds
    // Prints a budget breakdown showing what fits in a 128-sample block
    static void testTimingBudget();

    // Simulates worst-case: 4 voices x supersaw + all effects enabled
    // Reports whether block completes within deadline
    static void testFullChainWorstCase();

    // Tests NaN/Inf propagation through the full effects chain
    // Injects bad values at each stage and checks output
    static void testNaNPropagation();

    // Runs voices to completion and checks for leaked state
    // (voices stuck ACTIVE after noteOff + release)
    static void testVoiceLifecycle();

    // Stacks resonator + comb + reverb at extreme settings
    // Checks for runaway feedback and filter explosion
    static void testFeedbackAccumulation();

    // Measures free heap and stack high-water mark
    static void testMemoryPressure();

    // Tests granular processor mode specifically
    static void testGranularProcessor();

    // Tests reverb freeze stability
    static void testReverbFreeze();

    // Verifies all resonator profiles are audible and distinct
    static void testResonatorProfiles();

    // Checks spectral modules for stability under high resonance sweeps
    static void testSpectralStability();
};

#endif
