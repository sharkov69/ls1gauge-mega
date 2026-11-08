#pragma once
#include "modem_debug_config.h"
#if DEBUG_MODEM
#include "modem_debug_control.h"
// Warm-reset breadcrumbs survive C startup; a power loss can erase them.
struct DebugBreadcrumb {
	uint16_t magic;
	uint8_t stage, watchdog, owner, phase;
};
#ifdef __AVR__
volatile DebugBreadcrumb debugBreadcrumb __attribute__((section(".noinit")));
#else
volatile DebugBreadcrumb debugBreadcrumb;
#endif
DebugBreadcrumb debugPrevious;
uint8_t debugResetFlags = 0;
bool debugBootPending = false;

void debugBootBegin(uint8_t flags) {
	debugResetFlags = flags;
	const bool valid = debugBreadcrumb.magic == 0x59d1 && !(flags & 1);
	debugPrevious.magic = valid ? 0x59d1 : 0;
	debugPrevious.stage = valid ? debugBreadcrumb.stage : 0;
	debugPrevious.watchdog = valid ? debugBreadcrumb.watchdog : 0;
	debugPrevious.owner = valid ? debugBreadcrumb.owner : 0;
	debugPrevious.phase = valid ? debugBreadcrumb.phase : 0;
	debugBreadcrumb.magic = 0x59d1;
	debugBreadcrumb.stage = 1;
	debugBreadcrumb.watchdog = 0;
	debugBreadcrumb.owner = debugBreadcrumb.phase = 0;
	debugBootPending = true;
}
void debugStage(uint8_t stage) {
	if (!debugBreadcrumb.watchdog)
		debugBreadcrumb.stage = stage;
}
void debugWatchdogExpired() {
	debugBreadcrumb.owner = (uint8_t)modemOwner;
	debugBreadcrumb.phase = at.phase;
	debugBreadcrumb.watchdog = 1;
}
void debugBootReport() {
	if (!debugBootPending || !modemDebugEnabled || Serial.availableForWrite() < 60)
		return;
	char report[60];
	const int n = snprintf_P(report, sizeof(report),
	    PSTR("BOOT raw=%u valid=%u wdt=%u stage=%u owner=%u phase=%u\n"),
	    debugResetFlags, debugPrevious.magic == 0x59d1, debugPrevious.watchdog,
	    debugPrevious.stage, debugPrevious.owner, debugPrevious.phase);
	if (n > 0 && n < (int)sizeof(report)) {
		Serial.write((const uint8_t *)report, n);
		debugBootPending = false;
	}
}
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
	debugBootReport();
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
#define debugBootBegin(...) ((void)0)
#define debugBootReport() ((void)0)
#define debugStage(...) ((void)0)
#define debugWatchdogExpired() ((void)0)
#define modemDebugRecord(...) ((void)0)
#define modemDebugNumber(...) ((void)0)
#define modemDebugRx(...) ((void)0)
#define modemDebugTick() ((void)0)
#define modemDebugLoad() ((void)0)
#define modemDebugCommand(...) false
#endif
