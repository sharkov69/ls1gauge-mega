#pragma once
#include <math.h>
#include <stdlib.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

// One UART owner, fixed buffers, bounded RX/TX work on each loop iteration.
static const uint8_t MODEM_BUDGET = 24;
static const char AT_OK[] PROGMEM = "OK";
static const char LOGGER_URL[] PROGMEM = "AT+HTTPPARA=\"URL\",\"http://ls1.cyra.com.pl/logger.php\"";
struct AtTransaction {
  char command[80];
  char line[112];
  const char* expected = nullptr; // Flash string.
  uint32_t since = 0, quietSince = 0;
  uint16_t timeout = 0;
  uint8_t pos = 0, length = 0, phase = 0;
  bool busy = false, done = false, result = false;
  bool matched = false, ok = false, overflow = false;
} at;

bool parseGpsLine(char* line) {
  if (strncmp_P(line, PSTR("+CGPSINFO:"), 10) != 0) return false;
  char* fields[9]; uint8_t count = 1;
  fields[0] = line + 10;
  for (char* p = fields[0]; *p && count < 9; ++p) {
    if (*p == ',') { *p = 0; fields[count++] = p + 1; }
  }
  if (count != 9) return false;
  float coords[2];
  for (uint8_t i = 0; i < 2; ++i) {
    char* value = fields[i * 2];
    while (*value == ' ') ++value;
    if (!*value) return false;
    char* end; double raw = strtod(value, &end);
    const char direction = fields[i * 2 + 1][0];
    if (*end || fields[i * 2 + 1][1] || !isfinite(raw) || raw < 0) return false;
    if (i == 0 ? (direction != 'N' && direction != 'S') : (direction != 'E' && direction != 'W')) return false;
    if (raw > (i == 0 ? 9000.0 : 18000.0)) return false;
    const int degrees = (int)(raw / 100);
    const double minutes = raw - degrees * 100;
    if (minutes >= 60) return false;
    coords[i] = (float)(degrees + minutes / 60.0);
    if (direction == 'S' || direction == 'W') coords[i] = -coords[i];
  }
  encodeCoord(coords[0], cmd[LAT].wartosc, cmd[LAT].lastValue);
  encodeCoord(coords[1], cmd[LON].wartosc, cmd[LON].lastValue);
  return true;
}

enum ModemOwner : uint8_t { OWNER_INIT, OWNER_IDLE, OWNER_NET_ATTACH, OWNER_NET_REG, OWNER_GPS, OWNER_HTTP, OWNER_RECOVERY };
ModemOwner modemOwner = OWNER_INIT;
bool blinkState = false, modemOnline = false;
bool initReady = false, gpsRequested = false;
uint8_t stepIdx = 0, tries = 0;
uint32_t netLastCheck = 0, nextNetCheck = 0;
uint32_t recoveryQuietSince = 0;
const uint16_t NET_TIMEOUT = 20000;
void modemHttpTick();
bool modemHttpPending();
void cancelLogSend();

void beginAT(const char* flashCommand, const char* flashExpected = AT_OK, uint16_t timeout = 2000) {
  if (at.busy || at.done) return;
  if (flashCommand) {
    size_t n = strlen_P(flashCommand);
    if (n >= sizeof(at.command)) return;
    memcpy_P(at.command, flashCommand, n + 1);
  }
  at.expected = flashExpected; at.timeout = timeout;
  at.pos = 0; at.length = 0; at.phase = 0;
  at.busy = true; at.result = at.matched = at.ok = at.overflow = false;
  at.since = millis(); at.quietSince = at.since;
}

void finishAT(bool result) {
  at.result = result;
  at.phase = 3; // Drain trailing lines before handing the port to another operation.
  at.since = at.quietSince = millis();
}

void pumpAT() {
  if (!at.busy) return;
  const uint32_t now = millis();
  if (at.phase == 0 || at.phase == 3) {
    uint8_t budget = MODEM_BUDGET;
    while (budget-- && Serial1.available()) { Serial1.read(); at.quietSince = now; }
    if (at.phase == 3) {
      if ((uint32_t)(now - at.quietSince) >= 50 || (uint32_t)(now - at.since) >= 250) { at.busy = false; at.done = true; }
      return;
    }
    if (Serial1.available()) {
      if ((uint32_t)(now - at.since) >= at.timeout) finishAT(false);
      return;
    }
    at.phase = 1;
  }
  if (at.phase == 1) {
    uint8_t budget = MODEM_BUDGET;
    const uint8_t n = (uint8_t)strlen(at.command);
    while (budget-- && Serial1.availableForWrite() > 0 && at.pos < n + 2) {
      Serial1.write((uint8_t)(at.pos < n ? at.command[at.pos] : at.pos == n ? '\r' : '\n'));
      ++at.pos;
    }
    if (at.pos == n + 2) { at.phase = 2; at.since = now; }
    else if ((uint32_t)(now - at.since) >= at.timeout) finishAT(false);
    return;
  }
  uint8_t budget = MODEM_BUDGET;
  while (budget-- && Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if ((size_t)at.length + 1 < sizeof(at.line)) at.line[at.length++] = c;
      else at.overflow = true;
      continue;
    }
    at.line[at.length] = 0;
    if (!at.overflow && at.length) {
      if (strcmp_P(at.line, PSTR("ERROR")) == 0 || strncmp_P(at.line, PSTR("+CME ERROR"), 10) == 0) {
        finishAT(false); return;
      }
      if (strcmp_P(at.line, PSTR("OK")) == 0) at.ok = true;
      if (modemOwner == OWNER_GPS && strncmp_P(at.line, PSTR("+CGPSINFO:"), 10) == 0) {
        parseGpsLine(at.line); // Empty/no-fix leaves the last position intact.
        at.matched = true;
      } else if (strcmp_P(at.line, at.expected) == 0 ||
                 (strncmp_P(at.line, PSTR("+HTTPACTION:"), 12) == 0 &&
                  strncmp_P(at.line, at.expected, strlen_P(at.expected)) == 0 &&
                  at.line[strlen_P(at.expected)] == ',') ||
                 (strcmp_P(at.expected, PSTR("READY")) == 0 && strcmp_P(at.line, PSTR("+CPIN: READY")) == 0)) {
        at.matched = true;
      }
      if (modemOwner == OWNER_NET_REG &&
          (strcmp_P(at.line, PSTR("+CREG: 0,1")) == 0 || strcmp_P(at.line, PSTR("+CREG: 0,5")) == 0)) at.matched = true;
      if (at.matched && (at.ok || strcmp_P(at.expected, PSTR("DOWNLOAD")) == 0)) {
        finishAT(true); return;
      }
    }
    at.length = 0; at.overflow = false;
  }
  if ((uint32_t)(now - at.since) >= at.timeout) finishAT(false);
}

// Definitions and strings stay in Flash; no initialization-time heap allocations.
struct InitStep { const char* command; const char* expected; uint8_t mode; };
// mode: 0 required, 1 successful query skips next command, 2 optional.
InitStep initStep(uint8_t index) {
  switch (index) {
    case 0: return {PSTR("AT"), PSTR("OK"), 0};
    case 1: return {PSTR("AT+CPIN?"), PSTR("READY"), 0};
    case 2: return {PSTR("AT+CNMP=13"), PSTR("OK"), 0};
    case 3: return {PSTR("AT+CGDCONT=1,\"IP\",\"internet\""), PSTR("OK"), 0};
    case 4: return {PSTR("AT+CREG?"), PSTR("+CREG: 0,1"), 0};
    case 5: return {PSTR("ATE0"), PSTR("OK"), 0};
    case 6: return {PSTR("AT+NETOPEN?"), PSTR("+NETOPEN: 1"), 1};
    case 7: return {PSTR("AT+NETOPEN"), PSTR("OK"), 0};
    case 8: return {PSTR("AT+IPADDR"), PSTR("OK"), 0};
    case 9: return {PSTR("AT+HTTPTERM"), PSTR("OK"), 2};
    case 10: return {PSTR("AT+HTTPINIT"), PSTR("OK"), 0};
    case 11: return {PSTR("AT+HTTPPARA=\"CID\",1"), PSTR("OK"), 0};
    case 12: return {LOGGER_URL, PSTR("OK"), 0};
    case 13: return {PSTR("AT+HTTPACTION=0"), PSTR("+HTTPACTION: 0,403"), 0};
    case 14: return {PSTR("AT+HTTPTERM"), PSTR("OK"), 0};
    case 15: return {PSTR("AT+CGPS?"), PSTR("+CGPS: 1,1"), 1};
    default: return {PSTR("AT+CGPS=1"), PSTR("OK"), 0};
  }
}

void modemInitStart() {
  at.busy = at.done = false;
  stepIdx = tries = 0; initReady = modemOnline = false;
  gpsRequested = false; modemOwner = OWNER_INIT;
}
bool modemInitDone() { return initReady; }
void getGPSInfo() { gpsRequested = true; }
// A GPS line exceeds the Mega core's 63-byte usable RX ring. While awaiting it,
// service RX/VPW before starting another slow TFT/SD/DHT operation or VPW TX.
bool modemNeedsFastService() {
  return modemOwner == OWNER_GPS && at.busy && at.phase < 3;
}
void recoverModem() {
  cancelLogSend();
  at.busy = at.done = false;
  initReady = modemOnline = false;
  modemOwner = OWNER_RECOVERY;
  recoveryQuietSince = millis();
}

void modemInitTick() {
  if (modemOwner == OWNER_RECOVERY) {
    uint8_t budget = MODEM_BUDGET;
    while (budget-- && Serial1.available()) { Serial1.read(); recoveryQuietSince = millis(); }
    // HTTPDATA advertises a 5000 ms input window. Never send AT command bytes
    // into a possibly active payload window; late DOWNLOAD restarts this guard.
    if ((uint32_t)(millis() - recoveryQuietSince) >= 5500) modemInitStart();
    return;
  }
  pumpAT();
  if (modemOwner == OWNER_INIT) {
    InitStep st = initStep(stepIdx);
    if (at.done) {
      bool success = at.result; at.done = false;
      if (success || st.mode != 0) {
        stepIdx += (success && st.mode == 1) ? 2 : 1; tries = 0;
      } else if (++tries >= 5) { stepIdx = tries = 0; }
      if (stepIdx >= 17) {
        initReady = modemOnline = true; modemOwner = OWNER_IDLE;
        netLastCheck = millis(); nextNetCheck = millis() + 4000;
      }
    } else if (!at.busy) beginAT(st.command, st.expected, 2000);
    return;
  }
  if (modemOwner == OWNER_HTTP) { modemHttpTick(); return; }
  if (modemOwner == OWNER_GPS || modemOwner == OWNER_NET_ATTACH || modemOwner == OWNER_NET_REG) {
    if (!at.done) return;
    bool success = at.result; at.done = false;
    if (modemOwner == OWNER_NET_ATTACH && success) {
      modemOwner = OWNER_NET_REG; beginAT(PSTR("AT+CREG?"), PSTR("+CREG: 0,1")); return;
    }
    if (modemOwner != OWNER_GPS) {
      modemOnline = success && modemOwner == OWNER_NET_REG;
      if (modemOnline) netLastCheck = millis();
      else if ((uint32_t)(millis() - netLastCheck) >= NET_TIMEOUT) { cancelLogSend(); modemInitStart(); return; }
      nextNetCheck = millis() + 4000;
    }
    modemOwner = OWNER_IDLE;
  }
  if (modemHttpPending()) { modemOwner = OWNER_HTTP; modemHttpTick(); }
  else if (gpsRequested) { gpsRequested = false; modemOwner = OWNER_GPS; beginAT(PSTR("AT+CGPSINFO"), PSTR("+CGPSINFO:"), 1000); }
  else if ((int32_t)(millis() - nextNetCheck) >= 0) {
    modemOwner = OWNER_NET_ATTACH; beginAT(PSTR("AT+CGATT?"), PSTR("+CGATT: 1"));
  }
}
