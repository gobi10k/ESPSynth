#include "HardwareEncoder.h"

// ISR routing: static instances for up to 2 encoders
static HardwareEncoder* encoderInstances[2] = {nullptr, nullptr};
static uint8_t encoderCount = 0;

static void IRAM_ATTR encoderISR0() {
    if (encoderInstances[0]) encoderInstances[0]->handleInterrupt();
}
static void IRAM_ATTR encoderISR1() {
    if (encoderInstances[1]) encoderInstances[1]->handleInterrupt();
}

// Quadrature state transition table — MUST be in DRAM for ISR access!
// Accessing flash from an IRAM_ATTR ISR causes LoadProhibited on ESP32.
// Index = (oldState << 2) | newState, where state = (A << 1) | B
static DRAM_ATTR const int8_t KNOB_DIR[] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

HardwareEncoder::HardwareEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW) :
    pinA_(pinA), pinB_(pinB), pinSW_(pinSW),
    state_(0), delta_(0),
    lastSwState_(true), clicked_(false), lastDebounceTime_(0),
    instanceIndex_(-1), isrCount_(0)
{
}

void HardwareEncoder::init() {
    // Register for ISR routing
    if (encoderCount < 2) {
        instanceIndex_ = encoderCount;
        encoderInstances[encoderCount] = this;
        encoderCount++;
    }

    // GPIO 36 and 39 are input-only, no internal pullups
    if (pinA_ == 36 || pinA_ == 39) {
        pinMode(pinA_, INPUT);
    } else {
        pinMode(pinA_, INPUT_PULLUP);
    }

    if (pinB_ == 36 || pinB_ == 39) {
        pinMode(pinB_, INPUT);
    } else {
        pinMode(pinB_, INPUT_PULLUP);
    }

    if (pinSW_ == 36 || pinSW_ == 39) {
        pinMode(pinSW_, INPUT);
    } else {
        pinMode(pinSW_, INPUT_PULLUP);
    }

    // Read initial state
    state_ = (digitalRead(pinA_) << 1) | digitalRead(pinB_);

    // Attach interrupts on BOTH pins
    void (*isr)() = nullptr;
    if (instanceIndex_ == 0) isr = encoderISR0;
    else if (instanceIndex_ == 1) isr = encoderISR1;

    if (isr) {
        attachInterrupt(digitalPinToInterrupt(pinA_), isr, CHANGE);
        attachInterrupt(digitalPinToInterrupt(pinB_), isr, CHANGE);
    }

    Serial.printf("[Encoder %d] pins A=%d B=%d SW=%d, initial state=%d\n",
                  instanceIndex_, pinA_, pinB_, pinSW_, state_);
}

void IRAM_ATTR HardwareEncoder::handleInterrupt() {
    // All data accessed here MUST be in RAM (not flash):
    // - KNOB_DIR: DRAM_ATTR ✓
    // - state_, delta_, isrCount_: class members (heap/stack = RAM) ✓
    // - digitalRead: IRAM-safe on ESP32 ✓
    uint8_t newState = (digitalRead(pinA_) << 1) | digitalRead(pinB_);

    if (newState != state_) {
        delta_ += KNOB_DIR[(state_ << 2) | newState];
        state_ = newState;
    }
    isrCount_++;
}

void HardwareEncoder::update() {
    // Encoder rotation handled by interrupts.
    // Only switch debouncing here.
    bool sw = digitalRead(pinSW_);
    if (sw != lastSwState_) {
        if (millis() - lastDebounceTime_ > 50) {
            if (lastSwState_ && !sw) {
                clicked_ = true;
            }
            lastSwState_ = sw;
            lastDebounceTime_ = millis();
        }
    }
}

int HardwareEncoder::getDelta() {
    noInterrupts();
    int32_t d = delta_;
    delta_ = 0;
    interrupts();

    if (d == 0) return 0;

    // Most encoders: 4 state changes per detent (full quadrature cycle).
    // Some cheap ones: 2 per detent. Try 4 first; if encoder feels sluggish,
    // change to 2. If it's too sensitive, try 4.
    const int STATES_PER_DETENT = 4;

    if (abs(d) >= STATES_PER_DETENT) {
        int result = d / STATES_PER_DETENT;
        int remainder = d - (result * STATES_PER_DETENT);
        // Put remainder back
        noInterrupts();
        delta_ += remainder;
        interrupts();
        return result;
    }

    // Not enough for a detent — put it all back
    noInterrupts();
    delta_ += d;
    interrupts();
    return 0;
}

bool HardwareEncoder::isPressed() {
    return !digitalRead(pinSW_);
}

bool HardwareEncoder::wasClicked() {
    bool c = clicked_;
    clicked_ = false;
    return c;
}

void HardwareEncoder::printDebug() {
    uint8_t a = digitalRead(pinA_);
    uint8_t b = digitalRead(pinB_);
    bool sw = digitalRead(pinSW_);

    noInterrupts();
    int32_t d = delta_;
    uint32_t isr = isrCount_;
    interrupts();

    Serial.printf("[Enc%d] A(pin%d)=%d  B(pin%d)=%d  SW(pin%d)=%d  "
                  "state=%d  delta=%d  ISRs=%u\n",
                  instanceIndex_, pinA_, a, pinB_, b, pinSW_, sw ? 1 : 0,
                  state_, d, isr);
}
