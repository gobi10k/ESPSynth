#ifndef HARDWARE_ENCODER_H
#define HARDWARE_ENCODER_H

#include <Arduino.h>

class HardwareEncoder {
public:
    HardwareEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW);

    void init();
    void update(); // Call in loop (switch debounce only)

    int getDelta();
    bool isPressed();
    bool wasClicked();

    // Called from ISR — must be public for static wrapper
    void IRAM_ATTR handleInterrupt();

    // Debug: print pin states and ISR counters to Serial
    void printDebug();

private:
    uint8_t pinA_, pinB_, pinSW_;

    // Encoder state (ISR-accessed, must be volatile)
    volatile uint8_t state_;
    volatile int32_t delta_;

    // Switch state
    bool lastSwState_;
    volatile bool clicked_;
    uint32_t lastDebounceTime_;

    // Instance index for ISR routing
    int8_t instanceIndex_;

    // Debug counters
    volatile uint32_t isrCount_;
};

#endif
