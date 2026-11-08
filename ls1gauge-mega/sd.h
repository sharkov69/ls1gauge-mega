
#include <SdFat.h>
#include "sd_clock.h"
SdFat SD;
void sdDateTime(uint16_t *date, uint16_t *time) {
	const DateTime stamp = rtc.now();
	*date = FS_DATE(stamp.year(), stamp.month(), stamp.day());
	*time = FS_TIME(stamp.hour(), stamp.minute(), stamp.second());
}
#define sdCS 53
struct SdSnapshot {
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	int16_t data[19]; // Base channels and two high words of GPS coordinates.
};

void encodeCoord(float deg, int16_t &low, int16_t &high) {
	int32_t scaled = (int32_t)(deg * 1e6); // np. 52.158512 → 52158512
	low = scaled & 0xFFFF;
	high = scaled >> 16;
}

float decodeCoord(int16_t low, int16_t high) {
	int32_t scaled = ((int32_t)high << 16) | ((uint16_t)low);
	return scaled / 1e6;
}


// One snapshot per SD row.
// No queue/catch-up: slow storage must not cause a burst of duplicate snapshots.
File sdLogFile;
char sdUploadName[13] = {};
uint32_t sdUploadSyncedSize = 0;
bool sdUploadFailed = false;
bool sdUploadFaultLatched = false;
bool csvQueueNewFile(const char *name);
bool csvQueueRename(const char *oldName, const char *newName);
bool sdSyncLog() {
	const bool ok = sdLogFile.sync();
	if (ok) sdUploadSyncedSize = sdLogFile.size();
	sdUploadFailed = !ok;
	return ok;
}
SdSampleClock sdClock;
const char sdCsvHeader[] PROGMEM =
	"Datetime;Control Module Voltage;Engine RPM (SAE);Vehicle Speed (SAE);"
	"Throttle Position (SAE);Intake Air Temp (SAE);Engine Coolant Temp (SAE);Trans Fluid Temp;"
	"Intake Manifold Absolute Pressure (SAE);Engine Oil Pressure;Timing Advance (SAE);AFR 1;"
	"AFR 2;Vent Temp;Vent Humidity(%);Outside Temp;Inside Temp;Latitude;Longitude;PRNDL Status;"
	"TCC Applied;Trans Shift Mode;Injector Pulse Width Avg. Bank 1;"
	"Injector Pulse Width Avg. Bank 2;AC Requested;AC Active;AC Clutch Engaged;"
	"AC Disable (Pressure);AC Disable (WOT);Fan 1;Fan 2;Knock Retard;Closed Loop Active;"
	"Reverse Inhibited;Fuel Trim Learn;O2 B1S1 Ready;O2 B2S1 Ready";
struct CsvField {
	uint8_t source, mask, invert;
};
// CSV order is independent of the channel indices.
const CsvField csvFields[] PROGMEM = {
	{BATT, 0, 0},
	{RPM, 0, 0},
	{SPEED, 0, 0},
	{TPS, 0, 0},
	{AIR, 0, 0},
	{WATER, 0, 0},
	{TRANS, 0, 0},
	{MAP, 0, 0},
	{OIL, 0, 0},
	{SA, 0, 0},
	{O2, 0, 0},
	{O22, 0, 0},
	{TEMP_AC, 0, 0},
	{HUM, 0, 0},
	{TEMP_OUT, 0, 0},
	{TEMP_IN, 0, 0},
	{LAT, 0, 0},
	{LON, 0, 0},
	{PRNDL, 0, 0},
	{TCC, 128, 0},
	{SHIFT_MODE, 0, 0},
	{INJ1, 0, 0},
	{INJ2, 0, 0},
	{AC_STATE, 1, 0},
	{AC_STATE, 2, 0},
	{AC_STATE, 8, 0},
	{AC_STATE, 16, 0},
	{AC_STATE, 64, 0},
	{FAN_STATE, 1, 0},
	{FAN_STATE, 2, 0},
	{KR, 0, 0},
	{CL_STATE, 1, 0},
	{TCC, 16, 1},
	{CL_STATE, 32, 0},
	{CL_STATE, 2, 0},
	{CL_STATE, 8, 0},
};
// Exact decimal formatting without a floating-point or line buffer.
void sdPrintFixed(int32_t value, uint32_t scale) {
	const uint32_t magnitude = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
	if (value < 0)
		sdLogFile.print('-');
	sdLogFile.print(magnitude / scale);
	sdLogFile.print(',');
	for (uint32_t place = scale / 10; place; place /= 10)
		sdLogFile.print((char)('0' + (magnitude / place) % 10));
}
// 1: matching header, 0: different format, -1: I/O error (never rename).
int8_t sdCompatibleFile(const char *filename) {
	File existing = SD.open(filename, O_RDONLY);
	if (!existing)
		return -1;
	bool same = true;
	for (uint16_t i = 0;; ++i) {
		const char expected = pgm_read_byte(sdCsvHeader + i);
		const int actual = existing.read();
		if (!expected) {
			same = actual == '\n' || (actual == '\r' && existing.read() == '\n');
			break;
		}
		if (actual != expected) {
			same = false;
			break;
		}
	}
	const bool ioError = existing.getError() != 0;
	existing.close();
	return ioError ? -1 : (same ? 1 : 0);
}
const uint16_t SD_SAMPLE_MS = 200, SD_SYNC_MS = 1000, SD_RETRY_MS = 5000;
uint32_t sdLastSample = 0, sdLastSync = 0, sdFailureSince = 0, sdRows = 0;
uint16_t sdYear = 0, sdErrors = 0;
uint8_t sdMonth = 0, sdDay = 0;
bool sdBackoff = false;
void sdWriteFailed() {
	++sdErrors;
	sdUploadFailed = true;
	sdUploadFaultLatched = true;
	sdLogFile.close();
	sdBackoff = true;
	sdFailureSince = millis();
}
void updateLogToSD() {
	const uint32_t sampleMs = millis();
	if (!engineLogGate.allowed(sampleMs)) {
		if (sdLogFile) {
			if (!sdSyncLog())
				sdWriteFailed();
			else
				sdLogFile.close();
		}
		// The first received running RPM may write immediately, without catch-up.
		sdLastSample = sampleMs - SD_SAMPLE_MS;
		return;
	}
	if (sdBackoff && (uint32_t)(sampleMs - sdFailureSince) < SD_RETRY_MS)
		return;
	if ((uint32_t)(sampleMs - sdLastSample) < SD_SAMPLE_MS)
		return;
	sdLastSample = sampleMs;
	sdBackoff = false;
	sdClock.update(rtc.now().unixtime(), sampleMs);
	DateTime timestamp(sdClock.seconds);
	// Copy values before disk work; the only per-sample buffer is 42 bytes on stack.
	SdSnapshot record = {};
	record.day = timestamp.day();
	record.hour = timestamp.hour();
	record.minute = timestamp.minute();
	record.second = timestamp.second();
	for (uint8_t i = 0; i < BASE_CHANNEL_COUNT; ++i) {
		record.data[i] = channels[i].value;
		if (i == 15 || i == 16)
			record.data[i + 2] = gpsHigh[i - LAT];
	}
	if (sdLogFile &&
	    (sdYear != timestamp.year() || sdMonth != timestamp.month() || sdDay != timestamp.day())) {
		if (!sdSyncLog()) {
			sdWriteFailed();
			return;
		}
		sdLogFile.close();
	}
	if (!sdLogFile) {
		sdYear = timestamp.year();
		sdMonth = timestamp.month();
		sdDay = timestamp.day();
		char filename[13];
		snprintf(filename, sizeof(filename), "l-%02u%02u%02u.csv", sdYear % 100, sdMonth, sdDay);
		bool newFile = !SD.exists(filename);
		const int8_t compatible = newFile ? 1 : sdCompatibleFile(filename);
		if (compatible < 0) {
			sdWriteFailed();
			return;
		}
		if (!compatible) {
			char backup[13];
			memcpy(backup, filename, sizeof(backup));
			memcpy(backup + 9, "bak", 4);
			uint8_t revision = 0;
			while (SD.exists(backup) && revision < 99) {
				++revision;
				snprintf(backup + 9, 4, "b%02u", revision);
			}
			if (SD.exists(backup) || !SD.rename(filename, backup)
			    || !csvQueueRename(filename, backup)
			    ) {
				sdWriteFailed();
				return;
			}
			newFile = true;
		}
		if (newFile && !csvQueueNewFile(filename)) { sdWriteFailed(); return; }
		const bool sameFile = !strcmp(sdUploadName, filename);
		memcpy(sdUploadName, filename, sizeof(sdUploadName));
		if (newFile || !sameFile) { sdUploadSyncedSize = 0; sdUploadFailed = false; }
		sdLogFile = SD.open(filename, FILE_WRITE);
		if (!sdLogFile) {
			sdWriteFailed();
			return;
		}
		if (!newFile && !sameFile) sdUploadSyncedSize = sdLogFile.size();
		sdLastSync = sampleMs;
		if (newFile) {
			for (uint16_t i = 0; pgm_read_byte(sdCsvHeader + i); ++i)
				sdLogFile.print((char)pgm_read_byte(sdCsvHeader + i));
			sdLogFile.println();
		}
	}
	char line[24];
	snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u,%03u", sdYear, sdMonth, record.day,
	         record.hour, record.minute, record.second, sdClock.fraction);
	sdLogFile.print(line);
	for (uint8_t i = 0; i < sizeof(csvFields) / sizeof(csvFields[0]); ++i) {
		CsvField field;
		memcpy_P(&field, csvFields + i, sizeof(field));
		sdLogFile.print(';');
		if (!channels[field.source].valid)
			continue; // Blank only until the first received value, including a valid zero.
		const uint8_t id = field.source;
		if (id < BASE_CHANNEL_COUNT) {
			if (id == BATT || id == OIL || (id >= TEMP_AC && id <= TEMP_IN))
				sdPrintFixed(record.data[id], 10);
			else if (id == MAP || id == O2 || id == O22)
				sdPrintFixed(record.data[id], 100);
			else if (id == LAT || id == LON)
				sdPrintFixed((int32_t)(((uint32_t)(uint16_t)record.data[id + 2] << 16) |
				                       (uint16_t)record.data[id]), 1000000);
			else
				sdLogFile.print(record.data[id]);
			continue;
		}
		const uint16_t raw = channels[id].raw();
		if (field.mask)
			sdLogFile.print((int)(((raw & field.mask) != 0) ^ field.invert));
		else if (id == SA)
			sdPrintFixed(((int32_t)raw - 128) * 5, 10);
		else if (id == KR)
			sdPrintFixed(((uint32_t)raw * 800 + 45) / 91, 100);
		else if (id == INJ1 || id == INJ2) {
			const uint32_t numerator = (uint32_t)raw * 1000;
			sdPrintFixed((numerator / 65535) * 1000 + ((numerator % 65535) * 1000 + 32767) / 65535,
			             1000);
		} else
			sdLogFile.print(raw);
	}

	sdLogFile.println();
	if (sdLogFile.getWriteError()) {
		sdWriteFailed();
		return;
	}
	if ((uint32_t)(sampleMs - sdLastSync) >= SD_SYNC_MS) {
		if (!sdSyncLog()) {
			sdWriteFailed();
			return;
		}
		sdLastSync = sampleMs;
	}
	++sdRows;
}
