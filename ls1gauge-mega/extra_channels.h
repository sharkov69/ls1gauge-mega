#pragma once
#include <Arduino.h>
// Kept outside Cmd/LogEntry: the existing LTE record must stay exactly 42 bytes.
enum ExtraId : uint8_t { X_INJ1, X_INJ2, X_SA, X_PRNDL, X_TCC, X_SHIFT, X_AC, X_FAN, X_KR, X_CL, EXTRA_COUNT };
const uint16_t extraPids[EXTRA_COUNT] PROGMEM = {0x125a,0x125b,0x000e,0x1113,0x1102,0x1973,0x1100,0x1106,0x11a6,0x1105};
inline uint16_t extraPid(uint8_t i) { return pgm_read_word(extraPids+i); }
inline int8_t extraIndex(uint16_t pid) {
  for(uint8_t i=0;i<EXTRA_COUNT;++i) if(extraPid(i)==pid)return i;
  return -1;
}
struct ExtraChannels {
  uint16_t raw[EXTRA_COUNT] = {};
  uint32_t seen[EXTRA_COUNT] = {};
  uint16_t valid = 0;
  void set(uint8_t id,uint16_t value,uint32_t now) { raw[id]=value;seen[id]=now;valid|=(uint16_t)1<<id; }
  bool fresh(uint8_t id,uint32_t now,uint16_t age=500) const {
    return (valid&((uint16_t)1<<id)) && (uint32_t)(now-seen[id])<=age;
  }
  bool flagActive(uint8_t id,uint8_t mask,uint32_t now,uint16_t age=500) const { return fresh(id,now,age)&&(raw[id]&mask); }
  int16_t sparkHalfDegrees() const { return (int16_t)raw[X_SA]-128; }
  bool fuelCut(uint32_t now) const {
    // raw / 65.535 ms < 0.3 ms, on BOTH banks, including valid zero.
    return fresh(X_INJ1,now)&&fresh(X_INJ2,now)&&raw[X_INJ1]<=19&&raw[X_INJ2]<=19;
  }
};
static ExtraChannels extra;
