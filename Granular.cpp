#include "Granular.h"
#include "Wavetables.h"
#include <math.h>
#include <string.h>

const char* GRAIN_SOURCE_NAMES[] = {"NOI", "SIN", "IMP", "TRI", "DST"};

float GranularExciter::hannTable_[WINDOW_TABLE_SIZE];
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
            hannTable_[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (WINDOW_TABLE_SIZE - 1)));
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

float GranularExciter::getWindow(float position, GrainWindow window) {
    // position: 0 to 1 through grain lifetime
    if (position < 0.0f || isnan(position)) position = 0.0f;
    else if (position > 1.0f) position = 1.0f;

    switch (window) {
        case GrainWindow::HANN:
        case GrainWindow::BLACKMAN: {
            int idx = (int)(position * (WINDOW_TABLE_SIZE - 1));
            return hannTable_[idx];
        }
            
        case GrainWindow::TRIANGLE:
            return (position < 0.5f) ? (2.0f * position) : (2.0f - 2.0f * position);
            
        case GrainWindow::RECTANGULAR:
            return 1.0f;
            
        case GrainWindow::EXPONENTIAL:
            return fastExp(-4.0f * position) * (1.0f - fastExp(-20.0f * position));
            
        default:
            return 1.0f;
    }
}

float GranularExciter::getSourceSample(Grain& grain) {
    switch (grain.source) {
        case GrainSource::NOISE:
            return fastRandFloat(noiseState_);
            
        case GrainSource::SINE:
            return Wavetables::readSine(grain.phase);
            
        case GrainSource::IMPULSE:
            return (grain.position < 1.0f) ? 1.0f : 0.0f;
            
        case GrainSource::TRIANGLE:
            return Wavetables::readTriangle(grain.phase, 2);
            
        case GrainSource::DUST:
            if (fastRandFloat(noiseState_) < dustProb_ * 10.0f) {
                return fastRandFloat(noiseState_);
            }
            return 0.0f;
            
        default:
            return 0.0f;
    }
}

float GranularExciter::processGrain(Grain& grain) {
    // Get source sample
    float sample = getSourceSample(grain);
    
    // Apply window
    float windowedPos = grain.position / grain.duration;
    float window = getWindow(windowedPos, grain.window);
    
    sample *= window * grain.amplitude;
    
    // Advance grain
    grain.phase += grain.phaseIncrement;
    grain.position += 1.0f;
    
    // Check if grain is complete
    if (grain.position >= grain.duration) {
        grain.active = false;
    }
    
    return sample;
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
    
    // Sum all active grains
    float output = 0.0f;
    for (int i = 0; i < MAX_GRAINS; i++) {
        if (grains_[i].active) {
            output += processGrain(grains_[i]);
        }
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
