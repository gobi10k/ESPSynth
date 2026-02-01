#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include "Config.h"
#include <functional>

// MIDI message types
enum class MIDIStatus : uint8_t {
    NOTE_OFF = 0x80,
    NOTE_ON = 0x90,
    POLY_PRESSURE = 0xA0,
    CONTROL_CHANGE = 0xB0,
    PROGRAM_CHANGE = 0xC0,
    CHANNEL_PRESSURE = 0xD0,
    PITCH_BEND = 0xE0,
    SYSTEM = 0xF0
};

// Common CC numbers
namespace MIDI_CC {
    constexpr uint8_t MOD_WHEEL = 1;
    constexpr uint8_t BREATH = 2;
    constexpr uint8_t VOLUME = 7;
    constexpr uint8_t PAN = 10;
    constexpr uint8_t EXPRESSION = 11;
    constexpr uint8_t SUSTAIN = 64;
    constexpr uint8_t FILTER_CUTOFF = 74;
    constexpr uint8_t FILTER_RESO = 71;
    constexpr uint8_t ATTACK = 73;
    constexpr uint8_t DECAY = 75;
    constexpr uint8_t SUSTAIN_LEVEL = 79;
    constexpr uint8_t RELEASE = 72;
    constexpr uint8_t ALL_NOTES_OFF = 123;
}

// Callback types
using NoteOnCallback = std::function<void(uint8_t channel, uint8_t note, uint8_t velocity)>;
using NoteOffCallback = std::function<void(uint8_t channel, uint8_t note)>;
using CCCallback = std::function<void(uint8_t channel, uint8_t cc, uint8_t value)>;
using PitchBendCallback = std::function<void(uint8_t channel, int16_t value)>;
using ClockCallback = std::function<void()>;

class MIDIHandler {
public:
    MIDIHandler();
    
    void begin(uint8_t rxPin = 16, uint8_t txPin = 17);
    void process();  // Call in loop
    
    void setChannel(uint8_t channel);  // 0 = omni
    uint8_t getChannel() const { return channel_; }
    
    // Register callbacks
    void setNoteOnCallback(NoteOnCallback cb) { noteOnCb_ = cb; }
    void setNoteOffCallback(NoteOffCallback cb) { noteOffCb_ = cb; }
    void setCCCallback(CCCallback cb) { ccCb_ = cb; }
    void setPitchBendCallback(PitchBendCallback cb) { pitchBendCb_ = cb; }
    void setClockCallback(ClockCallback cb) { clockCb_ = cb; }
    
    // CC mapping helpers
    float ccToFloat(uint8_t value) { return value / 127.0f; }
    float ccToRange(uint8_t value, float min, float max) {
        return min + (max - min) * (value / 127.0f);
    }

private:
    void parseMessage();
    
    uint8_t channel_;
    
    // Parser state
    uint8_t runningStatus_;
    uint8_t dataBytes_[2];
    uint8_t dataIndex_;
    uint8_t expectedLength_;
    
    // Callbacks
    NoteOnCallback noteOnCb_;
    NoteOffCallback noteOffCb_;
    CCCallback ccCb_;
    PitchBendCallback pitchBendCb_;
    ClockCallback clockCb_;
};

#endif
