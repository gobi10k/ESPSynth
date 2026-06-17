#include "Granular.h"
#include "Wavetables.h"
#include "MathUtils.h"
#include <math.h>
#include <string.h>

const char* GRAIN_SOURCE_NAMES[] = {"NOI", "SIN", "IMP", "TRI", "DST"};

float GranularExciter::hannTable_[WINDOW_TABLE_SIZE];
float GranularExciter::expTable_[WINDOW_TABLE_SIZE];
bool GranularExciter::tablesInitialized_ = false;

GranularExciter::GranularExciter() :
    density_(10.0f),
    durationMs_(30.0f),
    durationSpread_(0.3f),
    basePitch_(440.0f),
    pitchSpread_(0.0f),
    amplitude_(0.5f),
    source_(GrainSource::NOISE),
    window_(GrainWindow::HANN),
    freeRunning_(true),
    samplesPerGrain_(0.0f),
    sampleCounter_(0.0f),
    noiseState_(12345),
    dustProb_(0.001f)
{
    memset(grains_, 0, sizeof(grains_));
    setDensity(density_);

    if (!tablesInitialized_) {
        for (int i = 0; i < WINDOW_TABLE_SIZE; i++) {
            float pos = (float)i / (WINDOW_TABLE_SIZE - 1);
            hannTable_[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * pos));
            expTable_[i] = fastExp(-4.0f * pos) * (1.0f - fastExp(-20.0f * pos));
        }
        tablesInitialized_ = true;
    }
}

void GranularExciter::setDensity(float grainsPerSecond) {
    density_ = constrain(grainsPerSecond, 0.5f, 100.0f);
    samplesPerGrain_ = SAMPLE_RATE / density_;
    dustProb_ = density_ / SAMPLE_RATE;
}

void GranularExciter::setDuration(float ms) {
    durationMs_ = constrain(ms, 1.0f, 500.0f);
}

void GranularExciter::setDurationSpread(float spread) {
    durationSpread_ = constrain(spread, 0.0f, 1.0f);
}

void GranularExciter::setPitch(float hz) {
    basePitch_ = constrain(hz, 20.0f, 10000.0f);
}

void GranularExciter::setPitchSpread(float semitones) {
    pitchSpread_ = constrain(semitones, 0.0f, 24.0f);
}

void GranularExciter::setAmplitude(float amp) {
    amplitude_ = constrain(amp, 0.0f, 1.0f);
}

void GranularExciter::setSource(GrainSource source) {
    source_ = source;
}

void GranularExciter::setWindow(GrainWindow window) {
    window_ = window;
}

void GranularExciter::setFreeRunning(bool free) {
    freeRunning_ = free;
}

void GranularExciter::trigger() {
    trigger(basePitch_, amplitude_);
}

void GranularExciter::trigger(float pitch, float amp) {
    // Find free grain slot
    for (int i = 0; i < MAX_GRAINS; i++) {
        if (!grains_[i].active) {
            Grain& g = grains_[i];
            g.active = true;
            g.position = 0.0f;
            g.amplitude = amp;
            g.source = source_;
            g.window = window_;
            
            // Duration with randomization
            float durationVariation = 1.0f + fastRandFloat(noiseState_) * durationSpread_;
            g.duration = (durationMs_ * 0.001f) * SAMPLE_RATE * durationVariation;
            g.duration = max(10.0f, g.duration);
            g.invDuration = 1.0f / g.duration;
            
            // Pitch with randomization
            float pitchVariation = fastRandFloat(noiseState_) * pitchSpread_;
            float grainPitch = pitch * fastExp2(pitchVariation / 12.0f);
            g.phaseIncrement = (uint32_t)(grainPitch * PHASE_INCREMENT_MULTIPLIER);
            g.phase = 0;
            
            return;
        }
    }
}

void GranularExciter::spawnGrain() {
    trigger();
}


float GranularExciter::process() {
    // Auto-spawn grains in free-running mode
    if (freeRunning_) {
        sampleCounter_ += 1.0f;
        if (sampleCounter_ >= samplesPerGrain_) {
            sampleCounter_ -= samplesPerGrain_;
            spawnGrain();
        }
    }
    
    // Sum all active grains - massively optimized per-sample path
    float output = 0.0f;
    int activeCount = 0;
    for (int i = 0; i < MAX_GRAINS; i++) {
        Grain& g = grains_[i];
        if (!g.active) continue;

        // 1. Get Source Sample
        float sample = 0.0f;
        switch (g.source) {
            case GrainSource::NOISE:
                sample = fastRandFloat(noiseState_);
                break;
            case GrainSource::SINE:
                sample = Wavetables::readSine(g.phase);
                break;
            case GrainSource::IMPULSE:
                sample = (g.position < 1.0f) ? 1.0f : 0.0f;
                break;
            case GrainSource::TRIANGLE:
                sample = Wavetables::readTriangle(g.phase, 2);
                break;
            case GrainSource::DUST:
                if (fastRandFloat01(noiseState_) < dustProb_ * 10.0f) {
                    sample = fastRandFloat(noiseState_);
                }
                break;
            default: break;
        }

        // 2. Apply Window
        float windowedPos = g.position * g.invDuration;
        int winIdx = (int)(windowedPos * (WINDOW_TABLE_SIZE - 1));
        if (winIdx < 0) winIdx = 0; else if (winIdx >= WINDOW_TABLE_SIZE) winIdx = WINDOW_TABLE_SIZE - 1;

        float window = 1.0f;
        switch (g.window) {
            case GrainWindow::HANN:
            case GrainWindow::BLACKMAN:
                window = hannTable_[winIdx];
                break;
            case GrainWindow::TRIANGLE:
                window = (windowedPos < 0.5f) ? (2.0f * windowedPos) : (2.0f - 2.0f * windowedPos);
                break;
            case GrainWindow::EXPONENTIAL:
                window = expTable_[winIdx];
                break;
            case GrainWindow::RECTANGULAR:
            default:
                window = 1.0f;
                break;
        }

        output += sample * window * g.amplitude;
        activeCount++;

        // 3. Advance Grain
        g.phase += g.phaseIncrement;
        g.position += 1.0f;
        if (g.position >= g.duration) {
            g.active = false;
        }
    }
    
    if (activeCount > 1) {
        output /= sqrtf((float)activeCount);
    }

    return output;
}

int GranularExciter::getActiveGrainCount() const {
    int count = 0;
    for (int i = 0; i < MAX_GRAINS; i++) {
        if (grains_[i].active) count++;
    }
    return count;
}

void GranularExciter::reset() {
    for (int i = 0; i < MAX_GRAINS; i++) {
        grains_[i].active = false;
    }
    sampleCounter_ = 0.0f;
}
