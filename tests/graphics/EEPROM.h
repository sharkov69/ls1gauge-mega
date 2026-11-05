#pragma once
struct TraceEEPROM {
    uint8_t read(int) { return 0; }
    void write(int, uint8_t) {}
    void update(int, uint8_t) {}
} inline EEPROM;
