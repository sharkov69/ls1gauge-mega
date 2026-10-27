
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
#define SREDNIA 2
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


// Write one record per loop; the acquisition buffer remains the only full log buffer.
File sdLogFile;
uint8_t sdNext = 0, sdCount = 0;
uint16_t sdGeneration = 0, sdYear = 0, sdErrors = 0;
uint8_t sdMonth = 0;
bool sdPending = false;
void dumpLogToSD() {
  if (sdPending) { sdLogFile.close(); ++sdErrors; }
  DateTime timestamp = rtc.now();
  sdYear = timestamp.year(); sdMonth = timestamp.month();
  char filename[13];
  snprintf(filename, sizeof(filename), "l-%02d%02d%02d.csv", sdYear % 100, sdMonth, timestamp.day());
  bool newFile = !SD.exists(filename);
  sdLogFile = SD.open(filename, FILE_WRITE);
  sdPending = (bool)sdLogFile;
  if (!sdPending) { ++sdErrors; return; }
  sdGeneration = logGeneration; sdNext = 0; sdCount = logIndex;
  if (newFile) sdLogFile.println(F("Datetime,Air Temp,Manifold Pressure (bar),TPS (%),Battery (V),AFR Bank 2,Speed (km/h),Transmission Temp,Oil Pressure(bar),Engine RPM,Coolant Temp,AFR Bank 1,Vent Temp, Vent  Humidity(%), Outside Temp, Inside Temp, Latitude, Longitude"));
}
void updateLogToSD() {
  if (!sdPending) return;
  // A long SD stall must not silently join records from two generations.
  if (logGeneration != sdGeneration || logIndex > sdNext) {
    ++sdErrors; sdLogFile.close(); sdPending = false; return;
  }
  char line[20];
  const LogEntry& record = logBuffer[sdNext];
  snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d", sdYear, sdMonth,
           record.day, record.hour, record.minute, record.second);
  sdLogFile.print(line);
  for (uint8_t j=0;j<CMD_SIZE;++j) {
    sdLogFile.print(',');
    if (j == 3 || (j >= 11 && j <= 14)) sdLogFile.print(record.data[j] / 10.0, 1);
    else if (j == 1 || j == 7 || j == 4 || j == 10) sdLogFile.print(record.data[j] / 100.0, 1);
    else if (j == 15 || j == 16) sdLogFile.print(decodeCoord(record.data[j], record.data[j+2]), 1);
    else sdLogFile.print(record.data[j]);
  }
  sdLogFile.println();
  if (sdLogFile.getWriteError()) { ++sdErrors; sdLogFile.close(); sdPending = false; return; }
  if (++sdNext >= sdCount) { sdLogFile.close(); sdPending = false; }
}