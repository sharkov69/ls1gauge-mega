#pragma once
struct TraceEEPROM {
    uint8_t bytes[32] = {};
    uint8_t read(int address) { return bytes[address]; }
    void write(int address, uint8_t value) { bytes[address] = value; }
    void update(int address, uint8_t value) { bytes[address] = value; }
} inline EEPROM;
