#include "SDManager.h"
#include <Arduino.h>

SDManager::SDManager() : available_(false), csPin_(SD_CS_PIN), spiBus_(nullptr) {
}

bool SDManager::begin(uint8_t csPin) {
    csPin_ = csPin;

    // 1. Setup Pins
    pinMode(csPin_, OUTPUT);
    digitalWrite(csPin_, HIGH); // Deselect
    pinMode(SD_MISO_PIN, INPUT_PULLUP);

    // 2. Initialize dedicated SPI bus (VSPI)
    if (spiBus_) delete spiBus_;
    spiBus_ = new SPIClass(VSPI);
    spiBus_->begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1); // -1: Manual CS

    Serial.println("[SD] SPI (VSPI) instance created.");

    // 3. Hardware Handshake (Reset SD state)
    // Send 80+ clock cycles with CS high to enter SPI mode
    Serial.println("[SD] Handshake: Sending 80+ clock pulses with CS HIGH...");
    spiBus_->beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    digitalWrite(csPin_, HIGH);
    for (int i = 0; i < 20; i++) { // 160 pulses to be safe
        spiBus_->transfer(0xFF);
    }
    spiBus_->endTransaction();

    // Toggle CS just to "wake up" some controllers
    digitalWrite(csPin_, LOW);
    delay(10);
    digitalWrite(csPin_, HIGH);
    delay(100);

    // 4. Initialization Loop (Retries at 400kHz)
    bool success = false;
    for (int retry = 0; retry < 3; retry++) {
        Serial.printf("[SD] Attempt %d (400kHz)...\n", retry + 1);
        if (SD.begin(csPin_, *spiBus_, 400000, "/sd", 5)) {
            success = true;
            break;
        }
        delay(500);
    }

    if (!success) {
        Serial.println("[SD] Critical failure: SD.begin failed after retries.");
        available_ = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No SD card attached");
        available_ = false;
        return false;
    }

    Serial.print("[SD] Card Type: ");
    if (cardType == CARD_MMC) Serial.println("MMC");
    else if (cardType == CARD_SD) Serial.println("SDSC");
    else if (cardType == CARD_SDHC) Serial.println("SDHC");
    else Serial.println("UNKNOWN");

    available_ = true;
    Serial.printf("[SD] Card initialized. Size: %lluMB\n", SD.cardSize() / (1024 * 1024));

    return true;
}

void SDManager::listFiles(const char* dirName, uint8_t levels) {
    if (!available_) {
        Serial.println("[SD] Cannot list files: Card not available");
        return;
    }

    Serial.printf("[SD] Opening directory: %s\n", dirName);
    File root = SD.open(dirName);
    if (!root) {
        Serial.printf("[SD] Failed to open directory: %s\n", dirName);
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("[SD] Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listFiles(file.name(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

bool SDManager::exists(const char* path) {
    if (!available_) return false;
    return SD.exists(path);
}

bool SDManager::writeFile(const char* path, const uint8_t* data, size_t len) {
    if (!available_) return false;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[SD] Failed to open file for writing: %s\n", path);
        return false;
    }

    size_t written = file.write(data, len);
    file.close();

    return written == len;
}

bool SDManager::readFile(const char* path, uint8_t* data, size_t len) {
    if (!available_) return false;

    File file = SD.open(path);
    if (!file) {
        Serial.printf("[SD] Failed to open file for reading: %s\n", path);
        return false;
    }

    if (file.size() < len) {
        Serial.printf("[SD] File too small: %s\n", path);
        file.close();
        return false;
    }

    size_t read = file.read(data, len);
    file.close();

    return read == len;
}

uint64_t SDManager::getCardSize() const {
    if (!available_) return 0;
    return SD.cardSize();
}

uint64_t SDManager::getUsedBytes() const {
    if (!available_) return 0;
    return SD.usedBytes();
}
