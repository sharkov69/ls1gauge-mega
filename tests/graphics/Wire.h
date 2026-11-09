#pragma once
#include "RTClib.h"
extern RTC_DS3231 rtc;
struct TestWire {
    uint8_t status = 0, count = 7, index = 0;
    int badIndex = -1;
    uint8_t badValue = 0;
    unsigned requests = 0;
    void beginTransmission(uint8_t) {}
    void write(uint8_t) {}
    uint8_t endTransmission(bool = true) { return status; }
    uint8_t requestFrom(uint8_t, uint8_t) { ++requests; index = 0; return count; }
    int read() {
        int fields[] = {rtc.value.second(), rtc.value.minute(), rtc.value.hour(),
                        rtc.value.dayOfTheWeek() + 1, rtc.value.day(),
                        rtc.value.month(), rtc.value.year() - 2000};
        const int field = index++;
        return field == badIndex ? badValue : (fields[field] / 10) * 16 + fields[field] % 10;
    }
} Wire;
