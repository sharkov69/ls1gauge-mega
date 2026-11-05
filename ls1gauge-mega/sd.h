
#include <SdFat.h>
#include "sd_clock.h"
#include "extra_channels.h"
SdFat SD;
void sdDateTime(uint16_t* date, uint16_t* time) {
  const DateTime stamp = rtc.now();
  *date = FS_DATE(stamp.year(), stamp.month(), stamp.day());
  *time = FS_TIME(stamp.hour(), stamp.minute(), stamp.second());
}
#define sdCS 53
#define LOG_ENTRIES 30
struct LogEntry {
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  int16_t data[CMD_SIZE+2];
};
static_assert(CMD_SIZE==17 && sizeof(LogEntry)==42,"Keep the existing LTE API binary layout");
LogEntry logBuffer[LOG_ENTRIES];
//LogEntry logHttpBuffer[LOG_ENTRIES];
uint8_t logIndex = 0;
uint16_t logGeneration = 0;
#define SREDNIA 1 // One RTC 1 Hz snapshot: 30 records per ~30-second LTE upload.
int32_t sums[CMD_SIZE+2] = {0};
uint8_t count = 0;

void encodeCoord(float deg, volatile int16_t &low, int16_t &high) {
  int32_t scaled = (int32_t)(deg * 1e6);  // np. 52.158512 → 52158512
  low  = scaled & 0xFFFF;
  high = scaled >> 16;
}

float decodeCoord(int16_t low, int16_t high) {
  int32_t scaled = ((int32_t)high << 16) | ((uint16_t)low);
  return scaled / 1e6;
}


void addLogEntry() {
  DateTime now = rtc.now();

  if (logIndex >= LOG_ENTRIES) return; // zabezpieczenie
  logBuffer[logIndex].day = now.day();
  logBuffer[logIndex].hour = now.hour();
  logBuffer[logIndex].minute = now.minute();
  logBuffer[logIndex].second = now.second();

  for (uint8_t i = 0; i < CMD_SIZE+2; i++) {
    logBuffer[logIndex].data[i] = sums[i] / SREDNIA;
    sums[i] = 0; // reset sumy po średniej
  }

  logIndex++;
}


// Independent SD snapshots. The 30-record buffer above belongs only to LTE.
// No queue/catch-up: slow storage must not cause a burst of duplicate snapshots.
File sdLogFile;
SdSampleClock sdClock;
const char sdCsvHeader[] PROGMEM = "Datetime;Intake Air Temp (SAE);Intake Manifold Absolute Pressure (SAE);Throttle Position (SAE);Control Module Voltage;AFR 2;Vehicle Speed (SAE);Trans Fluid Temp;Engine Oil Pressure;Engine RPM (SAE);Engine Coolant Temp (SAE);AFR 1;Vent Temp;Vent Humidity(%);Outside Temp;Inside Temp;Latitude;Longitude"
 ";Timing Advance (SAE);PRNDL Status;TCC Applied;Trans Shift Mode;Injector Pulse Width Avg. Bank 1;Injector Pulse Width Avg. Bank 2;AC Requested;Fan 1;Knock Retard;Closed Loop Active"
 ";Fan 2;AC Active;AC Clutch Engaged;AC Disable (Pressure);AC Disable (WOT);Controller Reset;Generator L Term;TCS Request;VATS Status;Traction Control System;Reverse Inhibited;Fuel Trim Learn;O2 B1S1 Ready;O2 B1S2 Ready;O2 B2S1 Ready;O2 B2S2 Ready;Cold Start";
struct CsvExtraField { uint8_t source, mask, invert; };
const CsvExtraField csvExtraFields[] PROGMEM = {
 {X_SA,0,0},{X_PRNDL,0,0},{X_TCC,128,0},{X_SHIFT,0,0},{X_INJ1,0,0},{X_INJ2,0,0},
 {X_AC,1,0},{X_FAN,1,0},{X_KR,0,0},{X_CL,1,0},
 {X_FAN,2,0},{X_AC,2,0},{X_AC,8,0},{X_AC,16,0},{X_AC,64,0},{X_AC,4,0},{X_AC,128,0},
 {X_FAN,128,0},{X_TCC,2,0},{X_TCC,1,1},{X_TCC,16,1},{X_CL,32,0},
 {X_CL,2,0},{X_CL,4,0},{X_CL,8,0},{X_CL,16,0},{X_CL,64,0}
};
// Exact decimal formatting without a floating-point or line buffer.
void sdPrintFixed(int32_t value, uint32_t scale) {
  const uint32_t magnitude = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
  if (value < 0) sdLogFile.print('-');
  sdLogFile.print(magnitude / scale); sdLogFile.print(',');
  for (uint32_t place = scale / 10; place; place /= 10)
    sdLogFile.print((char)('0' + (magnitude / place) % 10));
}
// 1: matching header, 0: different format, -1: I/O error (never rename).
int8_t sdCompatibleFile(const char* filename) {
  File existing = SD.open(filename, O_RDONLY);
  if (!existing) return -1;
  bool same = true;
  for (uint16_t i = 0; ; ++i) {
    const char expected = pgm_read_byte(sdCsvHeader + i);
    const int actual = existing.read();
    if (!expected) {
      same = actual == '\n' || (actual == '\r' && existing.read() == '\n');
      break;
    }
    if (actual != expected) { same = false; break; }
  }
  const bool ioError = existing.getError() != 0;
  existing.close(); return ioError ? -1 : (same ? 1 : 0);
}
const uint16_t SD_SAMPLE_MS = 200, SD_SYNC_MS = 1000, SD_RETRY_MS = 5000;
uint32_t sdLastSample = 0, sdLastSync = 0, sdFailureSince = 0, sdRows = 0;
uint16_t sdYear = 0, sdErrors = 0;
uint8_t sdMonth = 0, sdDay = 0;
bool sdBackoff = false;
void sdWriteFailed() {
  ++sdErrors; sdLogFile.close(); sdBackoff = true; sdFailureSince = millis();
}
void updateLogToSD() {
  const uint32_t sampleMs = millis();
  if (sdBackoff && (uint32_t)(sampleMs - sdFailureSince) < SD_RETRY_MS) return;
  if ((uint32_t)(sampleMs - sdLastSample) < SD_SAMPLE_MS) return;
  sdLastSample = sampleMs; sdBackoff = false;
  sdClock.update(rtc.now().unixtime(), sampleMs);
  DateTime timestamp(sdClock.seconds);
  // Copy values before disk work; the only per-sample buffer is 42 bytes on stack.
  LogEntry record = {};
  record.day = timestamp.day(); record.hour = timestamp.hour();
  record.minute = timestamp.minute(); record.second = timestamp.second();
  for (uint8_t i = 0; i < CMD_SIZE; ++i) {
    record.data[i] = cmd[i].wartosc == -32768 ? 0 : cmd[i].wartosc;
    if (i == 15 || i == 16) record.data[i+2] = cmd[i].lastValue == -32768 ? 0 : cmd[i].lastValue;
  }
  if (sdLogFile && (sdYear != timestamp.year() || sdMonth != timestamp.month() || sdDay != timestamp.day())) {
    if (!sdLogFile.sync()) { sdWriteFailed(); return; }
    sdLogFile.close();
  }
  if (!sdLogFile) {
    sdYear = timestamp.year(); sdMonth = timestamp.month(); sdDay = timestamp.day();
    char filename[13];
    snprintf(filename, sizeof(filename), "l-%02u%02u%02u.csv", sdYear % 100, sdMonth, sdDay);
    bool newFile = !SD.exists(filename);
    const int8_t compatible = newFile ? 1 : sdCompatibleFile(filename);
    if (compatible < 0) { sdWriteFailed(); return; }
    if (!compatible) {
      char backup[13]; memcpy(backup, filename, sizeof(backup));
      memcpy(backup + 9, "bak", 4);
      uint8_t revision=0;
      while(SD.exists(backup)&&revision<99){
        ++revision;snprintf(backup+9,4,"b%02u",revision);
      }
      if (SD.exists(backup) || !SD.rename(filename, backup)) { sdWriteFailed(); return; }
      newFile = true;
    }
    sdLogFile = SD.open(filename, FILE_WRITE);
    if (!sdLogFile) { sdWriteFailed(); return; }
    sdLastSync = sampleMs;
    if (newFile) {
      for (uint16_t i = 0; pgm_read_byte(sdCsvHeader + i); ++i)
        sdLogFile.print((char)pgm_read_byte(sdCsvHeader + i));
      sdLogFile.println();
    }
  }
  char line[24];
  snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u,%03u", sdYear, sdMonth,
           record.day, record.hour, record.minute, record.second, sdClock.fraction);
  sdLogFile.print(line);
  for (uint8_t j=0;j<CMD_SIZE;++j) {
    sdLogFile.print(';');
    if (j == 3 || (j >= 11 && j <= 14)) sdPrintFixed(record.data[j], 10);
    else if (j == 7) sdPrintFixed(record.data[j], 10); // Oil is stored in tenths of bar.
    else if (j == 1 || j == 4 || j == 10) sdPrintFixed(record.data[j], 100);
    else if (j == 15 || j == 16) sdPrintFixed((int32_t)(((uint32_t)(uint16_t)record.data[j+2] << 16) | (uint16_t)record.data[j]), 1000000);
    else sdLogFile.print(record.data[j]);
  }
  // Extra fields are SD-only. No enlargement of the 30-record LTE buffer.
  for(uint8_t i=0;i<sizeof(csvExtraFields)/sizeof(csvExtraFields[0]);++i){
    CsvExtraField f;memcpy_P(&f,csvExtraFields+i,sizeof(f));sdLogFile.print(';');
    if(!extra.fresh(f.source,sampleMs,3000))continue; // Missing/stale is blank, never a false zero.
    const uint16_t raw=extra.raw[f.source];
    if(f.mask)sdLogFile.print((int)(((raw&f.mask)!=0)^f.invert));
    else if(f.source==X_SA)sdPrintFixed(((int32_t)raw-128)*5,10);
    else if(f.source==X_KR)sdPrintFixed(((uint32_t)raw*800+45)/91,100);
    else if(f.source==X_INJ1||f.source==X_INJ2){
      const uint32_t numerator=(uint32_t)raw*1000;
      sdPrintFixed((numerator/65535)*1000+((numerator%65535)*1000+32767)/65535,1000);
    }else sdLogFile.print(raw);
  }
  sdLogFile.println();
  if (sdLogFile.getWriteError()) { sdWriteFailed(); return; }
  if ((uint32_t)(sampleMs - sdLastSync) >= SD_SYNC_MS) {
    if (!sdLogFile.sync()) { sdWriteFailed(); return; }
    sdLastSync = sampleMs;
  }
  ++sdRows;
}
