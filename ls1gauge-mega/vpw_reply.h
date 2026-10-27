#pragma once
#include <stdint.h>

// Size includes the wire CRC, already checked by J1850VPW.
inline bool validGaugeReply(const uint8_t* p, uint8_t size) {
  if (!p || size < 7 || p[2] != 0x10) return false;
  const bool physical = p[0] == 0x6C && p[1] == 0xF1;
  const bool functional = p[0] == 0x48 && p[1] == 0x6B;
  if (p[3] == 0x41 && (physical || functional)) {
    switch (p[4]) {
      case 0x0C: return size == 8;
      case 0x05: case 0x0B: case 0x0D: case 0x0F: case 0x11: return size == 7;
      default: return false;
    }
  }
  if (!physical || p[3] != 0x62 || size != 8) return false;
  return (p[4] == 0x19 && p[5] == 0x40) ||
         (p[4] == 0x11 && (p[5] == 0x41 || p[5] == 0x5C || p[5] == 0x4B || p[5] == 0x4E));
}
