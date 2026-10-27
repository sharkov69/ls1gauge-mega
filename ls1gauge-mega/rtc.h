#include <RTClib.h>
RTC_DS3231 rtc;
const byte sqwPin = 2;
volatile bool tick = false;
DateTime now;

#include <EEPROM.h>
#define EEPROM_DST_FLAG_ADDR 0
#define EEPROM_TOKEN_ADDR 1
#define EEPROM_DEMO_ADDR 14
#define EEPROM_GUI_ADDR 15
#define EEPROM_CLOCK_ADDR 16
#define EEPROM_PAUSE_ADDR 17

#define TOKEN_LENGTH 12

const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                       "abcdefghijklmnopqrstuvwxyz"
                       "0123456789"
                       "!@#$%^&*";

char token[TOKEN_LENGTH + 1]; // +1 na null

void generateToken(char* buffer, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = charset[random(0, sizeof(charset) - 1)];
  }
  buffer[length] = '\0';
}

void saveTokenToEEPROM(const char* buffer) {
  for (uint8_t i = 0; i < TOKEN_LENGTH; i++) {
    EEPROM.write(EEPROM_TOKEN_ADDR + i, buffer[i]);
  }
  EEPROM.write(EEPROM_TOKEN_ADDR + TOKEN_LENGTH, '\0'); // null terminator
}

void readTokenFromEEPROM(char* buffer) {
  for (uint8_t i = 0; i <= TOKEN_LENGTH; i++) {
    buffer[i] = EEPROM.read(EEPROM_TOKEN_ADDR + i);
  }
}

bool czyLetni(int day, int month, int weekday) {
  if (month < 3 || month > 10) return false;   // styczeń–luty, listopad–grudzień → zimowy
  if (month > 3 && month < 10) return true;    // kwiecień–wrzesień → letni

  int lastSunday = 31 - ((weekday + 31 - day) % 7);  // dzień ostatniej niedzieli w danym miesiącu

  if (month == 3)  return (day >= lastSunday);       // w marcu — po ostatniej niedzieli → letni
  if (month == 10) return (day < lastSunday);        // w październiku — do ostatniej niedzieli → letni

  return false;
}

void sprawdzLetni() {
  DateTime now = rtc.now();
  bool nowDST = czyLetni(now.day(), now.month(), now.dayOfTheWeek());
  uint8_t savedDST = EEPROM.read(EEPROM_DST_FLAG_ADDR);

  if (nowDST && savedDST == 0) {
    // był zapisany zimowy, teraz jest letni — dodaj godzinę
    now = now + TimeSpan(3600);
    rtc.adjust(now);
    EEPROM.update(EEPROM_DST_FLAG_ADDR, 1);
  } else if (!nowDST && savedDST == 1) {
    // był zapisany letni, teraz jest zimowy — odejmij godzinę
    now = now - TimeSpan(3600);
    rtc.adjust(now);
    EEPROM.update(EEPROM_DST_FLAG_ADDR, 0);
  }
}
const char* dni[] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};
int lastHour = -1, lastMinute = -1, lastDay = -1, lastMonth = -1, lastYear = -1, lastWeekday = -1;

void sqwISR() {
  tick = true;
}

void drawTFT2(){
    if(guiStarted) {
int16_t kat2 = KAT + map(now.second(), 0, 60, 0, 300);

if (kat2 > lastKat2) {
  obrotomierz(lastKat2, kat2, GC9A01A_WHITE, false);
} else if (kat2 < lastKat2) {
  obrotomierz(lastKat2, kat2, 0x39E7, false);  // np. tło, szaro-niebieski
}

lastKat2 = kat2;

/*
     tft2.setTextColor(GC9A01A_CYAN);
    tft2.setTextSize(5);
  if (now.minute() != lastMinute) {
    tft2.fillRect(40, 95, 60, 40, GC9A01A_BLACK);

    tft2.setCursor(40, 95);
    if (now.minute() < 10) tft2.print("0");
    tft2.print(now.minute());
    lastMinute = now.minute();
  }
  if (now.hour() != lastHour) {
    tft2.fillRect(40, 50, 60, 40, GC9A01A_BLACK);
    tft2.setCursor(40, 50);
    if (now.hour() < 10) tft2.print("0");
    tft2.print(now.hour());
    lastHour = now.hour();
  }
*/
if (clockMode) {

  tft2.setTextColor(GC9A01A_CYAN);
  tft2.setTextSize(1);
  tft2.setFont(&Neue_Leights28pt7b);
  if (now.minute() != lastMinute) {
    tft2.fillRect(30, 93, 71, 45, GC9A01A_BLACK);
    String buf = String(now.minute());
    if (now.minute() < 10) buf = "0" + buf;
    int16_t x1, y1;
    uint16_t w, h;
    tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft2.setCursor(90 - w, 133);
    tft2.print(buf);
    lastMinute = now.minute();
  }
  if (now.hour() != lastHour) {
    tft2.fillRect(33, 42, 67, 45, GC9A01A_BLACK);
    String buf = String(now.hour());
    if (now.hour() < 10) buf = "0" + buf;    
    int16_t x1, y1;
    uint16_t w, h;
    tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft2.setCursor(90 - w, 82);
    tft2.print(buf);
    lastHour = now.hour();
  }
  tft2.setFont();
  if (now.day() != lastDay || now.month() != lastMonth || now.year() != lastYear || now.dayOfTheWeek() != lastWeekday) {
    tft2.setTextColor(GC9A01A_WHITE);
    tft2.setTextSize(2);
    tft2.fillRect(105, 50, 108, 80, GC9A01A_BLACK);
    tft2.setCursor(105, 50);
    tft2.print(dni[now.dayOfTheWeek()]);
    tft2.setTextSize(3);
    tft2.setCursor(105, 75);
    tft2.print(now.day()); tft2.print("."); if (now.month() < 10) tft2.print("0"); tft2.print(now.month());
    tft2.setCursor(105, 105);
    tft2.print(now.year());

    lastDay = now.day();
    lastMonth = now.month();
    lastYear = now.year();
    lastWeekday = now.dayOfTheWeek();
  }

tft2.setTextSize(2);
tft2.setTextColor(GC9A01A_YELLOW);
// In Temp
if (cmd[TEMP_IN].wartosc != cmd[TEMP_IN].lastValue) {
  cmd[TEMP_IN].wartosc = constrain(cmd[TEMP_IN].wartosc, -999, 999);
  tft2.fillRect(115, 144, 90, 18, GC9A01A_BLACK);
  tft2.setCursor(70, 145);
  tft2.print("In: ");
  tft2.print(cmd[TEMP_IN].wartosc / 10.0, 1); tft2.write(0xF7); tft2.print("C");
  cmd[TEMP_IN].lastValue = cmd[TEMP_IN].wartosc;
}
// Out Temp
if (cmd[TEMP_OUT].wartosc != cmd[TEMP_OUT].lastValue) {
  cmd[TEMP_OUT].wartosc = constrain(cmd[TEMP_OUT].wartosc, -999, 999);
  tft2.fillRect(115, 164, 90, 18, GC9A01A_BLACK);
  tft2.setCursor(70, 165);
  tft2.print("Out:");
  tft2.print(cmd[TEMP_OUT].wartosc / 10.0, 1); tft2.write(0xF7); tft2.print("C");
  cmd[TEMP_OUT].lastValue = cmd[TEMP_OUT].wartosc;
}
// Vent Temp
if (cmd[TEMP_AC].wartosc != cmd[TEMP_AC].lastValue) {
  cmd[TEMP_AC].wartosc = constrain(cmd[TEMP_AC].wartosc, -999, 999);
  tft2.fillRect(115, 184, 90, 18, GC9A01A_BLACK);
  tft2.setCursor(70, 185);
  tft2.print("AC: ");
  tft2.print(cmd[TEMP_AC].wartosc / 10.0, 1); tft2.write(0xF7); tft2.print("C");
  cmd[TEMP_AC].lastValue = cmd[TEMP_AC].wartosc;
}
// Humidity
if (cmd[HUM].wartosc != cmd[HUM].lastValue) {
  cmd[HUM].wartosc = constrain(cmd[HUM].wartosc, 99, 999);
  tft2.fillRect(115, 204, 60, 18, GC9A01A_BLACK);
  tft2.setCursor(70, 205);
  tft2.print("Hum:");
  tft2.print(cmd[HUM].wartosc / 10.0, 0);
  tft2.print("%");
  cmd[HUM].lastValue = cmd[HUM].wartosc;
}
    } else {
      tft2.setTextSize(5);
  tft2.setTextColor(GC9A01A_WHITE);
 if (clock) {
      tft2.setCursor(113, 100);
    tft2.print(":");
 } else {
 tft2.fillRect(119, 105, 12, 25, GC9A01A_BLACK);
 }
clock = !clock;
tft2.setTextSize(2);
 tft2.setTextColor(GC9A01A_CYAN);
 tft2.setFont(&Neue_Leights28pt7b);
  if (now.minute() != lastMinute) {
    tft2.fillRect(63, 130, 126, 85, GC9A01A_BLACK);
    String buf = String(now.minute());
    if (now.minute() < 10) buf = "0" + buf;
    int16_t x1, y1;
    uint16_t w, h;
    tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft2.setCursor(120 - w /2, 210);
    tft2.print(buf);
    lastMinute = now.minute();
  }
  if (now.hour() != lastHour) {
    tft2.fillRect(63, 25, 126, 85, GC9A01A_BLACK);
    String buf = String(now.hour());
    if (now.hour() < 10) buf = "0" + buf;    
    int16_t x1, y1;
    uint16_t w, h;
    tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft2.setCursor(120 - w /2, 105);
    tft2.print(buf);
    lastHour = now.hour();
  }
   tft2.setFont();
tft2.setTextSize(2);
tft2.setTextColor(GC9A01A_YELLOW);
// In Temp
if (cmd[TEMP_IN].wartosc != cmd[TEMP_IN].lastValue) {
  cmd[TEMP_IN].wartosc = constrain(cmd[TEMP_IN].wartosc, -999, 999);
  tft2.fillRect(12, 75, 40, 18, GC9A01A_BLACK);
  tft2.setCursor(12, 75);
  tft2.print("In:");
  uint8_t minus = 0;
  if (cmd[TEMP_IN].wartosc / 10.0 < 0) {minus=10;} else {minus=0;}
  tft2.fillRect(12-minus, 95, 50, 18, GC9A01A_BLACK);
  tft2.setCursor(12-minus, 95);
  tft2.print(cmd[TEMP_IN].wartosc / 10.0, 1);
  cmd[TEMP_IN].lastValue = cmd[TEMP_IN].wartosc;
}
// Out Temp
if (cmd[TEMP_OUT].wartosc != cmd[TEMP_OUT].lastValue) {
  cmd[TEMP_OUT].wartosc = constrain(cmd[TEMP_OUT].wartosc, -999, 999);
  tft2.fillRect(12, 115, 45, 18, GC9A01A_BLACK);
  tft2.setCursor(12, 115);
  tft2.print("Out:");
    uint8_t minus = 0;
  if (cmd[TEMP_OUT].wartosc < 0) {minus=10;} else {minus=0;}
   tft2.fillRect(12-minus, 135, 50, 18, GC9A01A_BLACK);
  tft2.setCursor(12-minus, 135);
  tft2.print(cmd[TEMP_OUT].wartosc / 10.0, 1); 
  cmd[TEMP_OUT].lastValue = cmd[TEMP_OUT].wartosc;
}

// Vent Temp
if (cmd[TEMP_AC].wartosc != cmd[TEMP_AC].lastValue) {
  cmd[TEMP_AC].wartosc = constrain(cmd[TEMP_AC].wartosc, -999, 999);
  tft2.fillRect(187, 75, 35, 18, GC9A01A_BLACK);
  tft2.setCursor(187, 75);
  tft2.print("AC:");
    tft2.fillRect(187, 95, 50, 18, GC9A01A_BLACK);
  tft2.setCursor(187, 95);
  tft2.print(cmd[TEMP_AC].wartosc / 10.0, 1); 
  cmd[TEMP_AC].lastValue = cmd[TEMP_AC].wartosc;
}
// Humidity
if (cmd[HUM].wartosc != cmd[HUM].lastValue) {
  cmd[HUM].wartosc = constrain(cmd[HUM].wartosc, 99, 999);
  tft2.fillRect(187, 115, 45, 18, GC9A01A_BLACK);
  tft2.setCursor(187, 115);
  tft2.print("Hum:");
  tft2.fillRect(187, 135, 45, 18, GC9A01A_BLACK);
  tft2.setCursor(187, 135);
  if (cmd[HUM].wartosc > 990) cmd[HUM].wartosc = 990;
  tft2.print(cmd[HUM].wartosc / 10.0, 0);
  tft2.print("%");
  cmd[HUM].lastValue = cmd[HUM].wartosc;
}

    }
  }
}

