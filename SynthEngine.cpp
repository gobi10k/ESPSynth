#include "SynthEngine.h"
#include "Wavetables.h"
#include "MathUtils.h"
#include <driver/i2s_std.h>
#include <math.h>

SynthEngine::SynthEngine() :
    pitchBendRange_(2),
    lastArpGate_(false),
    resonatorEnabled_(false),
    combEnabled_(false),
    granularMix_(0.0f),
    granularEnabled_(false),
    euclideanEnabled_(false),
    euclideanNote_(36),
    globalPan_(0.0f),
    currentVelocity_(0.0f),
    running_(false),
    audioTaskHandle_(nullptr)
{
    // Initialize parameters
    pendingParams_.oscWaveforms[0] = Waveform::SAW;
    pendingParams_.oscWaveforms[1] = Waveform::SAW;
    pendingParams_.oscDetune[0] = 0.0f;
    pendingParams_.oscDetune[1] = 7.0f;
    pendingParams_.oscCoarse[0] = 0;
    pendingParams_.oscCoarse[1] = 0;
    pendingParams_.oscMix = 0.5f;
    pendingParams_.pulseWidth[0] = 0.5f;
    pendingParams_.pulseWidth[1] = 0.5f;
    pendingParams_.morph[0] = 0.0f;
    pendingParams_.morph[1] = 0.0f;
    pendingParams_.synthMode = VoiceSynthMode::STANDARD;
    pendingParams_.fmAmount = 1.0f;
    pendingParams_.filterType = VoiceFilterType::SVF;
    pendingParams_.filterCutoff = 2000.0f;
    pendingParams_.filterReso = 0.3f;
    pendingParams_.filterMode = FilterMode::LOWPASS;
    pendingParams_.filterEnvAmount = 0.5f;
    pendingParams_.filterEnvVelocity = 0.5f;
    pendingParams_.filterKeyTracking = 0.5f;
    pendingParams_.ampA = 0.01f; pendingParams_.ampD = 0.1f; pendingParams_.ampS = 0.7f; pendingParams_.ampR = 0.3f;
    pendingParams_.fltA = 0.01f; pendingParams_.fltD = 0.2f; pendingParams_.fltS = 0.3f; pendingParams_.fltR = 0.5f;
    pendingParams_.glideTime = 0.0f;

    activeParams_ = pendingParams_;
    paramsDirty_ = true;

    pitchBend_.setImmediate(0.0f);
    pitchBend_.setSmoothTime(5.0f);
    
    masterVolume_.setImmediate(0.7f);
    masterVolume_.setSmoothTime(10.0f);
    
    // Setup LFOs
    lfos_[0].setFrequency(2.0f);
    lfos_[0].setWaveform(LFOWaveform::SINE);
    lfos_[1].setFrequency(0.2f);
    lfos_[1].setWaveform(LFOWaveform::TRIANGLE);
    
    // Default mod routing - LFO1 to filter cutoff with significant amount
    modMatrix_.setSlot(0, ModSource::LFO1, ModDest::FILTER_CUTOFF, 0.5f);
}

bool SynthEngine::init() {
    Wavetables::init();
    
    // Modern I2S API Configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, &tx_handle_, NULL);
    if (err != ESP_OK) {
        Serial.printf("[SynthEngine] I2S channel allocation failed: %d\n", err);
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)I2S_MCK_PIN,
            .bclk = (gpio_num_t)I2S_BCK_PIN,
            .ws = (gpio_num_t)I2S_WS_PIN,
            .dout = (gpio_num_t)I2S_DATA_OUT_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(tx_handle_, &std_cfg);
    if (err != ESP_OK) {
        Serial.printf("[SynthEngine] I2S standard mode init failed: %d\n", err);
        return false;
    }

    err = i2s_channel_enable(tx_handle_);
    if (err != ESP_OK) {
        Serial.printf("[SynthEngine] I2S channel enable failed: %d\n", err);
        return false;
    }
    
    Serial.println("[SynthEngine] Ready (Modern API)");
    return true;
}

void SynthEngine::start() {
    if (running_) return;
    running_ = true;
    
    xTaskCreatePinnedToCore(
        audioTaskWrapper,
        "SynthTask",
        20480,  // Further increased stack size to 20KB
        this,
        configMAX_PRIORITIES - 3, // Slightly lower than max to prevent core starvation
        &audioTaskHandle_,
        1
    );
}

void SynthEngine::stop() {
    running_ = false;
    if (audioTaskHandle_) {
        vTaskDelay(pdMS_TO_TICKS(50));
        vTaskDelete(audioTaskHandle_);
        audioTaskHandle_ = nullptr;
    }
}

int SynthEngine::findVoiceForNote(uint8_t note) {
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isActive() && voices_[i].getNote() == note) {
            return i;
        }
    }
    return -1;
}

int SynthEngine::allocateVoice(uint8_t note) {
    // First: find a free voice
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isFree()) {
            return i;
        }
    }
    
    // Second: steal the oldest releasing voice
    int oldestReleasing = -1;
    uint32_t maxAge = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isReleasing() && voices_[i].getAge() > maxAge) {
            maxAge = voices_[i].getAge();
            oldestReleasing = i;
        }
    }
    if (oldestReleasing >= 0) {
        voices_[oldestReleasing].forceOff();
        return oldestReleasing;
    }
    
    // Third: steal the oldest active voice
    int oldest = 0;
    maxAge = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].getAge() > maxAge) {
            maxAge = voices_[i].getAge();
            oldest = i;
        }
    }
    voices_[oldest].forceOff();
    return oldest;
}

void SynthEngine::noteOn(uint8_t note, uint8_t velocity) {
    currentVelocity_ = velocity / 127.0f;
    notesSustained_[note % 128] = false;

    // If arpeggiator is on, feed it instead
    if (arp_.getMode() != ArpMode::OFF) {
        arp_.noteOn(note, velocity);
        return;
    }
    
    noteOnInternal(note, velocity);
}

void SynthEngine::noteOnInternal(uint8_t note, uint8_t velocity) {
    int voice = allocateVoice(note);
    if (voice < 0) return;
    
    voices_[voice].applyParams(activeParams_);

    // Combine spread and global pan
    float spread = -0.7f + (1.4f * voice / (NUM_VOICES - 1));
    voices_[voice].setPan(constrain(spread + globalPan_, -1.0f, 1.0f));

    if (resonatorEnabled_) {
        resonator_.setFrequency(midiToFreq(note));
    }

    voices_[voice].noteOn(note, velocity);
}

void SynthEngine::noteOff(uint8_t note) {
    if (arp_.getMode() != ArpMode::OFF) {
        arp_.noteOff(note);
        return;
    }

    if (sustainPedalActive_) {
        notesSustained_[note % 128] = true;
        return;
    }
    
    // Find voice playing this note
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isActive() && voices_[i].getNote() == note) {
            voices_[i].noteOff();
            break;
        }
    }
}

void SynthEngine::setSustainPedal(bool active) {
    if (sustainPedalActive_ == active) return;
    sustainPedalActive_ = active;

    if (!sustainPedalActive_) {
        for (int i = 0; i < 128; i++) {
            if (notesSustained_[i]) {
                for (int v = 0; v < NUM_VOICES; v++) {
                    if (voices_[v].isActive() && voices_[v].getNote() == i) {
                        voices_[v].noteOff();
                    }
                }
                notesSustained_[i] = false;
            }
        }
    }
}

void SynthEngine::allNotesOff() {
    arp_.allNotesOff();
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].forceOff();
    }
}

void SynthEngine::setPitchBend(int16_t value) {
    // Convert to semitones
    float semitones = (value / 8192.0f) * pitchBendRange_;
    pitchBend_.setTarget(semitones);
}

void SynthEngine::setPitchBendRange(uint8_t semitones) {
    pitchBendRange_ = constrain(semitones, 1, 24);
}

void SynthEngine::setOscWaveform(int osc, Waveform wf) {
    if (osc >= 0 && osc < 2) {
        pendingParams_.oscWaveforms[osc] = wf;
        paramsDirty_ = true;
    }
}

void SynthEngine::setOscCoarse(int osc, int8_t semitones) {
    if (osc >= 0 && osc < 2) {
        pendingParams_.oscCoarse[osc] = semitones;
        paramsDirty_ = true;
    }
}

void SynthEngine::setOscMorph(int osc, float morph) {
    if (osc >= 0 && osc < 2) {
        pendingParams_.morph[osc] = morph;
        paramsDirty_ = true;
    }
}

void SynthEngine::setOscDetune(int osc, float cents) {
    if (osc >= 0 && osc < 2) {
        pendingParams_.oscDetune[osc] = cents;
        paramsDirty_ = true;
    }
}

void SynthEngine::setOscMix(float mix) {
    pendingParams_.oscMix = constrain(mix, 0.0f, 1.0f);
    paramsDirty_ = true;
}

void SynthEngine::setSynthMode(VoiceSynthMode mode) {
    pendingParams_.synthMode = mode;
    paramsDirty_ = true;
}

void SynthEngine::setFMAmount(float amount) {
    pendingParams_.fmAmount = amount;
    paramsDirty_ = true;
}

void SynthEngine::setPulseWidth(int osc, float pw) {
    if (osc >= 0 && osc < 2) {
        pendingParams_.pulseWidth[osc] = pw;
        paramsDirty_ = true;
    }
}

void SynthEngine::setFilterCutoff(float hz) {
    pendingParams_.filterCutoff = constrain(hz, 20.0f, 20000.0f);
    paramsDirty_ = true;
}

void SynthEngine::setFilterResonance(float r) {
    pendingParams_.filterReso = constrain(r, 0.0f, 1.0f);
    paramsDirty_ = true;
}

void SynthEngine::setFilterMode(FilterMode mode) {
    pendingParams_.filterMode = mode;
    paramsDirty_ = true;
}

void SynthEngine::setFilterEnvAmount(float amount) {
    pendingParams_.filterEnvAmount = constrain(amount, -1.0f, 1.0f);
    paramsDirty_ = true;
}

void SynthEngine::setFilterEnvVelocity(float amount) {
    pendingParams_.filterEnvVelocity = constrain(amount, 0.0f, 1.0f);
    paramsDirty_ = true;
}

void SynthEngine::setFilterKeyTracking(float amount) {
    pendingParams_.filterKeyTracking = constrain(amount, 0.0f, 1.0f);
    paramsDirty_ = true;
}

void SynthEngine::setFilterType(VoiceFilterType type) {
    pendingParams_.filterType = type;
    paramsDirty_ = true;
}

void SynthEngine::setAmpADSR(float a, float d, float s, float r) {
    if (a >= 0) pendingParams_.ampA = a;
    if (d >= 0) pendingParams_.ampD = d;
    if (s >= 0) pendingParams_.ampS = s;
    if (r >= 0) pendingParams_.ampR = r;
    paramsDirty_ = true;
}

void SynthEngine::setFilterADSR(float a, float d, float s, float r) {
    if (a >= 0) pendingParams_.fltA = a;
    if (d >= 0) pendingParams_.fltD = d;
    if (s >= 0) pendingParams_.fltS = s;
    if (r >= 0) pendingParams_.fltR = r;
    paramsDirty_ = true;
}

void SynthEngine::setGlideTime(float ms) {
    pendingParams_.glideTime = ms;
    paramsDirty_ = true;
}

LFO& SynthEngine::getLFO(int index) {
    return lfos_[index % NUM_LFOS];
}

void SynthEngine::setGranularMix(float mix) {
    granularMix_ = constrain(mix, 0.0f, 1.0f);
}

void SynthEngine::setMasterVolume(float vol) {
    masterVolume_.setTarget(constrain(vol, 0.0f, 1.0f));
}

uint8_t SynthEngine::getActiveVoiceCount() const {
    uint8_t count = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isActive()) count++;
    }
    return count;
}

void SynthEngine::processBlock() {
    profiler_.startSample();
    blockCounter_++;

    // --- Control Rate Update (Once per 128 samples) ---
    if (paramsDirty_) {
        activeParams_ = pendingParams_;
        paramsDirty_ = false;
        for (int v = 0; v < NUM_VOICES; v++) {
            voices_[v].applyParams(activeParams_);
        }
    }

    // Massive CPU optimization: Move slow modulation out of the sample loop
    float lfo1 = lfos_[0].process(DMA_BUFFER_SAMPLES);
    float lfo2 = lfos_[1].process(DMA_BUFFER_SAMPLES);
    modMatrix_.setSourceValue(ModSource::LFO1, lfo1);
    modMatrix_.setSourceValue(ModSource::LFO2, lfo2);
    modMatrix_.setSourceValue(ModSource::VELOCITY, currentVelocity_);
    modMatrix_.process();
    float filterMod = modMatrix_.getModulation(ModDest::FILTER_CUTOFF);
    for (int i = 0; i < DMA_BUFFER_SAMPLES; i++) pitchBend_.process();
    float pbSemitones = pitchBend_.getCurrent();
    float pitchMod = modMatrix_.getModulation(ModDest::OSC_PITCH) * 2.0f + pbSemitones;
    float pw1Mod = modMatrix_.getModulation(ModDest::OSC1_PW);
    float pw2Mod = modMatrix_.getModulation(ModDest::OSC2_PW);

    for (int v = 0; v < NUM_VOICES; v++) {
        if (voices_[v].isActive()) {
            voices_[v].setGlobalFilterMod(filterMod);
            voices_[v].setGlobalPitchMod(pitchMod);
            voices_[v].setGlobalOsc1PWMod(pw1Mod);
            voices_[v].setGlobalOsc2PWMod(pw2Mod);
            voices_[v].updateBlockParams();

            // Pre-calculate panning coefficients
            float pan = voices_[v].getPan();
            float panAngle = (pan + 1.0f) * 0.785398f;
            voicePanL_[v] = cosf(panAngle);
            voicePanR_[v] = sinf(panAngle);
        }
    }
    // --------------------------------------------------
    
    for (int i = 0; i < DMA_BUFFER_SAMPLES; i++) {
        // Euclidean
        if (euclideanEnabled_) {
            if (euclideanSeq_.process()) {
                noteOnInternal(euclideanNote_, euclideanSeq_.getVelocity());
            }
        }

        // Process arpeggiator (only if mode is not OFF and we have notes)
        if (arp_.getMode() != ArpMode::OFF) {
            if (arp_.process()) {
                if (lastArpGate_) {
                    for (int v = 0; v < NUM_VOICES; v++) {
                        voices_[v].noteOff();
                    }
                }
                
                int voice = allocateVoice(arp_.getCurrentNote());
                if (voice >= 0 && voice < NUM_VOICES) {
                    voices_[voice].applyParams(activeParams_);

                    // Combine spread and global pan
                    float spread = -0.7f + (1.4f * voice / (NUM_VOICES - 1));
                    voices_[voice].setPan(constrain(spread + globalPan_, -1.0f, 1.0f));

                    currentVelocity_ = arp_.getCurrentVelocity() / 127.0f;

                    if (resonatorEnabled_) {
                        resonator_.setFrequency(midiToFreq(arp_.getCurrentNote()));
                    }

                    voices_[voice].noteOn(arp_.getCurrentNote(), arp_.getCurrentVelocity());
                }
            }
            
            // Handle arp gate off
            if (lastArpGate_ && !arp_.isGateOn()) {
                for (int v = 0; v < NUM_VOICES; v++) {
                    if (voices_[v].isActive()) {
                        voices_[v].noteOff();
                    }
                }
            }
            lastArpGate_ = arp_.isGateOn();
        }
        
        // Mix all voices with panning - unrolled for 4 voices
        float left = 0.0f;
        float right = 0.0f;

        if (voices_[0].isActive()) {
            float s = voices_[0].process();
            left += s * voicePanL_[0];
            right += s * voicePanR_[0];
        }
        if (voices_[1].isActive()) {
            float s = voices_[1].process();
            left += s * voicePanL_[1];
            right += s * voicePanR_[1];
        }
        if (voices_[2].isActive()) {
            float s = voices_[2].process();
            left += s * voicePanL_[2];
            right += s * voicePanR_[2];
        }
        if (voices_[3].isActive()) {
            float s = voices_[3].process();
            left += s * voicePanL_[3];
            right += s * voicePanR_[3];
        }
        
        // Scale down for mixing (1.0 / sqrt(NUM_VOICES))
        left *= 0.5f;
        right *= 0.5f;

        // Apply Granular exciter (if enabled) - Mono input, but we can spread it
        if (granularEnabled_) {
            float gran = granular_.process();
            left = left * (1.0f - granularMix_) + gran * granularMix_ * 0.7f;
            right = right * (1.0f - granularMix_) + gran * granularMix_ * 0.7f;
        }

        // Apply Resonator and Comb (if enabled) - Currently Mono
        if (resonatorEnabled_) {
            float mono = (left + right) * 0.5f;
            float res = resonator_.process(mono);
            left = left * 0.5f + res * 0.5f;
            right = right * 0.5f + res * 0.5f;
        }
        if (combEnabled_) {
            float mono = (left + right) * 0.5f;
            float cb = comb_.process(mono);
            left = left * 0.5f + cb * 0.5f;
            right = right * 0.5f + cb * 0.5f;
        }

        if (isnan(left) || isinf(left)) left = 0.0f;
        if (isnan(right) || isinf(right)) right = 0.0f;

        // Process global effects in stereo
        effects_.processStereo(left, right);

        // Reverb - stereo processing
        float wetL, wetR;
        reverb_.processStereo(left, right, wetL, wetR);

        // Compressor on stereo (before master volume and soft clip)
        float compL, compR;
        compressor_.processStereo(wetL, wetR, compL, compR);

        // Master volume
        float vol = masterVolume_.process();
        left = compL * vol;
        right = compR * vol;
        
        // Final Soft clip / Limiter
        left = fastTanh(left);
        right = fastTanh(right);
        
        blockBuffer_[i * 2] = (int16_t)(left * 32767.0f);
        blockBuffer_[i * 2 + 1] = (int16_t)(right * 32767.0f);
    }
    
    profiler_.endSample();
    size_t bytesWritten;
    i2s_channel_write(tx_handle_, blockBuffer_, sizeof(blockBuffer_), &bytesWritten, portMAX_DELAY);

    // Yield to allow other tasks (Core 0/1) and watchdog to run if CPU is saturated
    vTaskDelay(0);
}

void SynthEngine::audioTaskWrapper(void* param) {
    SynthEngine* engine = static_cast<SynthEngine*>(param);
    while (engine->running_) {
        engine->processBlock();
    }
    vTaskDelete(NULL);
}
