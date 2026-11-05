#pragma once
#include "modem_debug_config.h"
#if DEBUG_MODEM
#include "modem_debug_control.h"
int freeRam();
uint16_t modemDebugDropped = 0, modemDebugTruncated = 0;
uint32_t modemDebugLastReport = 0;
char modemDebugFragment[25];
bool modemDebugMuted = false;
void modemDebugCount(uint16_t &value) {
	if (value != 65535)
		++value;
}
// Whole record fits the Mega's 63 usable TX slots. No flush, queue, heap or wait.
void modemDebugRecord(char kind, char subkind, const char *detail) {
	if (!modemDebugEnabled)
		return;
	if (Serial.availableForWrite() < 32) {
		modemDebugCount(modemDebugDropped);
		return;
	}
	char record[64];
	int length =
	    snprintf_P(record, sizeof(record), PSTR("%lu M%u S%u R%u %c%c "), (unsigned long)millis(),
	               (unsigned)modemOwner, (unsigned)stepIdx, (unsigned)tries, kind, subkind);
	if (length < 0 || length > 36) {
		modemDebugCount(modemDebugDropped);
		return;
	}
	uint8_t i = 0;
	while (detail[i] && i < 24) {
		char c = detail[i++];
		record[length++] = c >= 32 && c <= 126 ? c : '.';
	}
	if (detail[i]) {
		record[length++] = '~';
		modemDebugCount(modemDebugTruncated);
	}
	record[length++] = '\n';
	if (Serial.availableForWrite() < length) {
		modemDebugCount(modemDebugDropped);
		return;
	}
	Serial.write((const uint8_t *)record, (size_t)length);
}
void modemDebugNumber(char kind, char subkind, uint32_t value) {
	if (!modemDebugEnabled)
		return;
	char text[11];
	snprintf_P(text, sizeof(text), PSTR("%lu"), (unsigned long)value);
	modemDebugRecord(kind, subkind, text);
}
void modemDebugRx(char c, bool discarded) {
	if (!modemDebugEnabled || modemDebugMuted) {
		modemDebugLength = 0;
		return;
	}
	if (c == '\r')
		return;
	if (c != '\n')
		modemDebugFragment[modemDebugLength++] = c >= 32 && c <= 126 ? c : '.';
	if (c == '\n' || modemDebugLength == sizeof(modemDebugFragment) - 1) {
		if (modemDebugLength) {
			modemDebugFragment[modemDebugLength] = 0;
			modemDebugRecord(discarded ? 'D' : 'R', c == '\n' ? 'X' : '+', modemDebugFragment);
			modemDebugLength = 0;
		}
	}
}
void modemDebugTick() {
	modemDebugControlTick(); // Confirm persistence before spending USB slots on log text.
	if (!modemDebugEnabled)
		return;
	const uint32_t now = millis();
	if ((uint32_t)(now - modemDebugLastReport) < 5000)
		return;
	modemDebugLastReport = now;
	char detail[24];
	snprintf_P(detail, sizeof(detail), PSTR("%d/%u/%u"), freeRam(), modemDebugDropped,
	           modemDebugTruncated);
	modemDebugRecord('H', 'B', detail); // RAM / omitted records / shortened records.
}
#else
#define modemDebugRecord(...) ((void)0)
#define modemDebugNumber(...) ((void)0)
#define modemDebugRx(...) ((void)0)
#define modemDebugTick() ((void)0)
#define modemDebugLoad() ((void)0)
#define modemDebugCommand(...) false
#endif
