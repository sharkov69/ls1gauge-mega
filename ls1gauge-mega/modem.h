#pragma once
#include <math.h>
#include <stdlib.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

// One UART owner, fixed buffers, bounded RX/TX work on each loop iteration.
uint16_t modemRestartCount = 0;
static const uint8_t MODEM_BUDGET = 24;
static const char AT_OK[] PROGMEM = "OK";
struct AtTransaction {
	char command[80];
	char line[112];
	const char *expected = nullptr; // Flash string.
	uint32_t since = 0, quietSince = 0;
	uint32_t timeout = 0;
	uint8_t pos = 0, length = 0, phase = 0;
	bool busy = false, done = false, result = false;
	bool matched = false, ok = false, overflow = false;
	bool responded = false;
} at;

bool parseGpsLine(char *line) {
	if (strncmp_P(line, PSTR("+CGPSINFO:"), 10) != 0)
		return false;
	char *fields[9];
	uint8_t count = 1;
	fields[0] = line + 10;
	for (char *p = fields[0]; *p && count < 9; ++p) {
		if (*p == ',') {
			*p = 0;
			fields[count++] = p + 1;
		}
	}
	if (count != 9)
		return false;
	float coords[2];
	for (uint8_t i = 0; i < 2; ++i) {
		char *value = fields[i * 2];
		while (*value == ' ')
			++value;
		if (!*value)
			return false;
		char *end;
		double raw = strtod(value, &end);
		const char direction = fields[i * 2 + 1][0];
		if (*end || fields[i * 2 + 1][1] || !isfinite(raw) || raw < 0)
			return false;
		if (i == 0 ? (direction != 'N' && direction != 'S')
		           : (direction != 'E' && direction != 'W'))
			return false;
		if (raw > (i == 0 ? 9000.0 : 18000.0))
			return false;
		const int degrees = (int)(raw / 100);
		const double minutes = raw - degrees * 100;
		if (minutes >= 60)
			return false;
		coords[i] = (float)(degrees + minutes / 60.0);
		if (direction == 'S' || direction == 'W')
			coords[i] = -coords[i];
	}
	encodeCoord(coords[0], channels[LAT].value, gpsHigh[LAT - LAT]);
	encodeCoord(coords[1], channels[LON].value, gpsHigh[LON - LAT]);
	channels[LAT].set(channels[LAT].value);
	channels[LON].set(channels[LON].value);
	return true;
}

enum ModemOwner : uint8_t {
	OWNER_INIT,
	OWNER_IDLE,
	OWNER_NET_ATTACH,
	OWNER_NET_REG,
	OWNER_GPS,
	OWNER_HTTP,
	OWNER_RECOVERY,
	OWNER_POWER,
	OWNER_TIME
};
ModemOwner modemOwner = OWNER_INIT;
bool blinkState = false, modemOnline = false;
bool initReady = false, gpsRequested = false;
uint8_t stepIdx = 0, tries = 0;
uint32_t netLastCheck = 0, nextNetCheck = 0;
uint32_t recoveryQuietSince = 0;
uint8_t recoveryStep = 0;
uint8_t modemPowerStep = 0;
uint32_t modemPowerSince = 0, modemPowerNext = 30000UL;
const uint16_t NET_TIMEOUT = 20000;
#include "modem_debug.h"
void modemHttpTick();
bool modemHttpPending();
void cancelLogSend();
bool csvAtByte(uint8_t c);
bool csvAtLine(const char *line);
bool csvFastService();
extern uint32_t csvRetryAfter;
bool parseModemClock(const char *line, uint32_t &utc);
bool syncRtcUtc(uint32_t utc);
uint32_t modemTimeNext = 0, modemTimeUtc = 0;
uint8_t modemTimeStep = 0;
bool modemTimeDue() {
	return modemOnline && (int32_t)(millis() - modemTimeNext) >= 0;
}

void modemPowerKey(bool pressed) {
	// R104 removed: never drive the module's low-voltage PWRKEY with Mega's 5 V.
	digitalWrite(12, LOW);
	pinMode(12, pressed ? OUTPUT : INPUT);
}

void beginAT(const char *flashCommand, const char *flashExpected = AT_OK, uint32_t timeout = 2000) {
	if (at.busy || at.done)
		return;
	if (flashCommand) {
		size_t n = strlen_P(flashCommand);
		if (n >= sizeof(at.command))
			return;
		memcpy_P(at.command, flashCommand, n + 1);
	}
	at.expected = flashExpected;
	at.timeout = timeout;
	at.pos = 0;
	at.length = 0;
	at.phase = 0;
	at.busy = true;
	at.result = at.matched = at.ok = at.overflow = false;
	at.responded = false;
	at.since = millis();
	at.quietSince = at.since;
	modemDebugRecord('C', 'Q', at.command);
}

void finishAT(bool result) {
	modemDebugNumber(result ? 'O' : 'E', result ? 'K' : 'R', at.phase);
	at.result = result;
	at.phase = 3; // Drain trailing lines before handing the port to another operation.
	at.since = at.quietSince = millis();
}

void pumpAT() {
	if (!at.busy)
		return;
	const uint32_t now = millis();
	if (at.phase == 0 || at.phase == 3) {
		uint8_t budget = MODEM_BUDGET;
		while (budget-- && Serial1.available()) {
			char c = (char)Serial1.read();
			modemDebugRx(c, true);
			at.quietSince = now;
		}
		if (at.phase == 3) {
			if ((uint32_t)(now - at.quietSince) >= 50 || (uint32_t)(now - at.since) >= 250) {
				at.busy = false;
				at.done = true;
			}
			return;
		}
		if (Serial1.available()) {
			if ((uint32_t)(now - at.since) >= at.timeout) {
				modemDebugNumber('T', 'O', at.phase);
				finishAT(false);
			}
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
		if (at.pos == n + 2) {
			modemDebugNumber('T', 'X', n + 2);
			at.phase = 2;
			at.since = now;
		} else if ((uint32_t)(now - at.since) >= at.timeout) {
			modemDebugNumber('T', 'O', at.phase);
			finishAT(false);
		}
		return;
	}
	uint8_t budget = MODEM_BUDGET;
	while (budget-- && Serial1.available()) {
		char c = (char)Serial1.read();
		if (modemOwner == OWNER_HTTP && csvAtByte((uint8_t)c))
			continue;
		modemDebugRx(c, false);
		if (c == '\r')
			continue;
		if (c != '\n') {
			if ((size_t)at.length + 1 < sizeof(at.line))
				at.line[at.length++] = c;
			else
				at.overflow = true;
			continue;
		}
		at.line[at.length] = 0;
		if (at.overflow)
			modemDebugNumber('O', 'V', at.length);
		if (!at.overflow && at.length) {
			if (modemOwner == OWNER_HTTP && csvAtLine(at.line)) {
				at.length = 0;
				at.overflow = false;
				if (at.phase == 3) return;
				continue;
			}
			if (strcmp_P(at.line, PSTR("ERROR")) == 0 ||
			    strncmp_P(at.line, PSTR("+CME ERROR"), 10) == 0) {
				at.responded = true;
				if (modemOwner == OWNER_RECOVERY && recoveryStep == 1 &&
				    strcmp_P(at.line, PSTR("ERROR")) == 0)
					at.matched = true; // HTTPTERM also returns ERROR when no session exists.
				finishAT(false);
				return;
			}
			if (strcmp_P(at.line, PSTR("OK")) == 0) {
				at.ok = true;
				at.responded = true;
			}
			if (modemOwner == OWNER_TIME) {
				if (modemTimeStep == 1 && strncmp_P(at.line, PSTR("+CNTP:"), 6) == 0 &&
				    strcmp_P(at.line, PSTR("+CNTP: 0")) != 0) {
					finishAT(false);
					return;
				}
				if (modemTimeStep == 2 && parseModemClock(at.line, modemTimeUtc))
					at.matched = true;
			}
			if (modemOwner == OWNER_GPS && strncmp_P(at.line, PSTR("+CGPSINFO:"), 10) == 0) {
				parseGpsLine(at.line); // Empty/no-fix leaves the last position intact.
				at.matched = true;
			} else if (strcmp_P(at.line, at.expected) == 0 ||
			           (strncmp_P(at.line, PSTR("+HTTPACTION:"), 12) == 0 &&
			            strncmp_P(at.line, at.expected, strlen_P(at.expected)) == 0 &&
			            at.line[strlen_P(at.expected)] == ',')) {
				at.matched = true;
			}
			if (modemOwner == OWNER_NET_REG && (strcmp_P(at.line, PSTR("+CREG: 0,1")) == 0 ||
			                                    strcmp_P(at.line, PSTR("+CREG: 0,5")) == 0))
				at.matched = true;
			if (at.matched && (at.ok || strcmp_P(at.line, PSTR("DOWNLOAD")) == 0)) {
				finishAT(true);
				return;
			}
		}
		at.length = 0;
		at.overflow = false;
	}
	if ((uint32_t)(now - at.since) >= at.timeout) {
		modemDebugNumber('T', 'O', at.phase);
		finishAT(false);
	}
}

// Definitions and strings stay in Flash; no initialization-time heap allocations.
struct InitStep {
	const char *command;
	const char *expected;
	uint8_t mode;
};
// mode: 0 required, 1 successful query skips next command, 2 optional.
InitStep initStep(uint8_t index) {
	// SIM7600 initialization before CSV upload.
	switch (index) {
	case 0: return {PSTR("AT"), AT_OK, 0};
	case 1: return {PSTR("ATE0"), AT_OK, 0};
	case 2: return {PSTR("AT+CPIN?"), PSTR("+CPIN: READY"), 0};
	case 3: return {PSTR("AT+CNMP=2"), AT_OK, 0};
	case 4: return {PSTR("AT+CGDCONT=1,\"IP\",\"internet\""), AT_OK, 0};
	case 5: return {PSTR("AT+CGATT?"), PSTR("+CGATT: 1"), 0};
	case 6: return {PSTR("AT+NETOPEN?"), PSTR("+NETOPEN: 1"), 1};
	case 7: return {PSTR("AT+NETOPEN"), AT_OK, 0};
	case 8: return {PSTR("AT+HTTPTERM"), AT_OK, 2};
	case 9: return {PSTR("AT+CGPS?"), PSTR("+CGPS: 1,1"), 1};
	default: return {PSTR("AT+CGPS=1"), AT_OK, 0};
	}
}

void modemInitStart() {
	at.busy = at.done = false;
	stepIdx = tries = 0;
	initReady = modemOnline = false;
	gpsRequested = false;
	modemOwner = OWNER_INIT;
#if DEBUG_MODEM
	modemDebugMuted = false;
	modemDebugLength = 0;
#endif
	modemDebugNumber('S', 'T', OWNER_INIT);
}
bool modemInitDone() {
	return initReady;
}
void getGPSInfo() {
	gpsRequested = true;
}
// A GPS line exceeds the Mega core's 63-byte usable RX ring. While awaiting it,
// service RX/VPW before starting another slow TFT/SD/DHT operation or VPW TX.
bool modemNeedsFastService() {
	return (modemOwner == OWNER_GPS && at.busy && at.phase < 3)
	    || (modemOwner == OWNER_HTTP && csvFastService())
	    ;
}
void recoverModem() {
	cancelLogSend();
	at.busy = at.done = false;
	initReady = modemOnline = false;
	modemOwner = OWNER_RECOVERY;
	recoveryQuietSince = millis();
	recoveryStep = 0;
	tries = 0;
	modemDebugNumber('S', 'T', OWNER_RECOVERY);
}

void startModemPowerCycle() {
	if (modemRestartCount < UINT16_MAX)
		++modemRestartCount;
	cancelLogSend();
	at.busy = at.done = false;
	initReady = modemOnline = false;
	modemOwner = OWNER_POWER;
	modemPowerStep = 0;
	modemPowerSince = millis();
	modemPowerNext = modemPowerSince + 120000UL;
	modemPowerKey(true);
	modemDebugNumber('P', 'W', 0);
}

void modemPowerTick() {
	const uint32_t elapsed = millis() - modemPowerSince;
	if (modemPowerStep == 2) {
		pumpAT();
		if (!at.done)
			return;
		const bool ready = at.result;
		at.done = false;
		if (ready) {
			modemInitStart();
			return;
		}
		modemPowerKey(true);
		modemPowerStep = 3;
		modemPowerSince = millis();
		modemDebugNumber('P', 'W', 3);
		return;
	}
	uint8_t budget = MODEM_BUDGET;
	while (budget-- && Serial1.available()) {
		const char c = (char)Serial1.read();
		modemDebugRx(c, true);
	}
	if (modemPowerStep == 0 && elapsed >= 3000) {
		modemPowerKey(false);
		modemPowerStep = 1;
		modemPowerSince = millis();
		modemDebugNumber('P', 'W', 1);
	} else if (modemPowerStep == 1 && elapsed >= 30000) {
		// If already off, the long pulse may have started it. Probe before pressing again.
		modemPowerStep = 2;
		beginAT(PSTR("AT"));
	} else if (modemPowerStep == 3 && elapsed >= 500) {
		modemPowerKey(false);
		modemPowerStep = 4;
		modemPowerSince = millis();
		modemDebugNumber('P', 'W', 4);
	} else if (modemPowerStep == 4 && elapsed >= 15000) {
		modemInitStart();
	}
}

void modemTimeTick() {
	if (at.done) {
		const bool success = at.result;
		at.done = false;
		if (!success || modemTimeStep == 2) {
			if (success) {
				const bool adjusted = syncRtcUtc(modemTimeUtc);
				modemDebugNumber('R', adjusted ? 'A' : 'K', modemTimeUtc);
			} else {
				modemDebugNumber('R', 'E', modemTimeStep);
			}
			modemTimeNext = millis() + (success ? 21600000UL : 300000UL);
			modemOwner = OWNER_IDLE;
			return;
		}
		++modemTimeStep;
	}
	if (at.busy)
		return;
	switch (modemTimeStep) {
	case 0:
		beginAT(PSTR("AT+CNTP=\"pool.ntp.org\",0"));
		break;
	case 1:
		beginAT(PSTR("AT+CNTP"), PSTR("+CNTP: 0"), 30000UL);
		break;
	case 2:
		modemTimeUtc = 0;
		beginAT(PSTR("AT+CCLK?"), PSTR("+VALID_CLOCK"));
		break;
	}
}

void modemInitTick() {
	modemDebugTick();
	if (modemOwner == OWNER_POWER) {
		modemPowerTick();
		return;
	}
	if (modemOwner == OWNER_RECOVERY) {
		if (at.busy || at.done) {
			pumpAT();
			if (!at.done)
				return;
			const bool success = at.result;
			const bool noSession = recoveryStep == 1 && at.matched;
			at.done = false;
			if (success || noSession) {
				tries = 0;
				if (++recoveryStep == 3) {
					csvRetryAfter = millis();
					modemInitStart();
					return;
				}
			} else {
				if (at.responded)
					tries = 0;
				else if (++tries >= 3 && (int32_t)(millis() - modemPowerNext) >= 0) {
					startModemPowerCycle();
					return;
				}
				recoveryStep = 0;
				recoveryQuietSince = millis();
				return;
			}
		}
		uint8_t budget = MODEM_BUDGET;
		while (budget-- && Serial1.available()) {
			char c = (char)Serial1.read();
			modemDebugRx(c, true);
			recoveryQuietSince = millis();
		}
		// A late DOWNLOAD restarts the guard against sending commands as payload.
		if (Serial1.available() || (uint32_t)(millis() - recoveryQuietSince) < 5500)
			return;
		// CPAS proves command readiness; a fresh HTTP context excludes the old request.
		switch (recoveryStep) {
		case 0:
			beginAT(PSTR("AT+CPAS"), PSTR("+CPAS: 0"));
			break;
		case 1:
			beginAT(PSTR("AT+HTTPTERM"), AT_OK, 120000UL);
			break;
		case 2:
			beginAT(PSTR("AT+HTTPINIT"), AT_OK, 120000UL);
			break;
		}
		return;
	}
	pumpAT();
	if (modemOwner == OWNER_TIME) {
		modemTimeTick();
		return;
	}
	if (modemOwner == OWNER_INIT) {
		InitStep st = initStep(stepIdx);
		if (at.done) {
			bool success = at.result;
			at.done = false;
			if (success || st.mode != 0) {
				stepIdx += (success && st.mode == 1) ? 2 : 1;
				tries = 0;
			} else if (++tries >= 5) {
				if (stepIdx == 0 && !at.responded && (int32_t)(millis() - modemPowerNext) >= 0) {
					startModemPowerCycle();
					return;
				}
				// Network attachment can take minutes; do not restart radio configuration.
				if (stepIdx == 5 && at.responded) tries = 0;
				else
				stepIdx = tries = 0;
			}
			if (stepIdx >=
			    11
			) {
				initReady = modemOnline = true;
				modemOwner = OWNER_IDLE;
				modemDebugNumber('S', 'T', OWNER_IDLE);
				netLastCheck = millis();
				nextNetCheck = millis() + 4000;
			}
		} else if (!at.busy)
			beginAT(st.command, st.expected, 2000);
		return;
	}
	if (modemOwner == OWNER_HTTP) {
		modemHttpTick();
		return;
	}
	if (modemOwner == OWNER_GPS || modemOwner == OWNER_NET_ATTACH || modemOwner == OWNER_NET_REG) {
		if (!at.done)
			return;
		bool success = at.result;
		at.done = false;
		if (modemOwner == OWNER_NET_ATTACH && success) {
			modemOwner = OWNER_NET_REG;
		}
		if (modemOwner != OWNER_GPS) {
			modemOnline = success && modemOwner == OWNER_NET_REG;
			if (modemOnline)
				netLastCheck = millis();
			else if ((uint32_t)(millis() - netLastCheck) >= NET_TIMEOUT) {
				cancelLogSend();
				modemInitStart();
				return;
			}
			nextNetCheck = millis() + 4000;
		}
		modemOwner = OWNER_IDLE;
	}
	if (!modemHttpPending() && modemTimeDue()) {
		modemOwner = OWNER_TIME;
		modemTimeStep = 0;
		modemTimeTick();
		return;
	}
	if (modemHttpPending()) {
		modemOwner = OWNER_HTTP;
		modemHttpTick();
	} else if (gpsRequested) {
		gpsRequested = false;
		modemOwner = OWNER_GPS;
		beginAT(PSTR("AT+CGPSINFO"), PSTR("+CGPSINFO:"), 1000);
	} else if ((int32_t)(millis() - nextNetCheck) >= 0) {
		modemOwner = OWNER_NET_ATTACH;
		beginAT(PSTR("AT+CGATT?"), PSTR("+CGATT: 1"));
	}
}
