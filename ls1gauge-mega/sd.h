
#include <SdFat.h>
SdFat SD;
#define sdCS 53
#define LOG_ENTRIES 30
struct LogEntry {
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  int16_t data[CMD_SIZE+2];
};
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
  DateTime timestamp = rtc.now();
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
    snprintf(filename, sizeof(filename), "f-%02u%02u%02u.csv", sdYear % 100, sdMonth, sdDay);
    bool newFile = !SD.exists(filename);
    sdLogFile = SD.open(filename, FILE_WRITE);
    if (!sdLogFile) { sdWriteFailed(); return; }
    sdLastSync = sampleMs;
    if (newFile) sdLogFile.println(F("Datetime,Uptime_ms,Air Temp,Manifold Pressure (bar),TPS (%),Battery (V),AFR Bank 2,Speed (km/h),Transmission Temp,Oil Pressure(bar),Engine RPM,Coolant Temp,AFR Bank 1,Vent Temp, Vent  Humidity(%), Outside Temp, Inside Temp, Latitude, Longitude"));
  }
  char line[20];
  snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d", sdYear, sdMonth,
           record.day, record.hour, record.minute, record.second);
  sdLogFile.print(line); sdLogFile.print(','); sdLogFile.print(sampleMs);
  for (uint8_t j=0;j<CMD_SIZE;++j) {
    sdLogFile.print(',');
    if (j == 3 || (j >= 11 && j <= 14)) sdLogFile.print(record.data[j] / 10.0, 1);
    else if (j == 1 || j == 7 || j == 4 || j == 10) sdLogFile.print(record.data[j] / 100.0, 1);
    else if (j == 15 || j == 16) sdLogFile.print(decodeCoord(record.data[j], record.data[j+2]), 1);
    else sdLogFile.print(record.data[j]);
  }
  sdLogFile.println();
  if (sdLogFile.getWriteError()) { sdWriteFailed(); return; }
  if ((uint32_t)(sampleMs - sdLastSync) >= SD_SYNC_MS) {
    if (!sdLogFile.sync()) { sdWriteFailed(); return; }
    sdLastSync = sampleMs;
  }
  ++sdRows;
}
