#include "SDManager.h"
#include <Arduino.h>

SDManager::SDManager() : available_(false), csPin_(SD_CS_PIN) {
}

bool SDManager::begin(uint8_t csPin) {
    csPin_ = csPin;

    // Explicitly configure CS pin
    pinMode(csPin_, OUTPUT);
    digitalWrite(csPin_, HIGH);

    // Initialize SPI for SD card
    Serial.println("[SD] SPI Begin...");
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, csPin_);
    delay(200); // Give it more time to settle

    // Use a much lower frequency for better compatibility (1MHz)
    Serial.println("[SD] Attempting initialization at 1MHz...");
    if (!SD.begin(csPin_, SPI, 1000000, "/sd", 5)) {
        Serial.println("[SD] Initialization failed at 1MHz! Trying 400kHz...");
        delay(500);
        if (!SD.begin(csPin_, SPI, 400000, "/sd", 5)) {
            Serial.println("[SD] Initialization failed at 400kHz!");
            available_ = false;
            return false;
        }
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
