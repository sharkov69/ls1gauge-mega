#pragma once
#include <Arduino.h>
#define TX 5
#define RX 3
#ifndef VPW_DPID_ENABLED
#define VPW_DPID_ENABLED 1
#endif
#ifdef DEBUGVPW
void handleError(J1850_Operations op, J1850_ERRORS err);
#endif
J1850VPW vpw;

/*
SEND
byte 0 - priority
0x68 01101000
0x6C 01101100
Bits 7,6 and 5 are priority 0=High, 7=Low
Bit 4 is header style (0=3 byte header-GM)
Bit 3 is In Frame Response (1=Not allowed-GM)
Bit 2 is addressing mode (0=Functional)
Bit 1,0 is message type: (depending on bit 2 and 3 see below)

Bit 3 2 1 0
-----------
Functional
1 0 0 0 Function
Physical
1 1 0 0 Node to Node

byte2 - destination
0x10 - PCM (physical)
0x6A - Request Legislated Diagnostics (functional)

byte3 - source
0xF1 - Off-board scan tool

byte4 - PCM mode requests
0x01 - Request Current Powertrain Diagnostic Data (functional)
0x22 - Request Diagnostic Data by PID (physical)

//READ
byte1 0x48 - Report Control of On-Board System, Test, or Component
byte2 0x6B -  Report Legislated Dianostics
*/

// Base order is fixed by the binary LTE API. CSV has its own column order.
enum ChannelId : uint8_t {
	AIR,
	MAP,
	TPS,
	BATT,
	O22,
	SPEED,
	TRANS,
	OIL,
	RPM,
	WATER,
	O2,
	TEMP_AC,
	HUM,
	TEMP_OUT,
	TEMP_IN,
	LAT,
	LON,
	BASE_CHANNEL_COUNT,
	INJ1 = BASE_CHANNEL_COUNT,
	INJ2,
	SA,
	PRNDL,
	TCC,
	SHIFT_MODE,
	AC_STATE,
	FAN_STATE,
	KR,
	CL_STATE,
	CHANNEL_COUNT
};
constexpr uint8_t EXTRA_COUNT = CHANNEL_COUNT - BASE_CHANNEL_COUNT;

struct Channel {
	int16_t value = -32768;
	uint32_t updatedAt = 0;
	bool valid = false;

	void setRaw(uint16_t bits, uint32_t now) {
		value = (int16_t)bits;
		updatedAt = now;
		valid = true;
	}
	void set(int16_t measurement) {
		setRaw((uint16_t)measurement, millis());
	}
	uint16_t raw() const {
		return (uint16_t)value;
	}
	bool fresh(uint32_t now, uint16_t maxAge = 500) const {
		return valid && (uint32_t)(now - updatedAt) <= maxAge;
	}
	bool flagActive(uint8_t mask, uint32_t now, uint16_t maxAge = 500) const {
		return fresh(now, maxAge) && (raw() & mask);
	}
};
Channel channels[CHANNEL_COUNT];

struct EcuLogGate {
	uint32_t batteryAt = 0;
	bool batteryReceived = false;
	void receivedBattery(uint32_t now) {
		batteryAt = now;
		batteryReceived = true;
	}
	bool allowed(uint32_t now) const {
		// Allow three fallback Battery intervals, including unchanged readings.
		return batteryReceived && (uint32_t)(now - batteryAt) <= 15000;
	}
} ecuLogGate;

// Presentation state and GPS encoding must never share measurement storage.
int16_t lastDisplayed[LAT] = {-32768, -32768, -32768, -32768, -32768, -32768, -32768, -32768,
                              -32768, -32768, -32768, -32768, -32768, -32768, -32768};
int16_t gpsHigh[2] = {-32768, -32768};

// One catalog for DPID definitions, direct PID requests and polling intervals.
// Offsets are relative to the six payload bytes, excluding the VPW header.
struct DpidField {
	uint16_t pid;
	ChannelId channel;
	uint8_t packet;
	uint8_t offset;
	uint8_t length;
	uint16_t intervalMs;
};
const DpidField DPID_FIELDS[] PROGMEM = {
    {0x000c, RPM, 0, 0, 2, 250},        {0x0011, TPS, 0, 2, 1, 250},
    {0x000b, MAP, 0, 3, 1, 250},        {0x000d, SPEED, 0, 4, 1, 250},
    {0x115c, OIL, 0, 5, 1, 5000},       {0x114b, O2, 1, 0, 1, 250},
    {0x114e, O22, 1, 1, 1, 250},        {0x1141, BATT, 1, 2, 1, 5000},
    {0x0005, WATER, 1, 3, 1, 5000},     {0x000f, AIR, 1, 4, 1, 5000},
    {0x1940, TRANS, 1, 5, 1, 5000},     {0x125a, INJ1, 2, 0, 2, 1000},
    {0x125b, INJ2, 2, 2, 2, 1000},      {0x000e, SA, 2, 4, 1, 250},
    {0x1113, PRNDL, 2, 5, 1, 1000},     {0x1102, TCC, 3, 0, 1, 1000},
    {0x1973, SHIFT_MODE, 3, 1, 1, 1000}, {0x1100, AC_STATE, 3, 2, 1, 2000},
    {0x1106, FAN_STATE, 3, 3, 1, 2000},  {0x11a6, KR, 3, 4, 1, 250},
    {0x1105, CL_STATE, 3, 5, 1, 2000}};
constexpr uint8_t DPID_FIELD_COUNT = sizeof(DPID_FIELDS) / sizeof(DPID_FIELDS[0]);
constexpr uint8_t DPID_PACKET_COUNT = 4;
constexpr uint8_t POLL_COUNT = DPID_FIELD_COUNT;
uint16_t lastRequestAt[POLL_COUNT] = {0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
                                    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
                                    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
                                    0xffff, 0xffff, 0xffff};

// Preserve the original tie-breaking order; skip local sensors and GPS.
uint8_t pollChannel(uint8_t index) {
	constexpr uint8_t basePidCount = O2 + 1;
	return index < basePidCount ? index : BASE_CHANNEL_COUNT + index - basePidCount;
}

DpidField dpidField(uint8_t index) {
	DpidField field;
	memcpy_P(&field, DPID_FIELDS + index, sizeof(field));
	return field;
}
DpidField fieldForChannel(uint8_t channel) {
	for (uint8_t i = 0; i < DPID_FIELD_COUNT; ++i) {
		if (pgm_read_byte(&DPID_FIELDS[i].channel) == channel)
			return dpidField(i);
	}
	return {}; // Local sensors and GPS have no ECU request.
}
int8_t channelForExtraPid(uint16_t pid) {
	for (uint8_t i = 0; i < DPID_FIELD_COUNT; ++i) {
		const uint8_t channel = pgm_read_byte(&DPID_FIELDS[i].channel);
		if (channel >= BASE_CHANNEL_COUNT && pgm_read_word(&DPID_FIELDS[i].pid) == pid)
			return channel;
	}
	return -1;
}
uint8_t dpidChannelCount(uint8_t packet) {
	uint8_t count = 0;
	for (uint8_t i = 0; i < DPID_FIELD_COUNT; ++i) {
		if (pgm_read_byte(&DPID_FIELDS[i].packet) == packet)
			++count;
	}
	return count;
}
uint8_t makePidRequest(uint16_t pid, uint8_t *request) {
	request[2] = 0xf1;
	if (pid <= 0xff) {
		request[0] = 0x68;
		request[1] = 0x6a;
		request[3] = 0x01;
		request[4] = (uint8_t)pid;
		return 5;
	}
	request[0] = 0x6c;
	request[1] = 0x10;
	request[3] = 0x22;
	request[4] = (uint8_t)(pid >> 8);
	request[5] = (uint8_t)pid;
	request[6] = 0x01;
	return 7;
}
bool fuelCut(uint32_t now) {
	// Both fresh banks must report less than 0.3 ms; valid zero is allowed.
	return channels[INJ1].fresh(now) && channels[INJ2].fresh(now) && channels[INJ1].raw() <= 19 &&
	       channels[INJ2].raw() <= 19;
}

// Wire frames include the header but omit CRC on TX. RX size includes CRC;
// J1850VPW must validate it before calling receive(). No allocations or waits.
class VpwDpid {
  public:
	enum State : uint8_t { WAIT_ECU, STOP, DEFINE, START, WAIT_STREAM, STREAM, LEGACY };
	State state = WAIT_ECU;
	uint16_t packets[4] = {};
	uint16_t failures = 0;
	uint8_t lastError = 0; // 1 timeout, 2 stale packet, otherwise ECU negative code.
	bool directPolling() const {
		return state == WAIT_ECU || state == LEGACY;
	}
	bool active() const {
		return state == STREAM;
	}
	uint8_t definitionIndex() const {
		return fieldIndex;
	}
	void ecuSeen() {
		// Retry only while the ECM still answers individual PID requests.
		if (state == LEGACY && (uint32_t)(millis() - fallbackSince) >= 30000) {
			state = WAIT_ECU;
			fieldIndex = startGroup = seenMask = 0;
		}
		if (state == WAIT_ECU) {
			state = STOP;
			fallback = false;
		}
	}

	// Returns packet index for accepted data, -1 for control/irrelevant frames.
	int8_t receive(const uint8_t *p, uint8_t size, uint32_t now) {
		if (!p || size < 6 || p[0] != 0x6c || p[1] != 0xf1 || p[2] != 0x10)
			return -1;
		if (size == 12 && p[3] == 0x6a && p[4] >= 0xfb && p[4] <= 0xfe) {
			sawStream = true;
			if (state == WAIT_ECU)
				ecuSeen(); // Clear a stream left by a previous reboot.
			if (state == LEGACY) {
				state = STOP;
				fallback = true;
				awaiting = false;
			}
			if (state != WAIT_STREAM && state != STREAM)
				return -1;
			uint8_t packet = (uint8_t)(0xfe - p[4]);
			seen[packet] = now;
			seenMask |= (uint8_t)(1 << packet);
			++packets[packet];
			if (state == WAIT_STREAM && seenMask == 15) {
				state = STREAM;
				awaiting = false;
				keepSince = now;
			}
			return (int8_t)packet;
		}
		if (!awaiting)
			return -1;
		if (state == STOP && size == 6 && p[3] == 0x6a && p[4] == 0) {
			awaiting = false;
			sawStream = started = false;
			state = fallback ? LEGACY : DEFINE;
			return -1;
		}
		if (state == DEFINE && size == 7 && p[3] == 0x6c) {
			DpidField f = dpidField(fieldIndex);
			if (p[4] == 0xfe - f.packet && p[5] == selector(f)) {
				awaiting = false;
				if (++fieldIndex == DPID_FIELD_COUNT)
					state = START;
			}
			return -1;
		}
		if (p[3] == 0x7f && (state == DEFINE || state == START || state == STOP)) {
			uint8_t request[10];
			uint8_t n = makeRequest(request);
			// P59 echoes the complete service request before the negative code.
			if (size != n + 3 || memcmp(p + 4, request + 3, n - 3) != 0)
				return -1;
			uint8_t code = p[size - 2];
			if (code == 0x78)
				return -1;
			if (state == START && code == 0x23) {
				if (startGroup == 0) {
					startGroup = 1;
					awaiting = false;
				} else {
					state = WAIT_STREAM;
					commandSince = now;
					seenMask = 0;
				}
				return -1;
			}
			lastError = code;
			fail(now);
		}
		return -1;
	}

	bool tick(uint32_t now, bool canTransmit, bool (*transmit)(uint8_t *, uint8_t)) {
		if (state == STREAM)
			for (uint8_t i = 0; i < DPID_PACKET_COUNT; ++i)
				if ((uint32_t)(now - seen[i]) >= 3000) {
					lastError = 2;
					fail(now);
					break;
				}
		if (awaiting && (uint32_t)(now - commandSince) >= 2000) {
			lastError = 1;
			fail(now);
		}
		if (!canTransmit || awaiting || directPolling())
			return false;
		if (state == STREAM && (uint32_t)(now - keepSince) < 2000)
			return false;
		uint8_t request[10];
		uint8_t n = makeRequest(request);
		if (!transmit(request, n))
			return false;
		if (state == STREAM)
			keepSince = now;
		else {
			awaiting = true;
			commandSince = now;
			if (state == START) {
				started = true;
				seenMask = 0;
			}
		}
		return true;
	}

  private:
	uint8_t fieldIndex = 0, seenMask = 0, startGroup = 0;
	bool awaiting = false, fallback = false, started = false, sawStream = false;
	uint32_t commandSince = 0, keepSince = 0, fallbackSince = 0, seen[4] = {};
	static uint8_t selector(const DpidField &f) {
		return (uint8_t)(0x40 | ((f.offset + 1) << 3) | f.length);
	}
	uint8_t makeRequest(uint8_t *p) const {
		p[0] = 0x6c;
		p[1] = 0x10;
		p[2] = 0xf1;
		if (state == DEFINE) {
			DpidField f = dpidField(fieldIndex);
			p[3] = 0x2c;
			p[4] = (uint8_t)(0xfe - f.packet);
			p[5] = selector(f);
			p[6] = (uint8_t)(f.pid >> 8);
			p[7] = (uint8_t)f.pid;
			p[8] = p[9] = 0xff;
			return 10;
		}
		if (state == START) {
			p[3] = 0x2a;
			p[4] = startGroup ? 0x24 : 0x14;
			p[5] = 0xfe;
			p[6] = 0xfd;
			p[7] = 0xfc;
			p[8] = 0xfb;
			return 9;
		}
		if (state == STOP) {
			p[3] = 0x2a;
			p[4] = 0;
			return 5;
		}
		p[3] = 0x3f;
		return 4;
	}
	void fail(uint32_t now) {
		++failures;
		fallbackSince = now;
		awaiting = false;
		if (started || sawStream) {
			state = STOP;
			fallback = true;
		} else
			state = LEGACY;
	}
};

VpwDpid dpid;
#include <stdint.h>

// Size includes the wire CRC, already checked by J1850VPW.
inline bool validGaugeReply(const uint8_t *p, uint8_t size) {
	if (!p || size < 7 || p[2] != 0x10)
		return false;
	const bool physical = p[0] == 0x6C && p[1] == 0xF1;
	const bool functional = p[0] == 0x48 && p[1] == 0x6B;
	if (p[3] == 0x41 && (physical || functional)) {
		switch (p[4]) {
		case 0x0C:
			return size == 8;
		case 0x05:
		case 0x0B:
		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x11:
			return size == 7;
		default:
			return false;
		}
	}
	if (!physical || p[3] != 0x62 || size < 8)
		return false;
	const int8_t x = channelForExtraPid(((uint16_t)p[4] << 8) | p[5]);
	if (x >= 0)
		return size == (x == INJ1 || x == INJ2 ? 9 : 8);
	if (size != 8)
		return false;
	return (p[4] == 0x19 && p[5] == 0x40) ||
	       (p[4] == 0x11 && (p[5] == 0x41 || p[5] == 0x5C || p[5] == 0x4B || p[5] == 0x4E));
}

uint16_t czas = 0;
volatile uint16_t lastRxEnd = 0;

#define O2SREDNIA 4 // Wideband AFR: shorter moving average for both banks.
struct objO2 {
	int16_t o2[O2SREDNIA];
	uint8_t id;
	bool ready = false;
};

#define O2SENSORS 2
objO2 o2[O2SENSORS] = {};

enum { O2_1 = 0, O2_2 = 1 };

int16_t updateO2(objO2 &o2, int16_t v) {
	if (v < 1029 || v > 1911)
		v = 1475;
	if (!o2.ready) {
		for (uint8_t i = 0; i < O2SREDNIA; i++)
			o2.o2[i] = v;
		o2.id = 0;
		o2.ready = true;
	} else {
		if (o2.id >= O2SREDNIA)
			o2.id = 0;
		o2.o2[o2.id] = v;
		o2.id++;
	}

	int16_t sum = 0;
	for (uint8_t i = 0; i < O2SREDNIA; i++)
		sum += o2.o2[i];
	return sum / O2SREDNIA;
}

uint16_t lastSend = 0;
bool transmitVpw(uint8_t *data, uint8_t size) {
	bool sent = vpw.send(data, size, 1);
	lastSend = (uint16_t)millis();
	return sent;
}
void send() {
	czas = (uint16_t)millis(); // read() may have advanced lastRxEnd since loop entry.
	const uint16_t guardRx = 10;
	const bool canTransmit =
	    (uint16_t)(czas - lastSend) >= pause && (uint16_t)(czas - lastRxEnd) >= guardRx;
#if VPW_DPID_ENABLED
	dpid.tick(millis(), canTransmit, transmitVpw);
	if (!dpid.directPolling())
		return;
#endif
	// Every ECU PID competes by elapsed time minus its own requested interval.
	uint8_t id = 0xFF;
	int32_t max = -32000;

	for (uint8_t i = 0; i < POLL_COUNT; i++) {
		const DpidField request = fieldForChannel(pollChannel(i));
		if (lastRequestAt[i] == 0xFFFF || (uint16_t)(czas - lastRequestAt[i]) > 0x8000) {
			lastRequestAt[i] = czas - (uint16_t)((i * pause) % request.intervalMs);
		}
		uint16_t diff = czas - lastRequestAt[i];
		if (diff >= request.intervalMs) {
			int32_t s = (int32_t)diff - request.intervalMs;
			if (s > max) {
				max = s;
				id = i;
			}
		}
	}
	if (id != 0xFF && (uint16_t)(czas - lastSend) >= pause &&
	    (uint16_t)(czas - lastRxEnd) >= guardRx) {
		const DpidField field = fieldForChannel(pollChannel(id));
		uint8_t request[7];
		const uint8_t size = makePidRequest(field.pid, request);
		bool sent = transmitVpw(request, size);
#ifdef TXRX
		DBG(F("TX: "));
		for (uint8_t j = 0; j < size; j++) {
			if (request[j] < 0x10)
				DBG("0");
			DBG(request[j], HEX);
			DBG(" ");
		}
		DBGL();
#endif
		czas = (uint16_t)(millis() & 0xFFFF);
		if (sent)
			lastRequestAt[id] = czas;
		lastSend = czas;
	}
}

void decodeGaugeReply(const uint8_t *pData, uint8_t dataSize) {
	if (validGaugeReply(pData, dataSize)) {
		const uint16_t fullPid = pData[3] == 0x41 ? pData[4] : ((uint16_t)pData[4] << 8) | pData[5];
		const int8_t x = channelForExtraPid(fullPid);
		if (x >= 0) {
			const uint8_t offset = pData[3] == 0x41 ? 5 : 6;
			const uint16_t raw = (x == INJ1 || x == INJ2)
			                         ? ((uint16_t)pData[offset] << 8) | pData[offset + 1]
			                         : pData[offset];
			channels[(uint8_t)x].setRaw(raw, millis());
			return;
		}
#ifdef TXRX
		DBG(F("RX:"));
		for (uint8_t i = 0; i < dataSize; i++) {
			DBG(F(" "));
			if (pData[i] < 0x10)
				DBG("0");
			DBG(pData[i], HEX);
		}
		DBGL();
#endif
		uint8_t pid = pData[4];
		switch (pid) {
		case 0x0C: { // rpm
			if (dataSize >= 7) {
				channels[RPM].set(((uint16_t)pData[5] << 8 | pData[6]) / 4);
#ifdef TXRX
				DBG(F("RPM: "));
				DBGL(channels[RPM].value);
#endif
			}
			break;
		}
		case 0x0D: {
			channels[SPEED].set(pData[5]);
#ifdef TXRX
			DBG(F("Speed: "));
			DBG(channels[SPEED].value);
			DBGL(F(" km/h"));
#endif
			break;
		}
		case 0x05: {
			channels[WATER].set(pData[5] - 40);
#ifdef TXRX
			DBG(F("Water Temp: "));
			DBG(channels[WATER].value);
			DBGL(F(" *C"));
#endif
			break;
		}
		case 0x0F: {
			channels[AIR].set(pData[5] - 40);
#ifdef TXRX
			DBG(F("Air Temp: "));
			DBG(channels[AIR].value);
			DBGL(F(" *C"));
#endif
			break;
		}

		case 0x11: {
			if (pData[3] == 0x41) {
				channels[TPS].set(pData[5] * 100 / 255);
#ifdef TXRX
				DBG(F("TPS: "));
				DBG(channels[TPS].value);
				DBGL(F(" %"));
#endif

			} else if (pData[3] == 0x62 && dataSize >= 7) {

				switch (pData[5]) {
				case 0x41: {
					channels[BATT].set(pData[6]);
					ecuLogGate.receivedBattery(millis());
#ifdef TXRX
					DBG(F("Battery: "));
					DBG(channels[BATT].value / 10.0, 1);
					DBGL(F(" V"));
#endif
					break;
				}
				case 0x5C: {
					int16_t v = (int16_t)(((uint16_t)pData[6] * 111) >> 8) - 10;
					if (v < 0)
						v = 0;

					channels[OIL].set(v);
#ifdef TXRX
					DBG(F("Oil pressure: "));
					DBG(channels[OIL].value / 10.0, 1);
					DBGL(F(" bar"));
#endif
					break;
				}
				case 0x4B: {
					int16_t v = 1029 + 3 * pData[6] + (((uint16_t)pData[6] * 117 + 128) >> 8);
					v = updateO2(o2[O2_1], v);
					channels[O2].set(v);
#ifdef TXRX
					DBG(F("O2: "));
					DBG(channels[O2].value / 100.0, 2);
					DBGL(F(" AFR"));
#endif
					break;
				}
				case 0x4E: {
					int16_t v = 1029 + 3 * pData[6] + (((uint16_t)pData[6] * 117 + 128) >> 8);
					v = updateO2(o2[O2_2], v);
					channels[O22].set(v);
#ifdef TXRX
					DBG(F("O2 2: "));
					DBG(channels[O22].value / 100.0, 2);
					DBGL(F(" AFR"));
#endif
					break;
				}
				}
			}
			break;
		}

		case 0x0B: {
			channels[MAP].set(pData[5]);
#ifdef TXRX
			DBG(F("MAP: "));
			DBG(channels[MAP].value / 100.0, 1);
			DBGL(F(" bar"));
#endif
			break;
		}
		case 0x19: {
			if (pData[5] == 0x40 && dataSize >= 7) {
				channels[TRANS].set(pData[6] - 40);
#ifdef TXRX
				DBG(F("Trans Temp: "));
				DBG(channels[TRANS].value);
				DBGL(F(" *C"));
#endif
			}
			break;
		}
		default:
			break;
		}
	}
}

void decodeDpidPacket(uint8_t packet, const uint8_t *data) {
	// Reuse the existing PID conversions and independent four-sample AFR filters.
	for (uint8_t i = 0; i < DPID_FIELD_COUNT; ++i) {
		DpidField f = dpidField(i);
		if (f.packet != packet)
			continue;
		uint8_t reply[9] = {0x6c, 0xf1, 0x10, 0, 0, 0, 0, 0, 0};
		if (f.pid > 0xff) {
			reply[3] = 0x62;
			reply[4] = (uint8_t)(f.pid >> 8);
			reply[5] = (uint8_t)f.pid;
			memcpy(reply + 6, data + f.offset, f.length);
			decodeGaugeReply(reply, (uint8_t)(7 + f.length));
		} else {
			reply[3] = 0x41;
			reply[4] = (uint8_t)f.pid;
			memcpy(reply + 5, data + f.offset, f.length);
			decodeGaugeReply(reply, (uint8_t)(6 + f.length));
		}
	}
}

void read() {
	static uint8_t buff[BS];
	uint8_t size;
	while ((size = vpw.tryGetReceivedFrame(buff)) != 0) {
		// All frames here passed CRC. Protect TX even for control/other bus traffic.
		lastRxEnd = (uint16_t)millis();
#if VPW_DPID_ENABLED
		int8_t packet = dpid.receive(buff, size, millis());
		if (packet >= 0) {
			decodeDpidPacket((uint8_t)packet, buff + 5);
			counter += dpidChannelCount((uint8_t)packet);
			continue;
		}
#endif
		if (validGaugeReply(buff, size)) {
#if VPW_DPID_ENABLED
			dpid.ecuSeen();
#endif
			decodeGaugeReply(buff, size);
			++counter;
		}
	}
}

#ifdef DEBUGVPW
void handleError(J1850_Operations op, J1850_ERRORS err) {
	if (err == J1850_OK)
		return;

	// registerError();
	const char *prefix = (op == J1850_Read) ? "READ " : "WRITE ";
	char buf[40];

	switch (err) {
	case J1850_ERR_BUS_IS_BUSY:
		sprintf(buf, "%sJ1850_ERR_BUS_IS_BUSY", prefix);
		break;
	case J1850_ERR_BUS_ERROR:
		sprintf(buf, "%sJ1850_ERR_BUS_ERROR", prefix);
		break;
	case J1850_ERR_PULSE_TOO_SHORT:
		sprintf(buf, "%sJ1850_ERR_PULSE_TOO_SHORT", prefix);
		break;
	case J1850_ERR_ARBITRATION_LOST:
		sprintf(buf, "%sJ1850_ERR_ARBITRATION_LOST", prefix);
		break;
	case J1850_ERR_PULSE_TOO_LONG:
		sprintf(buf, "%sJ1850_ERR_PULSE_TOO_LONG", prefix);
		break;
	case J1850_ERR_IFR_RX_NOT_SUPPORTED:
		sprintf(buf, "%sJ1850_ERR_IFR_RX_NOT_SUPPORTED", prefix);
		break;
	case J1850_ERR_CRC:
		sprintf(buf, "%sJ1850_ERR_CRC", prefix);
		break;
	default:
		sprintf(buf, "%sERR: %02X", prefix, err);
		break;
	}
	DBGL(buf);
}
#endif

int16_t randomDelta(int16_t lastValue, int16_t minValue, int16_t maxValue, float maxPercentChange) {
	if (lastValue == -32768) {
		return (int16_t)random(minValue, maxValue + 1);
	}
	int16_t maxDelta = (int16_t)((maxValue - minValue) * maxPercentChange);
	int16_t newMin = max(minValue, lastValue - maxDelta);
	int16_t newMax = min(maxValue, lastValue + maxDelta);
	return (int16_t)random(newMin, newMax + 1);
}
