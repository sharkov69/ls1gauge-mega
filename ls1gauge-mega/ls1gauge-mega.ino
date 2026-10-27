//#define SERIAL_TX_BUFFER_SIZE 256
#include <avr/wdt.h>

#define LOGO
bool demo;
bool guiMode;
bool clockMode;
uint8_t pause;
//#define TXRX
//#define DEBUGVPW
//#define DEBUG
#ifdef DEBUG
  #define DBG(x) Serial.print(x)
  #define DBGL(x) Serial.println(x)
  #define DBG2(x, y)    Serial.print(x, y)
  #define DBGL2(x, y)    Serial.println(x, y)
#else
  #define DBG(x)
  #define DBGL(x)
  #define DBG2(x, y)
  #define DBGL2(x, y)
#endif

#include <j1850vpw-wc.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"

#define BUTTON1_PIN 27
#define BUTTON2_PIN 29
#define BUTTON3_PIN 31

#define ILE 20

#define TFT_DC    11
#define TFT_CS    9
#define TFT_RST   7
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

#define TFT2_DC    17
#define TFT2_CS    15
#define TFT2_RST   13
Adafruit_GC9A01A tft2(TFT2_CS, TFT2_DC, TFT2_RST);

uint8_t stats[2][ILE] = {0};
uint8_t current{}, counter{}, counter2{};
uint16_t lastStats{};

constexpr int16_t KAT = 210;
int16_t lastKat = KAT;
int16_t lastKat2 = KAT;
//#ifdef DEBUG
//uint16_t errorTimestamps[10];
//uint8_t errorIndex = 0;
//#endif

uint16_t lastUpdate = 0;
uint16_t refreshInterval = 400;
uint16_t refreshInterval2 = 3000;
uint16_t guiStartTime;
bool guiStarted = false;
bool clock = true;

void(* resetFunc) (void) = 0;

#include "font.h"
#include "temp.h"
#include "cmd.h"
#include "obrotomierz.h"
#ifdef LOGO
#include "images.h"
#endif
#include "rtc.h"
#include "sd.h"
#include "drawbmp.h"
#include "modem.h"
#include "modem-post.h"

#include "display.h"

#include "menu.h"

  /*extern int __heap_start, *__brkval;
int freeMemory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
} */

void setup() {
  //cli();
  wdt_disable();
  //sei();

  Serial.begin(115200);
  Serial1.begin(115200);
  pinMode(12,OUTPUT);
  digitalWrite(12, LOW);
  //delay(500);
  //digitalWrite(12, HIGH);

  modemInitStart();

  sensors.begin();

  /*DBG(F("Wykryto czujnikow: "));
  DBGL(sensors.getDeviceCount());
  DeviceAddress addr;
  for (uint8_t i = 0; i < sensors.getDeviceCount(); i++) {
    sensors.getAddress(addr, i);
    DBG(F("Czujnik "));
    DBG(i);
    DBG(F(" adres = { "));
    for (uint8_t j = 0; j < 8; j++) {
      DBG(F("0x"));
      if (addr[j] < 0x10) DBG("0");
      DBG(addr[j], HEX);
      if (j < 7) DBG(F(", "));
    }
    DBGL(F(" }"));
  }*/
  sensors.setWaitForConversion(false);
  sensors.setResolution(tempIn, 11); 
  sensors.setResolution(tempOut, 11);
  sensors.requestTemperatures();

  #ifdef DEBUG
  DBGL(F("Begin setup."));
  #endif
  dht.begin();
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  if (!rtc.begin()) {
    DBGL(F("RTC nie działa!"));
    while (1) delay(10);
  }
  rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
  rtc.disable32K();
  sprawdzLetni();
  pinMode(sqwPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(sqwPin), sqwISR, FALLING);
  if (!SD.begin(sdCS,SPI_FULL_SPEED)) {
    DBGL(F("Blad SD!"));
  }
  loadBMP();
  readTokenFromEEPROM(token);

  if (token[0] < 33 || token[0] > 126) {
    DBGL(F("Generuję nowy token..."));
    generateToken(token, TOKEN_LENGTH);
    saveTokenToEEPROM(token);
  }

  DBG(F("Token: "));
  DBGL(token);
  demo = EEPROM.read(EEPROM_DEMO_ADDR);
  guiMode = EEPROM.read(EEPROM_GUI_ADDR);
  clockMode = EEPROM.read(EEPROM_CLOCK_ADDR);
  pause = EEPROM.read(EEPROM_PAUSE_ADDR);
  if (pause < 25 || pause > 125) {
    pause = 75;
    EEPROM.update(EEPROM_PAUSE_ADDR, pause);
  }

  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);
  tft2.begin();
  tft2.fillScreen(GC9A01A_BLACK);
  #ifdef LOGO
  drawCamaroLogo(); 
  #endif
  randomSeed(analogRead(A0));
  #ifdef DEBUGVPW
  vpw.onError(handleError);
  #endif
  vpw.init(RX, TX);
 
  lastStats = (uint16_t)(millis() & 0xFFFF);
  guiStartTime = (uint16_t)(millis() & 0xFFFF);
  netLastCheck = (uint16_t)(millis() & 0xFFFF);
  #ifdef DEBUG
  DBGL(F("Setup done."));
  #endif
  wdt_enable(WDTO_4S);    

}

void loop(void) {
  //DBG(F("Free RAM: "));
  //DBGL(freeMemory());

  czas = (uint16_t)(millis() & 0xFFFF);
  send();
  read();

  if (!guiStarted && (uint16_t)(czas - guiStartTime) >= 5000) {
    guiStarted = true;
    if (guiMode) {
      drawGUI();
    } else { drawGUI2();}
    drawClockGUI(); 

  }
  if (guiStarted && (uint16_t)(czas - lastUpdate) >= refreshInterval) {
    lastUpdate = czas;

  //char buf[6];
  if(demo) {
    cmd[RPM].wartosc = randomDelta(cmd[RPM].wartosc, 500, 6000, 0.1);
    cmd[SPEED].wartosc  = randomDelta(cmd[SPEED].wartosc, 0, 200, 0.1);
    cmd[TPS].wartosc  = randomDelta(cmd[TPS].wartosc, 5, 100, 0.1);
    cmd[TRANS].wartosc  = randomDelta(cmd[TRANS].wartosc, 20, 100, 0.1);
    cmd[WATER].wartosc  = randomDelta(cmd[WATER].wartosc, 20, 100, 0.1);
    cmd[AIR].wartosc  = randomDelta(cmd[AIR].wartosc, 5, 35, 0.1);
    cmd[O2].wartosc  = randomDelta(cmd[O2].wartosc, 1000, 1900, 0.1);
    cmd[O22].wartosc  = randomDelta(cmd[O22].wartosc, 1000, 1900, 0.1);
    cmd[SPARK].wartosc  = randomDelta(cmd[SPARK].wartosc, -10, 30, 0.1);
    cmd[BATT].wartosc  = randomDelta(cmd[BATT].wartosc, 131, 146, 0.1);
    cmd[MAP].wartosc = randomDelta(cmd[MAP].wartosc, 30, 70, 0.1);
    counter += random(0,10);
    //refreshInterval = 500;
  } //else {refreshInterval = 250;}

  if (cmd[RPM].wartosc > 6000) {cmd[RPM].wartosc = 6000;}
  if (cmd[RPM].wartosc < 0) {cmd[RPM].wartosc = 0;}
  cmd[RPM].wartosc = ((cmd[RPM].wartosc + 12) / 25) * 25;
  if (guiMode) {
display(SPEED, cmd[SPEED].wartosc);
display(RPM,   cmd[RPM].wartosc);
display(TPS,   cmd[TPS].wartosc);
display(BATT,  cmd[BATT].wartosc);
display(O2,    cmd[O2].wartosc);
display(MAP,   cmd[MAP].wartosc);
display(O22,   cmd[O22].wartosc);
display(SPARK, cmd[SPARK].wartosc);
  } else {

 tft.setTextSize(3);
 tft.setTextColor(GC9A01A_WHITE);
 tft.fillRect(90, 50, 80, 22, GC9A01A_BLACK);
  if (cmd[SPEED].wartosc < 0) {cmd[SPEED].wartosc = 0;}
  String buf = String(cmd[SPEED].wartosc);
  int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(119 - w / 2, 50);
    tft.print(buf);

  if (cmd[RPM].wartosc > 4499) {
    tft.setTextColor(GC9A01A_RED);
  } else { tft.setTextColor(GC9A01A_GREEN);}

  tft.setTextSize(1);
  tft.setFont(&Neue_Leights28pt7b);
  
    tft.fillRect(50, 90, 140, 45, GC9A01A_BLACK);
    buf = String(cmd[RPM].wartosc);
    tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(119 - w / 2, 130);
    tft.print(buf);
    tft.setFont();
    display(13, cmd[BATT].wartosc);
  }

display(AIR,   cmd[AIR].wartosc);
display(WATER, cmd[WATER].wartosc);
display(TRANS, cmd[TRANS].wartosc);

  

  int16_t zielony = KAT + map(3500, 0, 6000, 0, 300);
  int16_t zolty = KAT + map(4500, 0, 6000, 0, 300);
  int16_t kat = KAT + map(cmd[RPM].wartosc, 0, 6000, 0, 300);
  if (kat > lastKat) {
    int16_t from = lastKat;
    if (from < zielony) {
      int16_t to = min(kat, zielony);
      obrotomierz(from, to, GC9A01A_GREEN);
      from = to;
    }
    if (from < zolty && kat > zielony) {
      int16_t to = min(kat, zolty);
      obrotomierz(from, to, GC9A01A_YELLOW);
      from = to;
    }
    if (from < kat && kat > zolty) {
      obrotomierz(from, kat, GC9A01A_RED);
    }
  } else if (kat < lastKat) {
    obrotomierz(lastKat, kat, 0x39E7);
  }
lastKat = kat;
    //DBGL(F("O K"));
    //#ifdef DEBUG 
    //display(142, 225, 35, 18, getRecentErrorCount(), -1, 0, 1, 2, GC9A01A_BLUE);
    //#endif
  }
if ((uint16_t)(czas - lastStats) >= refreshInterval2) {
    lastStats = czas;

    current++;
    if (current >= ILE) current = 0;

    // wpisz nową wartość do historii
    stats[0][current] = counter;
    counter = 0;
    stats[1][current] = counter2;
    counter2 = 0;
    // policz sumę z ostatnich 60 sek
    uint16_t sumrx = 0;
    uint16_t sumtx = 0;
    for (uint8_t i = 0; i < ILE; i++) {
      sumrx += stats[0][i];
      sumtx += stats[1][i];
    }
       //#ifdef DEBUG 
      //DBG(F("RX: Avg frame per 60 sec: ")); DBGL(sum);
      //DBG(F("TX: Avg frame per 60 sec: ")); DBGL(sum2);
      //#endif 
      if (guiStarted) {
      display(11, sumrx);
      display(12, sumtx);

      }

  }

  if (!menuActive && digitalRead(BUTTON2_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
    menuActive = true;
    menuStartTime = czas;
    menuPosition = 0;
    if (!ekranWlaczony) {
      obudzEkran();
      ekranWlaczony = true;
    }
    drawMenu();
  }
  if (!menuActive && digitalRead(BUTTON1_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
    menuStartTime = czas;
            if (ekranWlaczony) {
      wygasEkran();
    } else {
      obudzEkran();
    }
    ekranWlaczony = !ekranWlaczony;
  }
    if (!menuActive && digitalRead(BUTTON3_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
    menuStartTime = czas;
    demo = !demo;
    EEPROM.update(EEPROM_DEMO_ADDR, demo);
  }
  if (pauseSubActive) {
  if ((uint16_t)(czas - menuStartTime) >= MENU_TIMEOUT_MS) {
    pauseSubActive = false;
    menuActive = false;
    drawClockGUI();
    drawTFT2();
    return;
  }

  if (digitalRead(BUTTON3_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
    menuStartTime = czas;
    if (pause >= 25 + 10) {
      pause -= 10;
      EEPROM.update(EEPROM_PAUSE_ADDR, pause);
      drawPauseSubmenu();
    }
  }
  if (digitalRead(BUTTON1_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
    menuStartTime = czas;
    if (pause + 10 <= 125) {
      pause += 10;
      EEPROM.update(EEPROM_PAUSE_ADDR, pause);
      drawPauseSubmenu();
    }
  }
  if (digitalRead(BUTTON2_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
    // OK / BACK → wyjście do głównego menu
    menuStartTime = czas;
    pauseSubActive = false;
    menuActive = true;
    drawMenu();
  }
}
  if (menuActive) {
    if ((uint16_t)(czas - menuStartTime) >= MENU_TIMEOUT_MS) {
      menuActive = false;
      drawClockGUI();
      drawTFT2();
      return;
    }
    if (digitalRead(BUTTON1_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
      menuStartTime = czas;
      menuPosition = (menuPosition + 5) % 6; // w górę
      drawMenu();
    }
    if (digitalRead(BUTTON3_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
      menuStartTime = czas;
      menuPosition = (menuPosition + 1) % 6; // w dół
      drawMenu();
    }
    if (digitalRead(BUTTON2_PIN) == LOW && (uint16_t)(czas - menuStartTime) > debounceTime) {
      menuStartTime = czas;
      handleMenuOption(menuPosition);
    }
  }
  
  if (bmpDrawing) {
  continueDrawBMP();
}
  if ((uint16_t)(czas - lastDHTread) >= SENSORS_INTERVAL) {
    lastDHTread = czas;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      cmd[TEMP_AC].wartosc = (int16_t)(t >= 0 ? t * 10.0 + 0.5 : t * 10.0 - 0.5);
      cmd[HUM].wartosc = (int16_t)(h >= 0 ? h * 10.0 + 0.5 : h * 10.0 - 0.5);
      /*DBG(F("Temp DHT: "));
      DBG(t);
      DBG(F(" *C, Hum: "));
      DBG(h);
      DBG(F("%"));*/
    } else {
      //DBGL(F("Błąd odczytu z DHT22")); 
    }
    //float tIn = sensors.getTempC(tempIn);
    //float tOut = sensors.getTempC(tempOut);
    //DBG(F(", Temp In: ")); DBG(tIn);DBG(F(" *C"));
    //DBG(F(", Temp Out: ")); DBG(tOut);DBGL(F(" *C"));
cmd[TEMP_OUT].wartosc = (int16_t)((sensors.getTemp(tempOut) * 10 + 64) >> 7);
cmd[TEMP_IN].wartosc = (int16_t)((sensors.getTemp(tempIn)  * 10 + 64) >> 7);
    sensors.requestTemperatures(); // rozpocznij kolejny pomiar
    //cmd[15].wartosc = (int16_t)(tOut >= 0 ? tOut * 10.0 + 0.5 : tOut * 10.0 - 0.5);
    //cmd[16].wartosc = (int16_t)(tIn >= 0 ? tIn * 10.0 + 0.5 : tIn * 10.0 - 0.5);
    if (modemInitDone()) {
    if(!stopModemWatchdog) {
      getGPSInfo();
    }
    }
  }

  if (tick) {
    tick = false;
    now = rtc.now();

       tft.setTextSize(2);
       tft.setTextColor(GC9A01A_RED);
       if (modemInitDone() && modemOnline) {
        blinkState = !blinkState;

        if (blinkState) {
             tft.setCursor(25, 55); 
             tft.print("*");
        } else {
             tft.setCursor(25, 55);
             tft.fillRect(25, 55, 15, 15, GC9A01A_BLACK);
        }
    } else {
         tft.setCursor(25, 55);
         tft.fillRect(25, 55, 15, 15, GC9A01A_BLACK);
        blinkState = false;
    }


    /*DBG(F("Czas: "));
    if (now.hour() < 10) DBG('0');
    DBG(now.hour());
    DBG(':');
    if (now.minute() < 10) DBG('0');
    DBG(now.minute());
    DBG(':');
    if (now.second() < 10) DBG('0');
    DBGL(now.second());
    */
    //DBG(F(", Temp RTC: "));
  //float tempRTC = rtc.getTemperature()-2.75;
  //DBG(tempRTC);
  //cmd[16].wartosc = (int16_t)(tempRTC >= 0 ? tempRTC * 10.0 + 0.5 : tempRTC * 10.0 - 0.5);
  //DBGL(F("*C"));
  for (uint8_t i = 0; i < CMD_SIZE; i++) {
    sums[i] += (cmd[i].wartosc == -32768) ? 0 : cmd[i].wartosc;
    if (i == 15 || i== 16) {
      sums[i+2] += (cmd[i].lastValue == -32768) ? 0 : cmd[i].lastValue;
    }
  }
    count++;
    if (count >= SREDNIA) {
      addLogEntry();
      count = 0;
    }
    if (logIndex >= LOG_ENTRIES) {
      dumpLogToSD();
      //memcpy(logHttpBuffer, logBuffer, sizeof(logBuffer));
      if (modemInitDone()) {
      startSendLog();
      }
      //getGPSInfo();
      logIndex = 0;
    }
    if (!menuActive) {
      drawTFT2();
    }
  
  }
  static String buffer = "";
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buffer.startsWith("SETTIME=") && buffer.length() >= 27) {
        int y = buffer.substring(8, 12).toInt();
        int m = buffer.substring(13, 15).toInt();
        int d = buffer.substring(16, 18).toInt();
        int h = buffer.substring(19, 21).toInt();
        int mi = buffer.substring(22, 24).toInt();
        int s = buffer.substring(25, 27).toInt();
        DateTime compiled(DateTime(y, m, d, h, mi, s));
        EEPROM.update(EEPROM_DST_FLAG_ADDR, czyLetni(compiled.day(), compiled.month(), compiled.dayOfTheWeek()) ? 1 : 0);
        rtc.adjust(compiled);
      }
      buffer = "";
    } else {
      buffer += c;
    }
  }
  modemInitTick();
  if (modemInitDone()) {
  updateSendLog();

  if(!stopModemWatchdog) {
  networkCheck_async();
  //DBGL(F("ModemWathdog on!"));
  if (networkCheckIsDone()) {
    if (isNetworkReady() ) {
      //DBGL(F("Modem online"));
      modemOnline = true;
      netLastCheck = czas;
  } else {
    if ((uint16_t)(czas - netLastCheck) > NET_TIMEOUT) {
      DBGL(F("Modem: offline... reset"));
      modemInitStart();
      netLastCheck = czas;
    } else {
      DBGL(F("Modem: offline... czekam"));
      modemOnline = false;
    }
  }
}
  }
  }
 //DBGL(F("Loop"));
   static uint32_t lastKick = 0;
  uint32_t now = millis();
    if (now - lastKick >= 2000) {              // nie musisz karmić co iterację
      wdt_reset();
      lastKick = now;
    }
  while ((uint16_t)(millis() - czas) < 5);
}