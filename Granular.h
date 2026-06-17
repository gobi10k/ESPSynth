#ifndef GRANULAR_H
#define GRANULAR_H

#include "Config.h"
#include "MathUtils.h"

/**
 * Granular Exciter
 * 
 * Generates short bursts of audio grains, ideal for exciting resonators
 * and creating textural elements. Each grain is a windowed burst of
 * noise, sine, or other source material.
 * 
 * This is a simplified granular engine focused on excitation rather than
 * full granular synthesis (no sample playback, time-stretching is limited).
 */

enum class GrainSource : uint8_t {
    NOISE = 0,      // White noise (broadband excitation)
    SINE,           // Sine wave (tonal excitation)
    IMPULSE,        // Click/impulse (very short, sharp)
    TRIANGLE,       // Softer tonal
    DUST,           // Random impulses (sparse)
    NUM_SOURCES
};

extern const char* GRAIN_SOURCE_NAMES[];

enum class GrainWindow : uint8_t {
    HANN = 0,       // Smooth, natural decay
    BLACKMAN,       // Even smoother
    TRIANGLE,       // Linear attack/decay
    RECTANGULAR,    // Harsh, clicky
    EXPONENTIAL,    // Sharp attack, long decay
    NUM_WINDOWS
};

// Single grain state
struct Grain {
    bool active;
    uint32_t phase;
    uint32_t phaseIncrement;
    float amplitude;
    float position;     // 0-1 within grain duration
    float duration;     // In samples
    float invDuration;
    GrainSource source;
    GrainWindow window;
};

constexpr uint8_t MAX_GRAINS = 8;
constexpr uint16_t WINDOW_TABLE_SIZE = 256;

class GranularExciter {
public:
    GranularExciter();
    
    // Grain parameters
    void setDensity(float grainsPerSecond);  // 1-50 grains/sec
    void setDuration(float ms);              // 5-200ms
    void setDurationSpread(float spread);    // Randomization (0-1)
    void setPitch(float hz);                 // Grain pitch (for tonal sources)
    void setPitchSpread(float semitones);    // Pitch randomization
    void setAmplitude(float amp);
    
    // Source and window
    void setSource(GrainSource source);
    void setWindow(GrainWindow window);
    
    // Trigger mode
    void setFreeRunning(bool free);   // Auto-trigger vs manual
    void trigger();                    // Manual grain trigger
    void trigger(float pitch, float amp);  // Trigger with specific params
    
    float getDensity() const { return density_; }
    float getDuration() const { return durationMs_; }
    float getDurationSpread() const { return durationSpread_; }
    float getPitchSpread() const { return pitchSpread_; }
    float getAmplitude() const { return amplitude_; }
    GrainSource getSource() const { return source_; }
    GrainWindow getWindow() const { return window_; }
    bool isFreeRunning() const { return freeRunning_; }
    
    // Process - returns sum of all active grains
    float process();
    
    // Get number of active grains (for visualization)
    int getActiveGrainCount() const;
    
    void reset();

private:
    void spawnGrain();
    
    // Parameters
    float density_;
    float durationMs_;
    float durationSpread_;
    float basePitch_;
    float pitchSpread_;
    float amplitude_;
    GrainSource source_;
    GrainWindow window_;
    bool freeRunning_;
    
    // Grain pool
    Grain grains_[MAX_GRAINS];
    
    // Window tables
    static float hannTable_[WINDOW_TABLE_SIZE];
    static float expTable_[WINDOW_TABLE_SIZE];
    static bool tablesInitialized_;

    // Timing
    float samplesPerGrain_;
    float sampleCounter_;
    
    // Noise state
    uint32_t noiseState_;
    float dustProb_;
};

#endif
