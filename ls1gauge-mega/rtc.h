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

void generateToken(char *buffer, uint8_t length) {
	for (uint8_t i = 0; i < length; i++) {
		buffer[i] = charset[random(0, sizeof(charset) - 1)];
	}
	buffer[length] = '\0';
}

void saveTokenToEEPROM(const char *buffer) {
	for (uint8_t i = 0; i < TOKEN_LENGTH; i++) {
		EEPROM.write(EEPROM_TOKEN_ADDR + i, buffer[i]);
	}
	EEPROM.write(EEPROM_TOKEN_ADDR + TOKEN_LENGTH, '\0'); // null terminator
}

void readTokenFromEEPROM(char *buffer) {
	for (uint8_t i = 0; i <= TOKEN_LENGTH; i++) {
		buffer[i] = EEPROM.read(EEPROM_TOKEN_ADDR + i);
	}
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
	tft2.fillRect(61, y, 120, 16, GC9A01A_BLACK);
	tft2.setTextColor(GC9A01A_YELLOW);
	tft2.setTextSize(2);
	tft2.setCursor(61, y);
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

void drawTFT2() {
	if (guiStarted) {
		int16_t kat2 = KAT + map(now.second(), 0, 60, 0, 300);

		if (kat2 > lastKat2) {
			obrotomierz(lastKat2, kat2, GC9A01A_WHITE, false);
		} else if (kat2 < lastKat2) {
			obrotomierz(lastKat2, kat2, 0x39E7, false); // np. tło, szaro-niebieski
		}

		lastKat2 = kat2;

		if (clockMode) {

			tft2.setTextColor(GC9A01A_CYAN);
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
				tft2.setTextColor(GC9A01A_WHITE);
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

			drawClockTemperature(TEMP_IN, F("In: "), 145);
			drawClockTemperature(TEMP_OUT, F("Out:"), 165);
			drawClockTemperature(TEMP_AC, F("AC: "), 185);
			if (channels[HUM].value != lastDisplayed[HUM]) {
				tft2.fillRect(73, 205, 96, 16, GC9A01A_BLACK);
				tft2.setFont();
				tft2.setTextSize(2);
				tft2.setTextColor(GC9A01A_YELLOW);
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
			tft2.setTextColor(GC9A01A_WHITE);
			if (clock) {
				tft2.setCursor(113, 100);
				tft2.print(F(":"));
			} else {
				tft2.fillRect(119, 105, 12, 25, GC9A01A_BLACK);
			}
			clock = !clock;
			tft2.setTextSize(2);
			tft2.setTextColor(GC9A01A_CYAN);
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
			tft2.setTextColor(GC9A01A_YELLOW);
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
