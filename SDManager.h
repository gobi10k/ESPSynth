#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include "Config.h"
#include <SD.h>
#include <SPI.h>

class SDManager {
public:
    SDManager();

    bool begin(uint8_t csPin = SD_CS_PIN);
    bool isAvailable() const { return available_; }

    void listFiles(const char* dirName = "/", uint8_t levels = 0);
    bool exists(const char* path);

    bool writeFile(const char* path, const uint8_t* data, size_t len);
    bool readFile(const char* path, uint8_t* data, size_t len);

    uint64_t getCardSize() const;
    uint64_t getUsedBytes() const;

private:
    bool available_;
    uint8_t csPin_;
    SPIClass* spiBus_;
};

#endif
