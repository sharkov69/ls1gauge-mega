#pragma once
#include <Arduino.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

// Six data bytes per packet. Offsets refer to those six bytes, not the header.
struct DpidField { uint16_t pid; uint8_t packet, offset, length; };
const DpidField DPID_FIELDS[] PROGMEM = {
  {0x000c,0,0,2}, {0x0011,0,2,1}, {0x000b,0,3,1},
  {0x000d,0,4,1}, {0x115c,0,5,1},
  {0x114b,1,0,1}, {0x114e,1,1,1}, {0x1141,1,2,1},
  {0x0005,1,3,1}, {0x000f,1,4,1}, {0x1940,1,5,1}
};
inline DpidField dpidField(uint8_t index) {
  DpidField field; memcpy_P(&field, &DPID_FIELDS[index], sizeof(field)); return field;
}
inline uint8_t dpidChannelCount(uint8_t packet) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < 11; ++i) if (dpidField(i).packet == packet) ++count;
  return count;
}

// Count requested channel values, not transport/control frames. A cyclic START
// counts each configured channel once; later autonomous samples belong to RX.
inline uint8_t vpwRequestedSamples(const uint8_t* data, uint8_t size) {
  if (size == 5 && data[3] == 0x01) return 1;
  if (size == 7 && data[3] == 0x22) return 1;
  if (size != 9 || data[3] != 0x2a || data[4] != 0x14) return 0;
  uint8_t mask = 0, count = 0;
  for (uint8_t i = 5; i < 9; ++i) {
    if (data[i] != 0xfe && data[i] != 0xfd) continue;
    uint8_t packet = (uint8_t)(0xfe - data[i]);
    if (!(mask & (1 << packet))) { mask |= (uint8_t)(1 << packet); count += dpidChannelCount(packet); }
  }
  return count;
}

// Wire frames include the header but omit CRC on TX. RX size includes CRC;
// J1850VPW must validate it before calling receive(). No allocations or waits.
class VpwDpid {
public:
  enum State : uint8_t { WAIT_ECU, STOP, DEFINE, START, STREAM, LEGACY };
  State state = WAIT_ECU;
  uint16_t packets[2] = {0,0};
  uint16_t failures = 0;
  uint8_t lastError = 0; // 1 timeout, 2 stale packet, otherwise ECU negative code.
  bool directPolling() const { return state == WAIT_ECU || state == LEGACY; }
  bool active() const { return state == STREAM; }
  uint8_t definitionIndex() const { return fieldIndex; }
  void ecuSeen() { if (state == WAIT_ECU) { state = STOP; fallback = false; } }

  // Returns packet index for accepted data, -1 for control/irrelevant frames.
  int8_t receive(const uint8_t* p, uint8_t size, uint32_t now) {
    if (!p || size < 6 || p[0] != 0x6c || p[1] != 0xf1 || p[2] != 0x10) return -1;
    if (size == 12 && p[3] == 0x6a && (p[4] == 0xfe || p[4] == 0xfd)) {
      sawStream = true;
      if (state == WAIT_ECU) ecuSeen(); // Clear a stream left by a previous reboot.
      if (state == LEGACY) { state = STOP; fallback = true; awaiting = false; }
      if ((state != START || !awaiting) && state != STREAM) return -1;
      uint8_t packet = (uint8_t)(0xfe - p[4]);
      seen[packet] = now; seenMask |= (uint8_t)(1 << packet); ++packets[packet];
      if (state == START && seenMask == 3) { state = STREAM; awaiting = false; keepSince = now; }
      return (int8_t)packet;
    }
    if (!awaiting) return -1;
    if (state == STOP && size == 6 && p[3] == 0x6a && p[4] == 0) {
      awaiting = false; sawStream = started = false;
      state = fallback ? LEGACY : DEFINE;
      return -1;
    }
    if (state == DEFINE && size == 7 && p[3] == 0x6c) {
      DpidField f = dpidField(fieldIndex);
      if (p[4] == 0xfe - f.packet && p[5] == selector(f)) {
        awaiting = false;
        if (++fieldIndex == 11) state = START;
      }
      return -1;
    }
    if (p[3] == 0x7f && (state == DEFINE || state == START || state == STOP)) {
      uint8_t request[10]; uint8_t n = makeRequest(request);
      // P59 echoes the complete service request before the negative code.
      if (size != n + 3 || memcmp(p + 4, request + 3, n - 3) != 0) return -1;
      uint8_t code = p[size - 2];
      if (code == 0x78 || (state == START && code == 0x23)) return -1;
      lastError = code;
      fail();
    }
    return -1;
  }

  bool tick(uint32_t now, bool canTransmit, bool (*transmit)(uint8_t*, uint8_t)) {
    if (state == STREAM && ((uint32_t)(now - seen[0]) >= 3000 || (uint32_t)(now - seen[1]) >= 3000)) {
      lastError = 2; fail();
    }
    if (awaiting && (uint32_t)(now - commandSince) >= 2000) {
      lastError = 1; fail();
    }
    if (!canTransmit || awaiting || directPolling()) return false;
    if (state == STREAM && (uint32_t)(now - keepSince) < 2000) return false;
    uint8_t request[10]; uint8_t n = makeRequest(request);
    if (!transmit(request, n)) return false;
    if (state == STREAM) keepSince = now;
    else {
      awaiting = true; commandSince = now;
      if (state == START) { started = true; seenMask = 0; }
    }
    return true;
  }
private:
  uint8_t fieldIndex = 0, seenMask = 0;
  bool awaiting = false, fallback = false, started = false, sawStream = false;
  uint32_t commandSince = 0, keepSince = 0, seen[2] = {0,0};
  static uint8_t selector(const DpidField& f) { return (uint8_t)(0x40 | ((f.offset + 1) << 3) | f.length); }
  uint8_t makeRequest(uint8_t* p) const {
    p[0] = 0x6c; p[1] = 0x10; p[2] = 0xf1;
    if (state == DEFINE) {
      DpidField f = dpidField(fieldIndex);
      p[3] = 0x2c; p[4] = (uint8_t)(0xfe - f.packet); p[5] = selector(f);
      p[6] = (uint8_t)(f.pid >> 8); p[7] = (uint8_t)f.pid; p[8] = p[9] = 0xff;
      return 10;
    }
    if (state == START) {
      p[3] = 0x2a; p[4] = 0x14; p[5] = p[7] = 0xfe; p[6] = p[8] = 0xfd;
      return 9;
    }
    if (state == STOP) { p[3] = 0x2a; p[4] = 0; return 5; }
    p[3] = 0x3f; return 4;
  }
  void fail() {
    ++failures; awaiting = false;
    if (started || sawStream) { state = STOP; fallback = true; }
    else state = LEGACY;
  }
};
