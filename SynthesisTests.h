#ifndef SYNTHESIS_TESTS_H
#define SYNTHESIS_TESTS_H

class SynthesisTests {
public:
    static void runAll();

private:
    static void testOscillator();
    static void testFilter();
    static void testMoogFilter();
    static void testEnvelope();
    static void testFM();
    static void testSync();
    static void testRingMod();
    static void testModMatrix();
    static void testReverbStress();
    static void testResonatorStress();
    static void testCombStress();
    static void testGranularStress();
    static void testChorusStress();
    static void testSaturationStress();
};

#endif
