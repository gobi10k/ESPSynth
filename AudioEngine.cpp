#include "AudioEngine.h"
#include "Wavetables.h"
#include <driver/i2s.h>
#include <math.h>

const char* SYNTH_MODE_NAMES[] = {"STD", "FM", "SYNC", "RING"};

AudioEngine::AudioEngine() : 
    synthMode_(SynthMode::STANDARD),
    filterEnvAmount_(0.5f),
    masterVolume_(0.7f),
    currentVelocity_(1.0f),
    baseFrequency_(220.0f),
    running_(false),
    audioTaskHandle_(nullptr) 
{
    // Default LFO settings
    lfos_[0].setFrequency(1.0f);
    lfos_[0].setWaveform(LFOWaveform::SINE);
    lfos_[0].setDepth(1.0f);
    
    lfos_[1].setFrequency(0.1f);
    lfos_[1].setWaveform(LFOWaveform::TRIANGLE);
    lfos_[1].setDepth(1.0f);
    
    // Default envelopes
    ampEnv_.setADSR(0.01f, 0.1f, 0.7f, 0.3f);
    filterEnv_.setADSR(0.01f, 0.2f, 0.3f, 0.5f);
    
    // Default filter
    filter_.setCutoff(2000.0f);
    filter_.setResonance(0.3f);
    
    // Default FM settings
    fmPair_.setRatio(2.0f);
    fmPair_.setModIndex(1.0f);
    
    // Default sync settings
    syncOsc_.setSlaveWaveform(Waveform::SAW);
    
    // Default modulation
    modMatrix_.setSlot(0, ModSource::LFO1, ModDest::FILTER_CUTOFF, 0.2f);
    modMatrix_.setSlot(1, ModSource::ENV_FILTER, ModDest::FILTER_CUTOFF, 0.5f);
}

bool AudioEngine::init() {
    Wavetables::init();
    
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_SAMPLES,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pinConfig = {
        .mck_io_num = I2S_MCK_PIN,
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_DATA_OUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[AudioEngine] I2S install failed: %d\n", err);
        return false;
    }
    
    err = i2s_set_pin(I2S_NUM_0, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("[AudioEngine] I2S pin config failed: %d\n", err);
        return false;
    }
    
    i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    
    Serial.println("[AudioEngine] I2S ready");
    return true;
}

void AudioEngine::start() {
    if (running_) return;
    running_ = true;
    
    xTaskCreatePinnedToCore(
        audioTaskWrapper,
        "AudioTask",
        8192,
        this,
        configMAX_PRIORITIES - 1,
        &audioTaskHandle_,
        1
    );
    
    Serial.println("[AudioEngine] Started");
}

void AudioEngine::stop() {
    running_ = false;
    if (audioTaskHandle_) {
        vTaskDelay(pdMS_TO_TICKS(50));
        vTaskDelete(audioTaskHandle_);
        audioTaskHandle_ = nullptr;
    }
}

void AudioEngine::setSynthMode(SynthMode mode) {
    synthMode_ = mode;
}

Oscillator& AudioEngine::getOscillator(int index) {
    return oscillators_[index % MAX_OSCILLATORS];
}

const Oscillator& AudioEngine::getOscillator(int index) const {
    return oscillators_[index % MAX_OSCILLATORS];
}

LFO& AudioEngine::getLFO(int index) {
    return lfos_[index % NUM_LFOS];
}

void AudioEngine::setMasterVolume(float vol) {
    masterVolume_ = constrain(vol, 0.0f, 1.0f);
}

void AudioEngine::setFilterEnvAmount(float amount) {
    filterEnvAmount_ = constrain(amount, -1.0f, 1.0f);
}

void AudioEngine::noteOn(float frequency, float velocity) {
    baseFrequency_ = frequency;
    currentVelocity_ = velocity;
    
    // Standard oscillators
    oscillators_[0].setFrequency(frequency);
    oscillators_[1].setFrequency(frequency * 1.005f);
    oscillators_[2].setFrequency(frequency * 0.5f);
    
    // FM
    fmPair_.setFrequency(frequency);
    
    // Sync
    syncOsc_.setMasterFreq(frequency);
    syncOsc_.setSlaveFreq(frequency * 2.0f);
    
    // Ring mod
    ringMod_.setFrequency(frequency * 1.5f);
    
    // Trigger envelopes
    ampEnv_.gate(true);
    filterEnv_.gate(true);
    
    modMatrix_.setSourceValue(ModSource::VELOCITY, velocity);
}

void AudioEngine::noteOff() {
    ampEnv_.gate(false);
    filterEnv_.gate(false);
}

void AudioEngine::applyModulation() {
    modMatrix_.setSourceValue(ModSource::LFO1, lfos_[0].process());
    modMatrix_.setSourceValue(ModSource::LFO2, lfos_[1].process());
    modMatrix_.setSourceValue(ModSource::ENV_FILTER, filterEnv_.getValue());
    modMatrix_.setSourceValue(ModSource::ENV_AMP, ampEnv_.getValue());
    
    modMatrix_.process();
    
    // Apply pitch modulation
    float pitchMod = modMatrix_.getModulation(ModDest::OSC_PITCH);
    if (pitchMod != 0.0f) {
        float freqMult = powf(2.0f, pitchMod * 2.0f / 12.0f);
        oscillators_[0].setFrequency(baseFrequency_ * freqMult);
        oscillators_[1].setFrequency(baseFrequency_ * 1.005f * freqMult);
    }
    
    // Apply filter modulation
    float filterMod = modMatrix_.getModulation(ModDest::FILTER_CUTOFF);
    filter_.setCutoffMod(filterMod * 5000.0f);
}

void AudioEngine::processBlock() {
    int16_t buffer[DMA_BUFFER_SAMPLES * 2];
    
    for (int i = 0; i < DMA_BUFFER_SAMPLES; i++) {
        applyModulation();
        
        float sample = 0.0f;
        
        // Generate audio based on synthesis mode
        switch (synthMode_) {
            case SynthMode::STANDARD:
                // Mix all oscillators
                for (int osc = 0; osc < MAX_OSCILLATORS; osc++) {
                    sample += oscillators_[osc].process();
                }
                break;
                
            case SynthMode::FM:
                sample = fmPair_.process();
                break;
                
            case SynthMode::SYNC:
                sample = syncOsc_.process();
                break;
                
            case SynthMode::RING: {
                // Oscillator 0 through ring mod
                float osc = oscillators_[0].process();
                sample = ringMod_.process(osc);
                break;
            }
        }
        
        // Optional AM
        sample = aMod_.process(sample);
        
        // Optional wavefolder
        sample = wavefolder_.process(sample);
        
        // Filter
        sample = filter_.process(sample);
        
        // Amplitude envelope
        float ampEnvValue = ampEnv_.process();
        sample *= ampEnvValue;
        
        // Effects chain
        sample = effects_.process(sample);
        
        // Master volume and soft clip
        sample *= masterVolume_;
        sample = tanhf(sample);
        
        int16_t sampleInt = (int16_t)(sample * 32767.0f);
        buffer[i * 2] = sampleInt;
        buffer[i * 2 + 1] = sampleInt;
    }
    
    size_t bytesWritten;
    i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytesWritten, portMAX_DELAY);
}

void AudioEngine::audioTaskWrapper(void* param) {
    AudioEngine* engine = static_cast<AudioEngine*>(param);
    while (engine->running_) {
        engine->processBlock();
    }
    vTaskDelete(NULL);
}
