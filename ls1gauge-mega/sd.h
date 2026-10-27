
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


// Funkcja zapisująca cały bufor do SD w formacie CSV
void dumpLogToSD() {
  char filename[13];
  DateTime now = rtc.now();
  snprintf(filename, sizeof(filename), "l-%02d%02d%02d.csv", now.year() % 100, now.month(), now.day());
  bool newFile = !SD.exists(filename);
  File logFile = SD.open(filename, FILE_WRITE);
  if (logFile) {
    if (newFile) {
      logFile.println(F("Datetime,Air Temp,Manifold Pressure (bar),TPS (%),Battery (V),AFR Bank 2,Speed (km/h),Transmission Temp,Oil Pressure(bar),Engine RPM,Coolant Temp,AFR Bank 1,Vent Temp, Vent  Humidity(%), Outside Temp, Inside Temp, Latitude, Longitude"));
    }

    for (uint8_t i = 0; i < logIndex; i++) {
char line[20];
snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d",
         now.year(), now.month(), logBuffer[i].day,
        logBuffer[i].hour, logBuffer[i].minute, logBuffer[i].second);
logFile.print(line);
     
      

for (uint8_t j = 0; j < CMD_SIZE; j++) {
  logFile.print(",");

  if (j == 3 || j == 11 || j == 12 || j == 13 || j == 14) {
    float val = logBuffer[i].data[j] / 10.0;
    logFile.print(val, 1);  // 1 miejsce po przecinku
  } else if (j == 1 || j == 7 || j == 4 || j == 10) {
    float val = logBuffer[i].data[j] / 100.0;
    logFile.print(val, 1);
  } else if (j == 15 || j == 16) {
    float val = decodeCoord(logBuffer[i].data[j], logBuffer[i].data[j+2]);
    logFile.print(val, 1);   
  } else {
    logFile.print(logBuffer[i].data[j]);  // bez dzielenia
  }
}
logFile.println();

    }

    logFile.close();
    DBGL(F("SD: Logi zapisane."));
    //sendLogEntry(now);
  } else {
    DBGL(F("SD: Blad"));
  }
}
