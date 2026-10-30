#pragma once
// Bounded console input: a disconnected/noisy terminal cannot grow the heap.
uint16_t minFreeRam = 0xFFFF;
uint32_t maxLoopUs = 0;
int freeRam() {
  extern int __heap_start;
  extern char* __brkval;
  char stack;
  return &stack - (__brkval ? __brkval : (char*)&__heap_start);
}
void sampleFreeRam() {
  int available = freeRam();
  if (available >= 0 && (uint16_t)available < minFreeRam) minFreeRam = (uint16_t)available;
}
void handleConsole() {
  static char line[28];
  static uint8_t length = 0;
  static bool overflow = false;
  uint8_t budget = 32;
  while (budget-- && Serial.available()) {
    char c = (char)Serial.read();
    if (c != '\r' && c != '\n') {
      if (length < sizeof(line) - 1) line[length++] = c;
      else overflow = true;
      continue;
    }
    line[length] = 0;
    if (!overflow && length == 3 && strcmp_P(line, PSTR("VPW")) == 0) {
      char report[64];
      int n = snprintf_P(report, sizeof(report), PSTR("dpid=%u fe=%u fd=%u fail=%u err=%u\n"),
                         (unsigned)(VPW_DPID_ENABLED ? dpid.state : VpwDpid::LEGACY),
                         dpid.packets[0], dpid.packets[1], dpid.failures, dpid.lastError);
      if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
        Serial.write((const uint8_t*)report, n);
    }
    if (!overflow && length == 3 && strcmp_P(line, PSTR("MEM")) == 0) {
      char report[64];
      int n = snprintf_P(report, sizeof(report), PSTR("ram=%d min=%u loop=%lu crc=%u drop=%u\n"),
                         freeRam(), minFreeRam, maxLoopUs, vpw.crcErrors, vpw.droppedFrames());
      if (n > 0 && n < (int)sizeof(report) && Serial.availableForWrite() >= n)
        Serial.write((const uint8_t*)report, n);
    }
    if (!overflow && length == 27 && strncmp_P(line, PSTR("SETTIME="), 8) == 0) {
      bool valid = true;
      for (uint8_t i=8;i<27;++i) {
        char sep = (i==12 || i==15) ? '-' : i==18 ? ' ' : (i==21 || i==24) ? ':' : 0;
        if (sep ? line[i]!=sep : (line[i]<'0' || line[i]>'9')) valid=false;
      }
      if (valid) {
        int y=atoi(line+8), m=atoi(line+13), d=atoi(line+16);
        int h=atoi(line+19), mi=atoi(line+22), s=atoi(line+25);
        if (y>=2000 && y<=2099 && m>=1 && m<=12 && d>=1 && d<=31 && h<24 && mi<60 && s<60) {
          DateTime value(y,m,d,h,mi,s);
          if (value.isValid()) {
            EEPROM.update(EEPROM_DST_FLAG_ADDR, czyLetni(d,m,value.dayOfTheWeek()) ? 1 : 0);
            rtc.adjust(value);
          }
        }
      }
    }
    length=0; overflow=false;
  }
}
