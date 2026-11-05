#pragma once
// Small immediate-mode graphics; no framebuffer and no dynamic strings.
void gaugeCircle(Adafruit_GFX &screen, uint8_t x, uint8_t y, const char *label, uint16_t color,
                 uint8_t sx = 2, uint8_t sy = 2) {
	screen.fillRect(x - 14, y - 14, 29, 29, GC9A01A_BLACK);
	screen.drawCircle(x, y, 14, color);
	screen.setFont();
	screen.setTextSize(sx, sy);
	screen.setTextColor(color);
	screen.setCursor(x - ((strlen(label) * 6 - 1) * sx) / 2, y - (7 * sy) / 2);
	screen.print(label);
}
void drawFuelCut(bool on) {
	tft.fillRect(161, 184, 28, 28, GC9A01A_BLACK);
	if (!on)
		return;
	const uint16_t c = GC9A01A_WHITE;
	tft.drawRect(166, 188, 12, 20, c);
	tft.drawRect(168, 191, 8, 7, c);
	tft.drawFastHLine(164, 208, 16, c);
	tft.drawLine(177, 200, 181, 200, c);
	tft.drawLine(181, 200, 181, 206, c);
	tft.drawLine(181, 206, 185, 206, c);
	tft.drawLine(185, 206, 185, 194, c);
	tft.drawLine(185, 194, 181, 190, c);
	for (int8_t d = -1; d <= 1; ++d)
		tft.drawLine(163 + d, 209, 186 + d, 186, GC9A01A_BLACK);
	tft.drawLine(163, 209, 186, 186, c);
	tft.drawLine(164, 209, 187, 186, c);
}
void drawFirstExtra(bool force = false) {
	static uint32_t previous = 0xffffffffUL;
	static int16_t previousSA = -32768;
	const uint32_t nowMs = millis();
	const uint16_t age = dpid.directPolling() ? 3000 : 500;
	const uint8_t gear = channels[PRNDL].fresh(nowMs, age) ? channels[PRNDL].raw() : 0;
	const bool sport = channels[SHIFT_MODE].fresh(nowMs, age) &&
	                   (channels[SHIFT_MODE].raw() == 7 || channels[SHIFT_MODE].raw() == 8);
	const bool tcc = channels[TCC].flagActive(128, nowMs, age), cut = fuelCut(nowMs);
	const uint32_t key =
	    gear | ((uint32_t)sport << 8) | ((uint32_t)tcc << 9) | ((uint32_t)cut << 10);
	if (force || key != previous) {
		char label[3] = {'?', 0, 0};
		uint16_t color = 0x8410;
		if (gear == 1)
			label[0] = 'P';
		else if (gear == 2) {
			label[0] = 'R';
			color = GC9A01A_RED;
		} else if (gear == 4)
			label[0] = 'N';
		else if (gear == 8 || gear == 16 || gear == 32 || gear == 64) {
			label[0] = 'D';
			label[1] = gear == 8 ? '4' : gear == 16 ? '3' : gear == 32 ? '2' : '1';
			color = GC9A01A_GREEN;
		}
		gaugeCircle(tft, 66, 198, label, color);
		gaugeCircle(tft, 102, 198, "S", sport ? GC9A01A_RED : 0x8410);
		gaugeCircle(tft, 138, 198, "TCC", tcc ? GC9A01A_GREEN : 0x8410, 1, 1);
		drawFuelCut(cut);
		previous = key;
	}
	const int16_t sa =
	    channels[SA].fresh(nowMs, age) ? ((int16_t)channels[SA].raw() - 128) / 2 : -32768;
	if (force || sa != previousSA) {
		char value[5];
		if (sa == -32768)
			strcpy(value, " --");
		else
			snprintf(value, sizeof(value), "%3d", sa);
		tft.fillRect(173, 160, 36, 16, GC9A01A_BLACK);
		tft.setFont();
		tft.setTextSize(2);
		tft.setTextColor(GC9A01A_WHITE);
		tft.setCursor(173, 160);
		tft.print(value);
		previousSA = sa;
	}
}
void drawClimateIndicators(bool force) {
	static uint8_t previous = 255;
	const uint32_t nowMs = millis();
	const uint16_t age = dpid.directPolling() ? 3000 : 500;
	const bool ac = channels[AC_STATE].flagActive(1, nowMs, age),
	           fan = channels[FAN_STATE].flagActive(1, nowMs, age);
	const uint8_t key = ac | (fan << 1);
	if (force || key != previous) {
		gaugeCircle(tft2, 38, 162, "A/C", ac ? GC9A01A_GREEN : 0x8410, 1, 2);
		gaugeCircle(tft2, 202, 162, "FAN", fan ? GC9A01A_RED : 0x8410, 1, 2);
		previous = key;
	}
}

struct DisplayField {
	uint8_t x, y, w, h;
	int16_t limit;
	uint8_t decimals, textSize, width;
	uint16_t normColor;
};

const DisplayField fields[13] PROGMEM = {
    {58, 140, 36, 16, -1, 0, 2, 3, GC9A01A_WHITE},   // AIR   = 0
    {175, 100, 35, 18, -1, 2, 2, 3, GC9A01A_WHITE},  // MAP   = 1
    {70, 80, 35, 18, 89, 0, 2, 3, GC9A01A_WHITE},    // TPS   = 2
    {175, 80, 47, 18, -1, 1, 2, 3, GC9A01A_WHITE},   // BATT  = 3
    {70, 120, 50, 18, -1, 2, 2, 3, GC9A01A_WHITE},   // O22   = 4
    {139, 30, 36, 16, -1, 0, 2, 3, GC9A01A_YELLOW},  // SPEED = 5
    {70, 160, 36, 16, 89, 0, 2, 3, GC9A01A_WHITE},   // TRANS = 6
    {175, 120, 35, 18, -1, 1, 2, 3, GC9A01A_WHITE},  // OIL = 7 (olej)
    {122, 50, 72, 22, 5499, 0, 3, 4, GC9A01A_GREEN}, // RPM   = 8
    {173, 140, 36, 16, 99, 0, 2, 3, GC9A01A_WHITE},  // WATER = 9
    {70, 100, 50, 18, -1, 2, 2, 3, GC9A01A_WHITE},   // O2    = 10

    {112, 225, 30, 14, -1, 0, 1, 4, GC9A01A_YELLOW}, // 11: RX channel samples/min
    {125, 200, 47, 18, -1, 1, 2, 3, GC9A01A_WHITE}   // 12: bateria w drugim widoku
};

void display(uint8_t id, int16_t value) {
	if (id >= 13)
		return;
	if (value == -32768)
		value = 0;

	if (id <= 10 || id == 12) {
		uint8_t channel = id == 12 ? (uint8_t)BATT : id;
		if (value == lastDisplayed[channel])
			return;
		lastDisplayed[channel] = value;
	}

	DisplayField field;
	memcpy_P(&field, &fields[id], sizeof(field));
	if (!guiMode && (id == AIR || id == WATER || id == TRANS)) {
		field.x = 155;
		field.y = id == AIR ? 140 : id == WATER ? 160 : 180;
	}

	tft.fillRect(field.x, field.y, field.w, field.h, GC9A01A_BLACK);
	tft.setTextSize(field.textSize);
	if (field.limit != -1 && value > field.limit) {
		tft.setTextColor(GC9A01A_RED);
	} else {
		tft.setTextColor(field.normColor);
	}
	tft.setCursor(field.x, field.y);
	char buf[8];
	switch (field.decimals) {
	case 1:
		sprintf(buf, "%d.%d", value / 10, value % 10);
		break;
	case 2:
		sprintf(buf, "%d.%d", (value + 5) / 100, ((value + 5) % 100) / 10);
		break;
	default:
		switch (field.width) {
		case 4:
			sprintf(buf, "%4d", value);
			break;
		case 2:
			sprintf(buf, "%2d", value);
			break;
		default:
			sprintf(buf, "%3d", value);
			break;
		}
		break;
	}
	tft.print(buf);
}

void drawGUI() {

	for (uint8_t i = 0; i <= 10; i++) {
		lastDisplayed[i] = -32768;
	}
	tft.fillScreen(GC9A01A_BLACK);
	tft.setTextSize(2);
	tft.setTextColor(GC9A01A_WHITE);
	tft.setCursor(67, 30);
	tft.println(F("Speed:"));
	tft.setTextSize(3);
	tft.setCursor(50, 50);
	tft.println(F("RPM:"));
	lastKat = KAT;
	obrotomierz(lastKat, lastKat + 300, 0x39E7);
	tft.setTextSize(2);
	tft.setCursor(10, 80);
	tft.println(F("TPS:"));
	tft.setCursor(70, 80);
	tft.print(F("  0"));
	tft.println(F("%"));
	tft.setCursor(125, 80);
	tft.println(F("BAT:"));
	tft.setCursor(175, 80);
	tft.print(F(" 0.0"));
	tft.println(F("V"));
	tft.setCursor(10, 100);
	tft.println(F("AFR1:"));
	tft.setCursor(125, 100);
	tft.println(F("MAP:"));
	tft.setCursor(175, 100);
	tft.print(F("0.0"));
	tft.setTextSize(1);
	tft.setCursor(215, 107);
	tft.println(F("bar"));
	tft.setTextSize(2);
	tft.setCursor(10, 120);
	tft.println(F("AFR2:"));
	tft.setCursor(125, 120);
	tft.println(F("OIL:"));
	tft.setCursor(175, 120);
	tft.print(F("0.0"));
	tft.setTextSize(1);
	tft.setCursor(215, 127);
	tft.println(F("bar"));
	tft.setTextSize(2);
	tft.setCursor(10, 140);
	tft.print(F("IAT:"));
	tft.setCursor(94, 140);
	tft.write(0xF7);
	tft.print(F("C"));
	tft.setCursor(125, 140);
	tft.print(F("ECT:"));
	tft.setCursor(209, 140);
	tft.write(0xF7);
	tft.print(F("C"));
	tft.setCursor(22, 160);
	tft.print(F("TFT:"));
	tft.setCursor(106, 160);
	tft.write(0xF7);
	tft.print(F("C"));
	tft.setCursor(141, 160);
	tft.print(F("SA:"));
	tft.setCursor(209, 160);
	tft.write(0xF7);
	drawFirstExtra(true);

	tft.setTextSize(1);
	tft.setTextColor(GC9A01A_YELLOW);
	tft.setCursor(90, 225);
	tft.print(F("RX"));
	wdt_reset();
}

void drawGUI2() {

	for (uint8_t i = 0; i <= 10; i++) {
		lastDisplayed[i] = -32768;
	}

	tft.fillScreen(GC9A01A_BLACK);
	tft.setTextSize(2);
	tft.setTextColor(GC9A01A_CYAN);
	tft.setCursor(60, 25);
	tft.println(F("Speed:"));
	lastKat = KAT;
	obrotomierz(lastKat, lastKat + 300, 0x39E7);
	tft.setTextSize(2);
	tft.setCursor(30, 70);
	tft.println(F("RPM:"));
	tft.setCursor(25, 140);
	tft.println(F("Air Temp:"));
	tft.setCursor(155, 140);
	tft.print(F("  0"));
	tft.write(0xF7);
	tft.println(F("C"));
	tft.setCursor(25, 160);
	tft.println(F("Water Temp:"));
	tft.setCursor(155, 160);
	tft.print(F("  0"));
	tft.write(0xF7);
	tft.println(F("C"));
	tft.setCursor(25, 180);
	tft.println(F("Trans Temp:"));
	tft.setCursor(155, 180);
	tft.print(F("  0"));
	tft.write(0xF7);
	tft.println(F("C"));
	tft.setCursor(72, 200);
	tft.println(F("Bat:"));
	tft.setTextSize(1);
	tft.setTextColor(GC9A01A_YELLOW);
	tft.setCursor(90, 225);
	tft.print(F("RX"));
	wdt_reset();
}

void drawClockGUI() {
	tft2.fillScreen(GC9A01A_BLACK);
	lastKat2 = KAT;
	obrotomierz(lastKat2, lastKat2 + 300, 0x39E7, false);
	lastHour = -1;
	lastMinute = -1;
	lastDay = -1;
	lastMonth = -1;
	lastYear = -1;
	lastWeekday = -1;
	lastDisplayed[TEMP_AC] = -999;
	lastDisplayed[HUM] = -999;
	lastDisplayed[TEMP_OUT] = -999;
	lastDisplayed[TEMP_IN] = -999;
	if (clockMode)
		drawClimateIndicators(true);
	wdt_reset();
}
