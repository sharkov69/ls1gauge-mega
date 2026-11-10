#pragma once
constexpr uint16_t MENU_TIMEOUT_MS = 5000;
constexpr uint16_t DEMO_INTERVAL_MS = 500;
constexpr uint8_t MENU_OPTION_COUNT = 6;
constexpr uint16_t debounceTime = 150;
uint16_t menuStartTime = 0, lastDemoUpdate = 0;
uint8_t menuPosition = 0;
bool ekranWlaczony = true;

uint8_t screenPowerPhase = 0;
uint32_t screenPowerSince = 0;
void screenCommand(uint8_t command) {
	tft.startWrite();
	tft.writeCommand(command);
	tft.endWrite();
	tft2.startWrite();
	tft2.writeCommand(command);
	tft2.endWrite();
}
void wygasEkran() {
	screenCommand(GC9A01A_SLPIN);
	screenPowerSince = millis();
	screenPowerPhase = 1;
}
void obudzEkran() {
	screenCommand(GC9A01A_DISPON);
	screenPowerSince = millis();
	screenPowerPhase = 2;
}
bool screenPowerReady() {
	return ekranWlaczony && screenPowerPhase == 0;
}
void updateScreenPower() {
	const uint32_t elapsed = millis() - screenPowerSince;
	if (screenPowerPhase == 1 && elapsed >= 5) {
		screenCommand(GC9A01A_DISPOFF);
		screenPowerPhase = 0;
	} else if (screenPowerPhase == 2 && elapsed >= 5) {
		screenCommand(GC9A01A_SLPOUT);
		screenPowerPhase = 3;
		screenPowerSince = millis();
	} else if (screenPowerPhase == 3 && elapsed >= 120)
		screenPowerPhase = 0;
}

void drawMenu() {
	const int x = 30, y = 48, w = 180, h = 144;
	tft2.fillRoundRect(x, y, w, h, 8, displayColor(GC9A01A_DARKGREY));
	tft2.setFont();
	for (uint8_t i = 0; i < MENU_OPTION_COUNT; ++i) {
		const int rowY = y + 5 + i * 22;
		const uint16_t color = i == menuPosition ? GC9A01A_WHITE : GC9A01A_BLACK;
		const uint16_t background = i == menuPosition ? GC9A01A_BLUE : GC9A01A_DARKGREY;
		tft2.fillRect(x + 5, rowY, w - 10, 20, displayColor(background));
		tft2.setCursor(x + 10, rowY + 5);
		tft2.setTextColor(displayColor(color));
		tft2.setTextSize(2);
		switch (i) {
		case 0: tft2.print(F("Show pictures")); break;
		case 1: tft2.print(F("Gauge View")); break;
		case 2: tft2.print(F("Clock View")); break;
		case 3: tft2.print(demo ? F("Demo: ON") : F("Demo: OFF")); break;
		case 4: tft2.print(darkMode ? F("Dark: ON") : F("Dark: OFF")); break;
		case 5: tft2.print(F("Restart")); break;
		}
	}
	wdt_reset();
}

void closeMenu() {
	menuActive = false;
	if (guiStarted) {
		drawClockGUI();
		drawTFT2();
	} else {
#ifdef LOGO
		drawStartScreen();
#endif
	}
}

void handleMenuOption(uint8_t pos) {
	switch (pos) {
	case 0:
		menuActive = false;
		guiStarted = false;
		guiStartTime = (uint16_t)millis();
#ifdef LOGO
		drawStartScreen();
#endif
		nextBMP();
		break;
	case 1:
		guiMode = !guiMode;
		EEPROM.update(EEPROM_GUI_ADDR, guiMode);
		if (guiStarted) {
			if (guiMode) drawGUI();
			else drawGUI2();
		}
		break;
	case 2:
		clockMode = !clockMode;
		EEPROM.update(EEPROM_CLOCK_ADDR, clockMode);
		closeMenu();
		break;
	case 3:
		demo = !demo;
		lastDemoUpdate = (uint16_t)millis();
		EEPROM.update(EEPROM_DEMO_ADDR, demo);
		drawMenu();
		break;
	case 4:
		darkMode = !darkMode;
		EEPROM.update(EEPROM_DARK_ADDR, darkMode);
		if (guiStarted) {
			if (guiMode) drawGUI();
			else drawGUI2();
		} else {
#ifdef LOGO
			drawCamaroLogo();
#endif
			if (bmpDrawing) startDrawBMP(currentBMP);
		}
		closeMenu();
		break;
	case 5:
		resetFunc();
		break;
	}
}

uint8_t menuEvents[16] = {};
uint8_t menuEventHead = 0, menuEventCount = 0;
void pollMenuButtons(uint16_t nowMs) {
	static uint8_t previousButtons = 0;
	static uint16_t lastPress = 0;
	const uint8_t buttons = (digitalRead(BUTTON1_PIN) == LOW ? 1 : 0) |
	                        (digitalRead(BUTTON2_PIN) == LOW ? 2 : 0) |
	                        (digitalRead(BUTTON3_PIN) == LOW ? 4 : 0);
	const uint8_t pressed = buttons & ~previousButtons;
	previousButtons = buttons;
	if (!pressed || (uint16_t)(nowMs - lastPress) <= debounceTime)
		return;
	lastPress = nowMs;
	if (menuEventCount < sizeof(menuEvents)) {
		menuEvents[(menuEventHead + menuEventCount) % sizeof(menuEvents)] = pressed;
		++menuEventCount;
	}
}
void handleMenuButtons(uint16_t nowMs) {
	pollMenuButtons(nowMs);
	if (!menuEventCount) {
	if (menuActive && (uint16_t)(nowMs - menuStartTime) >= MENU_TIMEOUT_MS)
		closeMenu();
		return;
	}
	const uint8_t pressed = menuEvents[menuEventHead];
	menuEventHead = (menuEventHead + 1) % sizeof(menuEvents);
	--menuEventCount;
	menuStartTime = nowMs;
	if (!menuActive) {
		if (pressed & 2) {
			menuActive = true;
			menuPosition = 0;
			if (!ekranWlaczony) {
				obudzEkran();
				ekranWlaczony = true;
			}
			drawMenu();
		} else if (pressed & 1) {
			if (ekranWlaczony) wygasEkran();
			ekranWlaczony = false;
		} else if (pressed & 4) {
			if (!ekranWlaczony) obudzEkran();
			ekranWlaczony = true;
		}
		return;
	}
	if (pressed & 1) {
		menuPosition = (menuPosition + MENU_OPTION_COUNT - 1) % MENU_OPTION_COUNT;
		drawMenu();
	} else if (pressed & 4) {
		menuPosition = (menuPosition + 1) % MENU_OPTION_COUNT;
		drawMenu();
	} else if (pressed & 2) {
		handleMenuOption(menuPosition);
	}
}

void updateStartupScreen(uint16_t nowMs) {
	if (guiStarted)
		return;
	// Count the viewing time after the last BMP block, not while it is loading.
	if (bmpDrawing) {
		guiStartTime = nowMs;
		return;
	}
	if (menuActive || (uint16_t)(nowMs - guiStartTime) < 5000)
		return;
	guiStarted = true;
	if (guiMode) drawGUI();
	else drawGUI2();
	drawClockGUI();
	drawTFT2();
}

void updateDemo(uint16_t nowMs) {
	if (!demo || !guiStarted || !screenPowerReady() ||
	    (uint16_t)(nowMs - lastDemoUpdate) < DEMO_INTERVAL_MS)
		return;
	lastDemoUpdate = nowMs;
	channels[RPM].set(randomDelta(channels[RPM].value, 500, 6000, 0.1));
	channels[SPEED].set(randomDelta(channels[SPEED].value, 0, 200, 0.1));
	channels[TPS].set(randomDelta(channels[TPS].value, 5, 100, 0.1));
	channels[TRANS].set(randomDelta(channels[TRANS].value, 20, 100, 0.1));
	channels[WATER].set(randomDelta(channels[WATER].value, 20, 100, 0.1));
	channels[AIR].set(randomDelta(channels[AIR].value, 5, 35, 0.1));
	channels[O2].set(randomDelta(channels[O2].value, 1000, 1900, 0.1));
	channels[O22].set(randomDelta(channels[O22].value, 1000, 1900, 0.1));
	channels[OIL].set(randomDelta(channels[OIL].value, -10, 30, 0.1));
	channels[BATT].set(randomDelta(channels[BATT].value, 131, 146, 0.1));
	channels[MAP].set(randomDelta(channels[MAP].value, 30, 70, 0.1));
}
