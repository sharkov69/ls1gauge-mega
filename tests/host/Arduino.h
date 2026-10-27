#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#include <deque>
using std::min;
using std::max;
using byte = uint8_t;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define CHANGE 3
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define memcpy_P memcpy
#define strlen_P strlen
#define strcmp_P strcmp
#define strncmp_P strncmp
#define snprintf_P snprintf
inline uint32_t testUs = 0;
inline uint32_t testStepUs = 0;
inline uint32_t testMillisOffset = 0;
inline int pins[256] = {};
inline bool irqEnabled = true;
inline uint32_t waitedMs = 0;
inline uint32_t micros() { testUs += testStepUs; return testUs; }
inline uint32_t millis() { testUs += testStepUs; return testMillisOffset + testUs / 1000; }
inline void delay(unsigned long ms) { waitedMs += ms; testUs += ms * 1000; }
inline void delayMicroseconds(unsigned int us) { testUs += us; }
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t pin) { return pins[pin]; }
inline void digitalWrite(uint8_t pin, uint8_t val) { pins[pin] = val; }
inline int digitalPinToInterrupt(uint8_t pin) { return pin; }
inline void attachInterrupt(int, void (*)(), int) {}
inline void detachInterrupt(int) {}
inline void noInterrupts() { irqEnabled = false; }
inline void interrupts() { irqEnabled = true; }
struct String : std::string {
    using std::string::string;
    String() = default;
    String(const std::string& s):std::string(s){}
    int indexOf(const char* s) const {auto p=find(s);return p==npos?-1:(int)p;}
};
struct FakeSerial {
    std::deque<char> rx;
    std::string tx;
    int room = 64;
    unsigned blockedWrites = 0;
    size_t rxCapacity = 65535;
    unsigned droppedRx = 0;
    int available() { return (int)rx.size(); }
    int read() { if (rx.empty()) return -1; char c=rx.front(); rx.pop_front(); return c; }
    int availableForWrite() { return room; }
    size_t write(uint8_t c) { if(room<=0) { ++blockedWrites; return 0; } --room; tx+=(char)c; return 1; }
    size_t write(const uint8_t* p,size_t n) { size_t i=0; for(;i<n;++i)write(p[i]); return i; }
    void println(const char* s) { while(*s)write(*s++); write('\r');write('\n'); }
    void println(const String& s) {println(s.c_str());}
    void flush() { ++blockedWrites; }
    void inject(const char* s) { while(*s){if(rx.size()<rxCapacity)rx.push_back(*s);else ++droppedRx;++s;} }
} inline Serial1;
