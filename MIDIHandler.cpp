#include "MIDIHandler.h"
#include <HardwareSerial.h>

// Use Serial2 for MIDI
HardwareSerial MIDISerial(2);

MIDIHandler::MIDIHandler() :
    channel_(0),  // Omni
    runningStatus_(0),
    dataIndex_(0),
    expectedLength_(0)
{
}

void MIDIHandler::begin(uint8_t rxPin, uint8_t txPin) {
    MIDISerial.begin(31250, SERIAL_8N1, rxPin, txPin);
    Serial.printf("[MIDI] Started on pins RX=%d, TX=%d\n", rxPin, txPin);
}

void MIDIHandler::setChannel(uint8_t channel) {
    channel_ = channel;  // 0 = omni, 1-16 = specific
}

void MIDIHandler::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    uint8_t status = 0x90 | ((channel - 1) & 0x0F);
    MIDISerial.write(status);
    MIDISerial.write(note & 0x7F);
    MIDISerial.write(velocity & 0x7F);
}

void MIDIHandler::sendNoteOff(uint8_t note, uint8_t channel) {
    uint8_t status = 0x80 | ((channel - 1) & 0x0F);
    MIDISerial.write(status);
    MIDISerial.write(note & 0x7F);
    MIDISerial.write(0x00);
}

void MIDIHandler::sendCC(uint8_t cc, uint8_t value, uint8_t channel) {
    uint8_t status = 0xB0 | ((channel - 1) & 0x0F);
    MIDISerial.write(status);
    MIDISerial.write(cc & 0x7F);
    MIDISerial.write(value & 0x7F);
}

void MIDIHandler::process() {
    while (MIDISerial.available()) {
        uint8_t byte = MIDISerial.read();
        
        // Real-time messages (can occur anywhere)
        if (byte >= 0xF8) {
            switch (byte) {
                case 0xF8:  // Timing clock
                    if (clockCb_) clockCb_();
                    break;
                case 0xFA:  // Start
                case 0xFB:  // Continue
                case 0xFC:  // Stop
                    // Could add callbacks for these
                    break;
            }
            continue;
        }
        
        // Status byte?
        if (byte & 0x80) {
            if (byte < 0xF8) {
                runningStatus_ = byte;
                dataIndex_ = 0;
            }
            
            // Determine expected data length
            uint8_t type = byte & 0xF0;
            if (byte >= 0xF0 && byte < 0xF8) {
                runningStatus_ = 0; // Clear running status for system common
                expectedLength_ = 0;
                // Handle specific system common if needed
                continue;
            }

            switch (type) {
                case 0x80:  // Note Off
                case 0x90:  // Note On
                case 0xA0:  // Poly Pressure
                case 0xB0:  // CC
                case 0xE0:  // Pitch Bend
                    expectedLength_ = 2;
                    break;
                case 0xC0:  // Program Change
                case 0xD0:  // Channel Pressure
                    expectedLength_ = 1;
                    break;
                default:
                    expectedLength_ = 0;
            }
        } else {
            // Data byte
            if (runningStatus_ == 0) continue;
            
            dataBytes_[dataIndex_++] = byte;
            
            if (dataIndex_ >= expectedLength_) {
                parseMessage();
                dataIndex_ = 0;
            }
        }
    }
}

void MIDIHandler::parseMessage() {
    uint8_t type = runningStatus_ & 0xF0;
    uint8_t ch = (runningStatus_ & 0x0F) + 1;  // 1-16
    
    // Channel filter (0 = omni)
    if (channel_ != 0 && ch != channel_) return;
    
    switch (type) {
        case 0x80:  // Note Off
            if (noteOffCb_) {
                noteOffCb_(ch, dataBytes_[0]);
            }
            break;
            
        case 0x90:  // Note On
            if (dataBytes_[1] == 0) {
                // Velocity 0 = Note Off
                if (noteOffCb_) noteOffCb_(ch, dataBytes_[0]);
            } else {
                if (noteOnCb_) noteOnCb_(ch, dataBytes_[0], dataBytes_[1]);
            }
            break;
            
        case 0xB0:  // Control Change
            if (dataBytes_[0] == MIDI_CC::ALL_NOTES_OFF) {
                // Special handling could go here
            }
            if (ccCb_) ccCb_(ch, dataBytes_[0], dataBytes_[1]);
            break;
            
        case 0xE0:  // Pitch Bend
            if (pitchBendCb_) {
                int16_t value = (dataBytes_[1] << 7) | dataBytes_[0];
                value -= 8192;  // Center at 0
                pitchBendCb_(ch, value);
            }
            break;
            
        case 0xC0:  // Program Change
            if (pcCb_) pcCb_(ch, dataBytes_[0]);
            break;
    }
}
