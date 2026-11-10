#pragma once
// Bounded console input: a disconnected/noisy terminal cannot grow the heap.
uint16_t minFreeRam = 0xFFFF;
uint32_t maxLoopUs = 0;
int freeRam() {
	extern int __heap_start;
	extern char *__brkval;
	char stack;
	return &stack - (__brkval ? __brkval : (char *)&__heap_start);
}
void sampleFreeRam() {
	int available = freeRam();
	if (available >= 0 && (uint16_t)available < minFreeRam)
		minFreeRam = (uint16_t)available;
}
uint8_t mapReportIndex = DPID_FIELD_COUNT;
void handleConsole() {
	if (mapReportIndex < DPID_FIELD_COUNT) {
		const DpidField field = dpidField(mapReportIndex);
		const uint8_t slot = passiveDpid.slot[mapReportIndex];
		char report[48];
		const int n = snprintf_P(report, sizeof(report),
		    PSTR("PID=%04X DPID=%02X offset=%u len=%u ready=%u\n"),
		    field.pid, passiveDpid.packet[mapReportIndex],
		    slot ? ((slot >> 3) & 7) - 1 : 0, field.length, (slot & 0x80) ? 1 : 0);
		if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n) {
			Serial.write((const uint8_t *)report, n);
			++mapReportIndex;
		}
	}

	static char line[28];
	static uint8_t length = 0;
	static bool overflow = false;
	uint8_t budget = 32;
	while (budget-- && Serial.available()) {
		char c = (char)Serial.read();
		if (c != '\r' && c != '\n') {
			if (length < sizeof(line) - 1)
				line[length++] = c;
			else
				overflow = true;
			continue;
		}
		line[length] = 0;
#ifdef __AVR__
		if (!overflow && strcmp_P(line, PSTR("UART1")) == 0) {
			char report[56];
			int n = snprintf_P(report, sizeof(report), PSTR("uart1 a=%u b=%u c=%u baud=%u pin=%u ddr=%u\n"),
			    UCSR1A, UCSR1B, UCSR1C, UBRR1, PIND, DDRD);
			if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
				Serial.write((const uint8_t *)report, n);
		}
#endif
		if (!overflow && strcmp_P(line, PSTR("TLS PASS")) == 0) {
			const bool success = csvSaveTlsQualification();
			if (success) csvLoadProvisioning();
			if (Serial.availableForWrite() >= 10) Serial.print(success ? F("TLS OK\n") : F("TLS ERROR\n"));
		}
		if (!overflow && strcmp_P(line, PSTR("CSV")) == 0) {
			char report[64];
			int n = snprintf_P(report, sizeof(report), PSTR("cfg=%u tls=%u state=%u err=%u off=%lu\n"),
			    csvProvisioned, csvTlsQualified, (unsigned)csvHttpState, logUploadErrors, csvUploader.offset());
			if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
				Serial.write((const uint8_t *)report, n);
		}
		if (!overflow && strcmp_P(line, PSTR("QUEUE")) == 0) {
			char report[64];
			int n = snprintf_P(report, sizeof(report), PSTR("q=%u file=%s size=%lu off=%lu\n"),
			    csvQueueReady, csvQueueName, csvQueueLimit(), csvUploader.offset());
			if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
				Serial.write((const uint8_t *)report, n);
		}
		if (!overflow && strcmp_P(line, PSTR("RTC")) == 0) {
			char report[64];
			int n = snprintf_P(report, sizeof(report), PSTR("rtc=%u errors=%u timeout=%u epoch=%lu\n"),
			    rtcTimeValid, rtcReadErrors, Wire.getWireTimeoutFlag(), rtcCachedNow().unixtime());
			if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
				Serial.write((const uint8_t *)report, n);
		}
		if (!overflow && strncmp_P(line, PSTR("CFG "), 4) == 0) {
			bool success = false;
			if (strcmp_P(line, PSTR("CFG COMMIT")) == 0) {
				success = csvConfigCommit();
				if (success) csvLoadProvisioning();
			} else if (length == 23 && line[6] == ' ' && line[4] >= '0' && line[4] <= '3' &&
			           line[5] >= '0' && line[5] <= '9') {
				uint8_t bytes[8] = {};
				bool valid = true;
				for (uint8_t i = 0; i < 16; ++i) {
					char c = line[7 + i];
					int8_t digit = c >= '0' && c <= '9' ? c - '0' :
					               c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
					if (digit < 0) valid = false;
					else bytes[i / 2] |= (uint8_t)digit << (i % 2 ? 0 : 4);
				}
				if (valid) success = csvConfigChunk((line[4] - '0') * 10 + line[5] - '0', bytes);
				memset(bytes, 0, sizeof(bytes));
			}
			memset(line, 0, sizeof(line));
			if (Serial.availableForWrite() >= 10) Serial.print(success ? F("CFG OK\n") : F("CFG ERROR\n"));
		}
		if (!overflow)
			modemDebugCommand(line);
		if (!overflow && length == 2 && strcmp_P(line, PSTR("SD")) == 0) {
			char report[64];
			int n =
			    snprintf_P(report, sizeof(report), PSTR("sd_rows=%lu errors=%u interval_ms=%u\n"),
			               sdRows, sdErrors, SD_SAMPLE_MS);
			if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
				Serial.write((const uint8_t *)report, n);
		}
		if (!overflow && length == 3 && strcmp_P(line, PSTR("MAP")) == 0)
			mapReportIndex = 0;
		if (!overflow && length == 3 && strcmp_P(line, PSTR("VPW")) == 0) {
			char report[64];
			int n = snprintf_P(report, sizeof(report), PSTR("dpid=%u fe=%u fd=%u fail=%u err=%u\n"),
			                   (unsigned)(VPW_DPID_ENABLED ? dpid.state : VpwDpid::LEGACY),
			                   dpid.packets[0], dpid.packets[1], dpid.failures, dpid.lastError);
			if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
				Serial.write((const uint8_t *)report, n);
		}
		if (!overflow && length == 3 && strcmp_P(line, PSTR("MEM")) == 0) {
			char report[64];
			int n =
			    snprintf_P(report, sizeof(report), PSTR("ram=%d min=%u loop=%lu crc=%u drop=%u\n"),
			               freeRam(), minFreeRam, maxLoopUs, vpw.crcErrors, vpw.droppedFrames());
			if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
				Serial.write((const uint8_t *)report, n);
		}
		if (!overflow && length == 27 && strncmp_P(line, PSTR("SETTIME="), 8) == 0) {
			bool valid = true;
			for (uint8_t i = 8; i < 27; ++i) {
				char sep = (i == 12 || i == 15)   ? '-'
				           : i == 18              ? ' '
				           : (i == 21 || i == 24) ? ':'
				                                  : 0;
				if (sep ? line[i] != sep : (line[i] < '0' || line[i] > '9'))
					valid = false;
			}
			if (valid) {
				int y = atoi(line + 8), m = atoi(line + 13), d = atoi(line + 16);
				int h = atoi(line + 19), mi = atoi(line + 22), s = atoi(line + 25);
				if (y >= 2000 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31 && h < 24 &&
				    mi < 60 && s < 60) {
					DateTime value(y, m, d, h, mi, s);
					if (value.isValid()) {
						EEPROM.update(EEPROM_DST_FLAG_ADDR,
						              czyLetni(d, m, value.dayOfTheWeek()) ? 1 : 0);
						setRtcTime(value);
					}
				}
			}
		}
		length = 0;
		overflow = false;
	}
}
