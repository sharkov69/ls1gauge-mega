#include <RTClib.h>
#include <Wire.h>
RTC_DS3231 rtc;
const byte sqwPin = 2;
volatile bool tick = false;
DateTime now;
bool rtcTimeValid = false;
uint16_t rtcReadErrors = 0;
uint32_t rtcSeconds = 0, rtcMillis = 0;

DateTime rtcCachedNow() {
	if (!rtcTimeValid)
		return DateTime(2000, 1, 1);
	const uint32_t elapsed = (uint32_t)(millis() - rtcMillis) / 1000;
	rtcSeconds += elapsed;
	rtcMillis += elapsed * 1000;
	return DateTime(rtcSeconds);
}

bool readRtcTime(DateTime &value) {
	Wire.beginTransmission(0x68);
	Wire.write((uint8_t)0);
	if (Wire.endTransmission(false) != 0 ||
	    Wire.requestFrom((uint8_t)0x68, (uint8_t)7) != 7)
		return false;
	uint8_t data[7];
	for (uint8_t i = 0; i < sizeof(data); ++i)
		data[i] = Wire.read();
	// RTClib normally uses 24-hour mode; also accept a valid 12-hour RTC setting.
	const bool twelveHour = data[2] & 0x40, pm = data[2] & 0x20;
	data[2] &= twelveHour ? 0x1f : 0x3f;
	if (data[5] & 0x80)
		return false; // DateTime and the CSV filename only support years through 2099.
	for (uint8_t i = 0; i < sizeof(data); ++i) {
		if ((data[i] & 0x0f) > 9 || (data[i] >> 4) > 9)
			return false;
		data[i] = (data[i] >> 4) * 10 + (data[i] & 0x0f);
	}
	if (twelveHour) {
		if (data[2] < 1 || data[2] > 12)
			return false;
		data[2] = data[2] % 12 + (pm ? 12 : 0);
	}
	if (data[0] > 59 || data[1] > 59 || data[2] > 23 || data[3] < 1 || data[3] > 7 ||
	    data[4] < 1 || data[4] > 31 || data[5] < 1 || data[5] > 12 || data[6] < 20)
		return false;
	value = DateTime(2000 + data[6], data[5], data[4], data[2], data[1], data[0]);
	return value.isValid();
}

DateTime rtcNow() {
	const DateTime cached = rtcCachedNow();
	DateTime value;
	if (readRtcTime(value)) {
		const int32_t difference = (int32_t)(value.unixtime() - cached.unixtime());
		if (!rtcTimeValid || (difference >= -2 && difference <= 2)) {
			rtcSeconds = value.unixtime();
			rtcMillis = millis();
			rtcTimeValid = true;
			return value;
		}
	}
	++rtcReadErrors;
	return cached;
}

void setRtcTime(const DateTime &value) {
	rtc.adjust(value);
	rtcSeconds = value.unixtime();
	rtcMillis = millis();
	rtcTimeValid = true;
}

#include <EEPROM.h>
#define EEPROM_DST_FLAG_ADDR 0
#define EEPROM_DEMO_ADDR 14
#define EEPROM_GUI_ADDR 15
#define EEPROM_CLOCK_ADDR 16
#define EEPROM_PAUSE_ADDR 17
#define EEPROM_DARK_ADDR 19

bool parseModemClock(const char *line, uint32_t &utc) {
	if (strncmp_P(line, PSTR("+CCLK: \""), 8) != 0)
		return false;
	const char *p = line + 8;
	if (strlen(p) != 21 || p[20] != '"')
		return false;
	uint8_t values[7];
	for (uint8_t i = 0; i < 7; ++i) {
		const uint8_t pos = i * 3;
		if (p[pos] < '0' || p[pos] > '9' || p[pos + 1] < '0' || p[pos + 1] > '9')
			return false;
		values[i] = (p[pos] - '0') * 10 + p[pos + 1] - '0';
	}
	if (p[2] != '/' || p[5] != '/' || p[8] != ',' || p[11] != ':' || p[14] != ':' ||
	    (p[17] != '+' && p[17] != '-') || values[0] < 20 || values[1] < 1 ||
	    values[1] > 12 || values[2] < 1 || values[2] > 31 || values[3] > 23 ||
	    values[4] > 59 || values[5] > 59 || values[6] > 96)
		return false;
	const DateTime value(2000 + values[0], values[1], values[2], values[3], values[4], values[5]);
	if (!value.isValid())
		return false;
	const int32_t offset = (int32_t)values[6] * 900 * (p[17] == '+' ? 1 : -1);
	utc = value.unixtime() - offset;
	return true;
}

bool warsawSummerTime(const DateTime &utc) {
	const uint8_t month = utc.month();
	if (month < 3 || month > 10)
		return false;
	if (month > 3 && month < 10)
		return true;
	const uint8_t sunday = 31 - DateTime(utc.year(), month, 31).dayOfTheWeek();
	const bool afterChange = utc.day() > sunday || (utc.day() == sunday && utc.hour() >= 1);
	return month == 3 ? afterChange : !afterChange;
}

bool syncRtcUtc(uint32_t utcSeconds) {
	const bool summer = warsawSummerTime(DateTime(utcSeconds));
	const DateTime local(utcSeconds + (summer ? 7200UL : 3600UL));
	const int32_t difference = (int32_t)(local.unixtime() - rtcNow().unixtime());
	EEPROM.update(EEPROM_DST_FLAG_ADDR, summer ? 1 : 0);
	if (rtcTimeValid && difference >= -30 && difference <= 30)
		return false;
	setRtcTime(local);
	now = local;
	tick = true;
	return true;
}


bool czyLetni(int day, int month, int weekday) {
	if (month < 3 || month > 10)
		return false; // styczeń–luty, listopad–grudzień → zimowy
	if (month > 3 && month < 10)
		return true; // kwiecień–wrzesień → letni

	int lastSunday = 31 - ((weekday + 31 - day) % 7); // dzień ostatniej niedzieli w danym miesiącu

	if (month == 3)
		return (day >= lastSunday); // w marcu — po ostatniej niedzieli → letni
	if (month == 10)
		return (day < lastSunday); // w październiku — do ostatniej niedzieli → letni

	return false;
}

void sprawdzLetni() {
	DateTime now = rtcNow();
	if (!rtcTimeValid)
		return;
	uint8_t savedDST = EEPROM.read(EEPROM_DST_FLAG_ADDR);
	const DateTime utc(now.unixtime() - (savedDST == 1 ? 7200UL : 3600UL));
	bool nowDST = warsawSummerTime(utc);

	if (nowDST && savedDST == 0) {
		// był zapisany zimowy, teraz jest letni — dodaj godzinę
		now = now + TimeSpan(3600);
		setRtcTime(now);
		EEPROM.update(EEPROM_DST_FLAG_ADDR, 1);
	} else if (!nowDST && savedDST == 1) {
		// był zapisany letni, teraz jest zimowy — odejmij godzinę
		now = now - TimeSpan(3600);
		setRtcTime(now);
		EEPROM.update(EEPROM_DST_FLAG_ADDR, 0);
	}
}
const char *dni[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int lastHour = -1, lastMinute = -1, lastDay = -1, lastMonth = -1, lastYear = -1, lastWeekday = -1;

void sqwISR() {
	tick = true;
}

void drawClockTemperature(uint8_t id, const __FlashStringHelper *label, uint8_t y) {
	if (channels[id].value == lastDisplayed[id])
		return;
	const int16_t v = channels[id].value;
	char value[8];
	if (v == -32768)
		strcpy(value, "--.-");
	else {
		const uint16_t magnitude = v < 0 ? -(int32_t)v : v;
		snprintf(value, sizeof(value), "%s%u.%u", v < 0 ? "-" : "", magnitude / 10, magnitude % 10);
	}
	const uint8_t length = strlen(value), sx = length > 4 ? 1 : 2;
	tft2.setFont();
	const uint8_t labelX = id == TEMP_AC ? 49 : id == TEMP_IN ? 73 : 61;
	tft2.fillRect(labelX, y, 181 - labelX, 16, GC9A01A_BLACK);
	tft2.setTextColor(displayColor(GC9A01A_YELLOW));
	tft2.setTextSize(2);
	tft2.setCursor(labelX, y);
	tft2.print(label);
	tft2.setTextSize(sx, 2);
	tft2.setCursor(157 - length * 6 * sx, y);
	tft2.print(value);
	tft2.setTextSize(2);
	tft2.setCursor(157, y);
	tft2.write(0xF7);
	tft2.print('C');
	lastDisplayed[id] = v;
}

extern uint16_t modemRestartCount;

void drawModemRestartCount() {
	char count[6];
	snprintf(count, sizeof(count), "%u", modemRestartCount);
	const uint8_t width = 17 + strlen(count) * 6;
	const uint8_t x = 120 - width / 2;
	const uint16_t color = displayColor(GC9A01A_DARKGREY);
	tft2.fillRect(94, 222, 52, 12, GC9A01A_BLACK);
	// Two small radio waves and a dot, matching the RX text scale.
	tft2.drawLine(x, 226, x + 3, 223, color);
	tft2.drawFastHLine(x + 3, 223, 7, color);
	tft2.drawLine(x + 9, 223, x + 12, 226, color);
	tft2.drawLine(x + 3, 228, x + 5, 226, color);
	tft2.drawFastHLine(x + 5, 226, 3, color);
	tft2.drawLine(x + 7, 226, x + 9, 228, color);
	tft2.drawFastHLine(x + 6, 231, 1, color);
	tft2.setFont();
	tft2.setTextSize(1);
	tft2.setTextColor(color);
	tft2.setCursor(x + 17, 225);
	tft2.print(count);
}

void drawTFT2() {
	if (menuActive)
		return;
	if (guiStarted) {
		drawModemRestartCount();
		int16_t kat2 = KAT + map(now.second(), 0, 60, 0, 300);

		if (kat2 > lastKat2) {
			obrotomierz(lastKat2, kat2, GC9A01A_WHITE, false);
		} else if (kat2 < lastKat2) {
			obrotomierz(lastKat2, kat2, 0x39E7, false); // np. tło, szaro-niebieski
		}

		lastKat2 = kat2;

		if (clockMode) {

			tft2.setTextColor(displayColor(GC9A01A_CYAN));
			tft2.setTextSize(1);
			tft2.setFont(&Neue_Leights28pt7b);
			if (now.minute() != lastMinute) {
				tft2.fillRect(30, 93, 71, 45, GC9A01A_BLACK);
				char buf[3];
				snprintf(buf, sizeof(buf), "%02u", now.minute());
				int16_t x1, y1;
				uint16_t w, h;
				tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
				tft2.setCursor(90 - w, 133);
				tft2.print(buf);
				lastMinute = now.minute();
			}
			if (now.hour() != lastHour) {
				tft2.fillRect(33, 42, 67, 45, GC9A01A_BLACK);
				char buf[3];
				snprintf(buf, sizeof(buf), "%02u", now.hour());
				int16_t x1, y1;
				uint16_t w, h;
				tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
				tft2.setCursor(90 - w, 82);
				tft2.print(buf);
				lastHour = now.hour();
			}
			tft2.setFont();
			if (now.day() != lastDay || now.month() != lastMonth || now.year() != lastYear ||
			    now.dayOfTheWeek() != lastWeekday) {
				tft2.setTextColor(displayColor(GC9A01A_WHITE));
				tft2.setTextSize(2);
				tft2.fillRect(105, 50, 108, 80, GC9A01A_BLACK);
				tft2.setCursor(105, 50);
				tft2.print(dni[now.dayOfTheWeek()]);
				tft2.setTextSize(3);
				tft2.setCursor(105, 75);
				tft2.print(now.day());
				tft2.print(F("."));
				if (now.month() < 10)
					tft2.print(F("0"));
				tft2.print(now.month());
				tft2.setCursor(105, 105);
				tft2.print(now.year());

				lastDay = now.day();
				lastMonth = now.month();
				lastYear = now.year();
				lastWeekday = now.dayOfTheWeek();
			}

			drawClockTemperature(TEMP_IN, F("In:"), 145);
			drawClockTemperature(TEMP_OUT, F("Out:"), 165);
			drawClockTemperature(TEMP_AC, F("Vent:"), 185);
			if (channels[HUM].value != lastDisplayed[HUM]) {
				tft2.fillRect(73, 205, 96, 16, GC9A01A_BLACK);
				tft2.setFont();
				tft2.setTextSize(2);
				tft2.setTextColor(displayColor(GC9A01A_YELLOW));
				char value[5];
				if (channels[HUM].value == -32768)
					strcpy(value, "--");
				else
					snprintf(value, sizeof(value), "%d",
					         constrain((channels[HUM].value + 5) / 10, 0, 100));
				tft2.setCursor(120 - ((5 + strlen(value)) * 12 - 2) / 2, 205);
				tft2.print(F("Hum:"));
				tft2.print(value);
				tft2.print('%');
				lastDisplayed[HUM] = channels[HUM].value;
			}
			drawClimateIndicators();
		} else {
			tft2.setTextSize(5);
			tft2.setTextColor(displayColor(GC9A01A_WHITE));
			if (clock) {
				tft2.setCursor(113, 100);
				tft2.print(F(":"));
			} else {
				tft2.fillRect(119, 105, 12, 25, GC9A01A_BLACK);
			}
			clock = !clock;
			tft2.setTextSize(2);
			tft2.setTextColor(displayColor(GC9A01A_CYAN));
			tft2.setFont(&Neue_Leights28pt7b);
			if (now.minute() != lastMinute) {
				tft2.fillRect(63, 130, 126, 85, GC9A01A_BLACK);
				char buf[3];
				snprintf(buf, sizeof(buf), "%02u", now.minute());
				int16_t x1, y1;
				uint16_t w, h;
				tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
				tft2.setCursor(120 - w / 2, 210);
				tft2.print(buf);
				lastMinute = now.minute();
			}
			if (now.hour() != lastHour) {
				tft2.fillRect(63, 25, 126, 85, GC9A01A_BLACK);
				char buf[3];
				snprintf(buf, sizeof(buf), "%02u", now.hour());
				int16_t x1, y1;
				uint16_t w, h;
				tft2.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
				tft2.setCursor(120 - w / 2, 105);
				tft2.print(buf);
				lastHour = now.hour();
			}
			tft2.setFont();
			tft2.setTextSize(2);
			tft2.setTextColor(displayColor(GC9A01A_YELLOW));
			// In Temp
			if (channels[TEMP_IN].value != lastDisplayed[TEMP_IN]) {
				channels[TEMP_IN].value = constrain(channels[TEMP_IN].value, -999, 999);
				tft2.fillRect(12, 75, 40, 18, GC9A01A_BLACK);
				tft2.setCursor(12, 75);
				tft2.print(F("In:"));
				uint8_t minus = 0;
				if (channels[TEMP_IN].value / 10.0 < 0) {
					minus = 10;
				} else {
					minus = 0;
				}
				tft2.fillRect(12 - minus, 95, 50, 18, GC9A01A_BLACK);
				tft2.setCursor(12 - minus, 95);
				tft2.print(channels[TEMP_IN].value / 10.0, 1);
				lastDisplayed[TEMP_IN] = channels[TEMP_IN].value;
			}
			// Out Temp
			if (channels[TEMP_OUT].value != lastDisplayed[TEMP_OUT]) {
				channels[TEMP_OUT].value = constrain(channels[TEMP_OUT].value, -999, 999);
				tft2.fillRect(12, 115, 45, 18, GC9A01A_BLACK);
				tft2.setCursor(12, 115);
				tft2.print(F("Out:"));
				uint8_t minus = 0;
				if (channels[TEMP_OUT].value < 0) {
					minus = 10;
				} else {
					minus = 0;
				}
				tft2.fillRect(12 - minus, 135, 50, 18, GC9A01A_BLACK);
				tft2.setCursor(12 - minus, 135);
				tft2.print(channels[TEMP_OUT].value / 10.0, 1);
				lastDisplayed[TEMP_OUT] = channels[TEMP_OUT].value;
			}

			// Vent Temp
			if (channels[TEMP_AC].value != lastDisplayed[TEMP_AC]) {
				channels[TEMP_AC].value = constrain(channels[TEMP_AC].value, -999, 999);
				tft2.fillRect(187, 75, 35, 18, GC9A01A_BLACK);
				tft2.setCursor(187, 75);
				tft2.print(F("AC:"));
				tft2.fillRect(187, 95, 50, 18, GC9A01A_BLACK);
				tft2.setCursor(187, 95);
				tft2.print(channels[TEMP_AC].value / 10.0, 1);
				lastDisplayed[TEMP_AC] = channels[TEMP_AC].value;
			}
			// Humidity
			if (channels[HUM].value != lastDisplayed[HUM]) {
				channels[HUM].value = constrain(channels[HUM].value, 99, 999);
				tft2.fillRect(187, 115, 45, 18, GC9A01A_BLACK);
				tft2.setCursor(187, 115);
				tft2.print(F("Hum:"));
				tft2.fillRect(187, 135, 45, 18, GC9A01A_BLACK);
				tft2.setCursor(187, 135);
				if (channels[HUM].value > 990)
					channels[HUM].value = 990;
				tft2.print(channels[HUM].value / 10.0, 0);
				tft2.print(F("%"));
				lastDisplayed[HUM] = channels[HUM].value;
			}
		}
	}
}
