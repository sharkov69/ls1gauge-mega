
constexpr uint16_t MENU_TIMEOUT_MS = 5000;
bool menuActive = false;
bool pauseSubActive = false;
uint16_t menuStartTime = 0;
uint8_t menuPosition = 0;
constexpr uint16_t debounceTime = 300;
bool ekranWlaczony = true;

const char* menuOptions[6] = {
  "Show pictures",
  "Gauge View",
  "Clock View",
  token,
  "VPW pause",
  "Restart"
};

uint8_t screenPowerPhase = 0;
uint32_t screenPowerSince = 0;
void screenCommand(uint8_t command) {
  tft.startWrite(); tft.writeCommand(command); tft.endWrite();
  tft2.startWrite(); tft2.writeCommand(command); tft2.endWrite();
}
void wygasEkran() {
  screenCommand(GC9A01A_SLPIN);
  screenPowerSince = millis(); screenPowerPhase = 1;
}
void obudzEkran() {
  screenCommand(GC9A01A_DISPON);
  screenPowerSince = millis(); screenPowerPhase = 2;
}
bool screenPowerReady() { return ekranWlaczony && screenPowerPhase == 0; }
void updateScreenPower() {
  const uint32_t elapsed = millis() - screenPowerSince;
  if (screenPowerPhase == 1 && elapsed >= 5) {
    screenCommand(GC9A01A_DISPOFF); screenPowerPhase = 0;
  } else if (screenPowerPhase == 2 && elapsed >= 5) {
    screenCommand(GC9A01A_SLPOUT); screenPowerPhase = 3; screenPowerSince = millis();
  } else if (screenPowerPhase == 3 && elapsed >= 120) screenPowerPhase = 0;
}
void drawMenu() {
  int x = 30, y = 48, w = 180, h = 144;
  tft2.fillRoundRect(x, y, w, h, 8, GC9A01A_DARKGREY);  // tło menu

  for (uint8_t i = 0; i < 6; i++) {
    int rowY = y + 5 + i * 22;
    uint16_t color = (i == menuPosition) ? GC9A01A_WHITE : GC9A01A_BLACK;
    uint16_t bg = (i == menuPosition) ? GC9A01A_BLUE : GC9A01A_DARKGREY;

    tft2.fillRect(x + 5, rowY, w - 10, 20, bg);
    tft2.setCursor(x + 10, rowY + 5);
    tft2.setTextColor(color);
    tft2.setTextSize(2);
    tft2.print(menuOptions[i]);
  }
  wdt_reset();
}

void drawPauseSubmenu() {
  int x = 30, y = 48, w = 180, h = 144;
  tft2.fillRoundRect(x, y, w, h, 8, GC9A01A_DARKGREY);

  tft2.setTextSize(2);
  tft2.setTextColor(GC9A01A_WHITE);
  tft2.setCursor(x + 10, y + 10);
  tft2.print(F("VPW pause (ms)"));

  // wartość duża, wyśrodkowana
  char buf[6];
  snprintf(buf, sizeof(buf), "%u", pause);
  int16_t x1, y1; uint16_t tw, th;
  tft2.setTextSize(3);
  tft2.getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
  tft2.setCursor(x + (w - tw) / 2, y + 30);
  tft2.setTextColor(GC9A01A_YELLOW);
  tft2.print(buf);

  // podpowiedzi
  tft2.setTextSize(2);
  tft2.setTextColor(GC9A01A_WHITE);
  tft2.setCursor(x + 10, y + h - 60);
  tft2.print(F("DOWN: -10 ms"));
  tft2.setCursor(x + 10, y + h - 80);
  tft2.print(F("UP:   +10 ms"));
  tft2.setCursor(x + 10, y + h - 40);
  tft2.print(F("OK:   BACK"));
  wdt_reset();
}

void handleMenuOption(uint8_t pos) {
  // wykonaj działanie
  switch (pos) {
    case 0: DBGL("Wybrano Opcja 1 - obrazek"); 
        if(guiStarted) {
    #ifdef LOGO
    drawStartScreen();
    #endif
    }
    guiStarted = false;
    guiStartTime = (uint16_t)(millis() & 0xFFFF);
    nextBMP();
    break;
    case 1: DBGL("Wybrano Opcja 2 - gauge"); 
    if (guiMode) { drawGUI2(); }
    else { drawGUI();}
    guiMode = !guiMode;
    EEPROM.update(EEPROM_GUI_ADDR, guiMode);
    break;
    case 2: DBGL("Wybrano Opcja 3 - clock"); 
        drawClockGUI();
    clockMode = !clockMode;
    EEPROM.update(EEPROM_CLOCK_ADDR, clockMode);
    drawMenu();
    break;
    case 3: DBGL("Wybrano Opcja 4 - token"); 
    DBGL("Generuję nowy token...");
    generateToken(token, TOKEN_LENGTH);
    saveTokenToEEPROM(token);
    DBG("Token: ");
    DBGL(token);
    break;
    case 4: DBGL("Wybrano Opcja 5 - VPW pause"); 
    pauseSubActive = true;
    drawPauseSubmenu();
    break;
    case 5: DBGL("Wybrano Opcja 6 - restart"); 
    resetFunc();
    break;
  }
}
