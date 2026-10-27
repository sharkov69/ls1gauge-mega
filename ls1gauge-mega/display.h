 struct DisplayField {
  uint8_t x, y, w, h;
  int16_t limit;
  uint8_t decimals, textSize, width;
  uint16_t normColor;
};

const DisplayField fields[14] PROGMEM = {
  {155, 140, 35, 18,   -1, 0, 2, 3, GC9A01A_WHITE},  // AIR   = 0
  {175, 100, 35, 18,   -1, 2, 2, 3, GC9A01A_WHITE},  // MAP   = 1
  { 70,  80, 35, 18,   89, 0, 2, 3, GC9A01A_WHITE},  // TPS   = 2
  {175,  80, 47, 18,   -1, 1, 2, 3, GC9A01A_WHITE},  // BATT  = 3
  { 70, 120, 50, 18,   -1, 2, 2, 3, GC9A01A_WHITE},  // O22   = 4
  {145,  30, 35, 20,   -1, 0, 2, 3, GC9A01A_YELLOW}, // SPEED = 5
  {155, 180, 35, 18,   89, 0, 2, 3, GC9A01A_WHITE},  // TRANS = 6
  {175, 120, 35, 18,   -1, 1, 2, 3, GC9A01A_WHITE},  // SPARK = 7 (olej)
  {135,  50, 75, 22, 5499, 0, 3, 4, GC9A01A_GREEN},  // RPM   = 8
  {155, 160, 35, 18,   99, 0, 2, 3, GC9A01A_WHITE},  // WATER = 9
  { 70, 100, 50, 18,   -1, 2, 2, 3, GC9A01A_WHITE},  // O2    = 10

  {138, 225, 25, 18,   -1, 0, 1, 3, GC9A01A_YELLOW}, // 11: RX
  {112, 225, 17, 18,   -1, 0, 1, 3, GC9A01A_YELLOW}, // 12: TX
  {125, 200, 47, 18,   -1, 1, 2, 3, GC9A01A_WHITE}   // 13: bateria w drugim widoku
};

void display(uint8_t id, int16_t value) {
  if (id >= 14) return;
  if (value == -32768) value = 0;

  if (id <= 10) {
    if (value == cmd[id].lastValue) return;
    cmd[id].lastValue = value;
  }

  DisplayField field;
  memcpy_P(&field, &fields[id], sizeof(field));
  
  tft.fillRect(field.x, field.y, field.w, field.h, GC9A01A_BLACK);
  tft.setTextSize(field.textSize);
  if (field.limit != -1 && value > field.limit) {
    tft.setTextColor(GC9A01A_RED);
  } else {
    tft.setTextColor(field.normColor);
  }
  tft.setCursor(field.x, field.y);
  char buf[6];
  switch (field.decimals) {
    case 1:
      sprintf(buf, "%d.%d", value / 10, value % 10);
      break;
    case 2:
      sprintf(buf, "%d.%d", (value + 5) / 100, ((value + 5) % 100) / 10);
      break;
    default:
      switch (field.width) {
        case 4: sprintf(buf, "%4d", value); break;
        case 2: sprintf(buf, "%2d", value); break;
        default: sprintf(buf, "%3d", value); break;
      }
      break;
  }
  tft.print(buf);
}


void drawGUI() {
   
  for (uint8_t i = 0; i <= 10; i++) {
  cmd[i].lastValue = -32768;
} 
  tft.fillScreen(GC9A01A_BLACK);
  tft.setTextSize(2); tft.setTextColor(GC9A01A_WHITE);
  tft.setCursor(55, 30); tft.println(F("Speed:"));
  tft.setTextSize(3);
  tft.setCursor(40, 50); tft.println(F("RPM:"));
  lastKat = KAT;
  obrotomierz(lastKat, lastKat+300, 0x39E7);
  tft.setTextSize(2);
  tft.setCursor(10, 80); tft.println(F("TPS:"));
  tft.setCursor(70, 80); tft.print(F("  0")); tft.println(F("%"));
  tft.setCursor(125, 80); tft.println(F("BAT:"));
  tft.setCursor(175, 80); tft.print(F(" 0.0")); tft.println(F("V"));
  tft.setCursor(10, 100); tft.println(F("AFR1:"));
  tft.setCursor(125, 100); tft.println(F("MAP:"));
  tft.setCursor(175, 100); tft.print(F("0.0")); tft.setTextSize(1); tft.setCursor(215, 107); tft.println(F("bar"));
  tft.setTextSize(2);
  tft.setCursor(10, 120); tft.println(F("AFR2:"));
  tft.setCursor(125, 120); tft.println(F("OIL:"));
  tft.setCursor(175, 120); tft.print(F("0.0")); tft.setTextSize(1); tft.setCursor(215, 127); tft.println(F("bar"));
  tft.setTextSize(2);
  tft.setCursor(25, 140); tft.println(F("Air Temp:"));
  tft.setCursor(155, 140); tft.print(F("  0")); tft.write(0xF7); tft.println(F("C"));
  tft.setCursor(25, 160); tft.println(F("Water Temp:"));
  tft.setCursor(155, 160); tft.print(F("  0")); tft.write(0xF7);  tft.println(F("C"));
  tft.setCursor(25, 180); tft.println(F("Trans Temp:"));
  tft.setCursor(155, 180); tft.print(F("  0")); tft.write(0xF7); tft.println(F("C"));

  tft.setTextSize(1); tft.setTextColor(GC9A01A_YELLOW);
  tft.setCursor(78, 225); tft.println(F("TX/RX    /"));
  wdt_reset();
}

void drawGUI2() {

  for (uint8_t i = 0; i <= 10; i++) {
  cmd[i].lastValue = -32768;
}
   
  tft.fillScreen(GC9A01A_BLACK);
  tft.setTextSize(2); tft.setTextColor(GC9A01A_CYAN);
  tft.setCursor(60, 25); tft.println(F("Speed:"));
  lastKat = KAT;
  obrotomierz(lastKat, lastKat+300, 0x39E7);
  tft.setTextSize(2);
  tft.setCursor(30, 70); tft.println(F("RPM:"));
  tft.setCursor(25, 140); tft.println(F("Air Temp:"));
  tft.setCursor(155, 140); tft.print(F("  0")); tft.write(0xF7); tft.println(F("C"));
  tft.setCursor(25, 160); tft.println(F("Water Temp:"));
  tft.setCursor(155, 160); tft.print(F("  0")); tft.write(0xF7);  tft.println(F("C"));
  tft.setCursor(25, 180); tft.println(F("Trans Temp:"));
  tft.setCursor(155, 180); tft.print(F("  0")); tft.write(0xF7); tft.println(F("C"));
  tft.setCursor(72, 200); tft.println(F("Bat:"));
  tft.setTextSize(1); tft.setTextColor(GC9A01A_YELLOW);
  tft.setCursor(78, 225); tft.println(F("TX/RX    /"));
  wdt_reset();
}

void drawClockGUI() {
      tft2.fillScreen(GC9A01A_BLACK);
      lastKat2 = KAT;
    obrotomierz(lastKat2, lastKat2+300, 0x39E7, false);
    lastHour = -1; lastMinute = -1; lastDay = -1; lastMonth = -1; lastYear = -1; lastWeekday = -1;
    cmd[TEMP_AC].lastValue = -999; cmd[HUM].lastValue = -999; cmd[TEMP_OUT].lastValue = -999; cmd[TEMP_IN].lastValue = -999;
    wdt_reset();
}
