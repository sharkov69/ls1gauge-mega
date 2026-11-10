#include <avr/wdt.h>
#include "modem_debug_config.h"

#define LOGO
bool demo;
bool menuActive = false;
bool guiMode;
bool clockMode;
bool darkMode = false;
uint16_t displayColor(uint16_t color);
uint8_t pause;
// #define TXRX
// #define DEBUGVPW
// #define DEBUG
#ifdef DEBUG
#define DBG(x) Serial.print(x)
#define DBGL(x) Serial.println(x)
#define DBG2(x, y) Serial.print(x, y)
#define DBGL2(x, y) Serial.println(x, y)
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

#define TFT_DC 11
#define TFT_CS 9
#define TFT_RST 7
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

#define TFT2_DC 17
#define TFT2_CS 15
#define TFT2_RST 13
Adafruit_GC9A01A tft2(TFT2_CS, TFT2_DC, TFT2_RST);

uint16_t stats[ILE] = {0};
uint8_t current{};
uint16_t counter{};
uint16_t lastStats{};

constexpr int16_t KAT = 210;
int16_t lastKat = KAT;
int16_t lastKat2 = KAT;

uint16_t lastUpdate = 0;
uint16_t refreshInterval = 200;
uint16_t refreshInterval2 = 3000;
uint16_t guiStartTime;
bool guiStarted = false;
bool clock = true;

void (*resetFunc)(void) = 0;

#include "font.h"
#include "temp.h"
#include "cmd.h"
#include "obrotomierz.h"
#ifdef LOGO
#include "images.h"
#endif
void drawClimateIndicators(bool force = false);
#include "rtc.h"
#include "sd.h"
#include "drawbmp.h"
#include "modem.h"
#include "csv_upload_queue.h"
#include "csv_provision.h"
#include "modem-csv.h"

#include "display.h"
#include "menu.h"
#include "console.h"

#if DEBUG_MODEM
ISR(WDT_vect) {
	debugWatchdogExpired();
	wdt_enable(WDTO_15MS);
	for (;;) {} // Fatal timeout: preserve the breadcrumb until hardware resets, no I/O.
}
#endif
void debugSetupStage(uint8_t stage) {
	debugStage(stage);
	modemDebugNumber('B', 'S', stage);
}

void setup() {
#if DEBUG_MODEM
	const uint8_t modemDebugResetFlags = MCUSR; // Bootloader may already have cleared this.
#endif
	wdt_disable();

	Serial.begin(115200);
	Serial1.begin(115200);
	modemDebugLoad();
	debugBootBegin(modemDebugResetFlags);
	debugBootReport();
	modemDebugNumber('B', 'O', modemDebugResetFlags);
	modemPowerKey(false);

	modemInitStart();

	debugSetupStage(2);
	sensors.begin();

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
	debugSetupStage(3);
	// A stalled RTC bus must not stop VPW, logging and modem servicing.
	Wire.setWireTimeout(25000, true);
	if (!rtc.begin()) {
		DBGL(F("RTC nie działa!"));
		while (1)
			delay(10);
	}
	rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
	rtc.disable32K();
	sprawdzLetni();
	pinMode(sqwPin, INPUT);
	attachInterrupt(digitalPinToInterrupt(sqwPin), sqwISR, FALLING);
	FsDateTime::setCallback(sdDateTime);
	debugSetupStage(4);
	if (!SD.begin(sdCS, SPI_FULL_SPEED)) {
		DBGL(F("Blad SD!"));
	}
	debugSetupStage(5);
	loadBMP();
	debugSetupStage(6);
	csvLoadProvisioning();
	demo = EEPROM.read(EEPROM_DEMO_ADDR) == 1;
	darkMode = EEPROM.read(EEPROM_DARK_ADDR) == 1;
	guiMode = EEPROM.read(EEPROM_GUI_ADDR);
	clockMode = EEPROM.read(EEPROM_CLOCK_ADDR);
	pause = EEPROM.read(EEPROM_PAUSE_ADDR);
	if (pause < 25 || pause > 125) {
		pause = 75;
		EEPROM.update(EEPROM_PAUSE_ADDR, pause);
	}

	debugSetupStage(7);
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
	debugSetupStage(8);
	vpw.init(RX, TX);

	lastStats = (uint16_t)(millis() & 0xFFFF);
	guiStartTime = (uint16_t)(millis() & 0xFFFF);
	netLastCheck = millis();
#ifdef DEBUG
	DBGL(F("Setup done."));
#endif
	wdt_enable(WDTO_4S);
#if DEBUG_MODEM
	WDTCSR |= _BV(WDIE);
#endif
	debugSetupStage(9);
}

void loop(void) {

	const uint32_t loopStarted = micros();
	debugStage(10);
	sampleFreeRam();
	updateScreenPower();
	czas = (uint16_t)(millis() & 0xFFFF);
	debugStage(11);
	read();
	// Capture short encoder pulses during LTE; draw/process only outside its fast path.
	pollMenuButtons(czas);
	if (modemNeedsFastService()) {
		debugStage(12);
		modemInitTick();
		wdt_reset();
		const uint32_t loopUs = micros() - loopStarted;
		if (loopUs > maxLoopUs)
			maxLoopUs = loopUs;
		return; // GPS response/timeout only; VPW reception continues on every pass.
	}
	debugStage(13);
	send();
	debugStage(14);

	updateStartupScreen(czas);
	updateDemo(czas);
	if (guiStarted && screenPowerReady() && (uint16_t)(czas - lastUpdate) >= refreshInterval) {
		lastUpdate = czas;

		// Presentation limits must not overwrite the measurements used by SD/LTE.
		const int16_t displayRpm = ((constrain(channels[RPM].value, 0, 6000) + 12) / 25) * 25;
		if (guiMode) {
			display(SPEED, channels[SPEED].value);
			display(RPM, displayRpm);
			display(TPS, channels[TPS].value);
			display(BATT, channels[BATT].value);
			display(O2, channels[O2].value);
			display(MAP, channels[MAP].value);
			display(O22, channels[O22].value);
			display(OIL, channels[OIL].value);
			drawFirstExtra();
		} else {

			char buf[8];
			int16_t x1, y1;
			uint16_t w, h;
			const int16_t displaySpeed = max((int16_t)0, channels[SPEED].value);
			if (displaySpeed != lastDisplayed[SPEED]) {
				tft.setTextSize(3);
				tft.setTextColor(displayColor(GC9A01A_WHITE));
				tft.fillRect(90, 50, 80, 22, GC9A01A_BLACK);
				snprintf(buf, sizeof(buf), "%d", displaySpeed);
				tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
				tft.setCursor(119 - w / 2, 50);
				tft.print(buf);
				lastDisplayed[SPEED] = displaySpeed;
			}

			if (displayRpm != lastDisplayed[RPM]) {
				if (displayRpm > 4499) {
					tft.setTextColor(displayColor(GC9A01A_RED));
				} else {
					tft.setTextColor(displayColor(GC9A01A_GREEN));
				}

				tft.setTextSize(1);
				tft.setFont(&Neue_Leights28pt7b);

				tft.fillRect(50, 90, 140, 45, GC9A01A_BLACK);
				snprintf(buf, sizeof(buf), "%d", displayRpm);
				tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
				tft.setCursor(119 - w / 2, 130);
				tft.print(buf);
				tft.setFont();
				lastDisplayed[RPM] = displayRpm;
			}
			display(12, channels[BATT].value);
		}

		display(AIR, channels[AIR].value);
		display(WATER, channels[WATER].value);
		display(TRANS, channels[TRANS].value);
		if (clockMode)
			drawClimateIndicators();

		int16_t zielony = KAT + map(3500, 0, 6000, 0, 300);
		int16_t zolty = KAT + map(4500, 0, 6000, 0, 300);
		int16_t kat = KAT + map(displayRpm, 0, 6000, 0, 300);
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
		// #ifdef DEBUG
		// display(142, 225, 35, 18, getRecentErrorCount(), -1, 0, 1, 2, GC9A01A_BLUE);
		// #endif
	}
	if ((uint16_t)(czas - lastStats) >= refreshInterval2) {
		lastStats = czas;

		current++;
		if (current >= ILE)
			current = 0;

		// wpisz nową wartość do historii
		stats[current] = counter;
		counter = 0;
		// policz sumę z ostatnich 60 sek
		uint16_t sumrx = 0;
		for (uint8_t i = 0; i < ILE; i++) {
			sumrx += stats[i];
		}
		if (guiStarted) {
			display(11, sumrx);
		}
	}

	debugStage(15);
	handleMenuButtons(czas);

	debugStage(16);
	if (bmpDrawing) {
		continueDrawBMP();
	}
	debugStage(17);
	if ((uint16_t)(czas - lastDHTread) >= SENSORS_INTERVAL) {
		lastDHTread = czas;

		float t = dht.readTemperature();
		float h = dht.readHumidity();

		if (!isnan(t) && !isnan(h)) {
			channels[TEMP_AC].set((int16_t)(t >= 0 ? t * 10.0 + 0.5 : t * 10.0 - 0.5));
			channels[HUM].set((int16_t)(h >= 0 ? h * 10.0 + 0.5 : h * 10.0 - 0.5));

		} else {
		}
		const int32_t outsideRaw = sensors.getTemp(tempOut);
		const int32_t insideRaw = sensors.getTemp(tempIn);
		// Dallas returns a raw error code on disconnect; retain the last valid sample.
		if (outsideRaw > DEVICE_DISCONNECTED_RAW)
			channels[TEMP_OUT].set((int16_t)((outsideRaw * 10 + 64) >> 7));
		if (insideRaw > DEVICE_DISCONNECTED_RAW)
			channels[TEMP_IN].set((int16_t)((insideRaw * 10 + 64) >> 7));
		sensors.requestTemperatures(); // rozpocznij kolejny pomiar
		if (modemInitDone())
			getGPSInfo();
	}

	if (tick) {
		tick = false;
		debugStage(18);
		now = rtcNow();
		debugStage(19);

		if (guiStarted && screenPowerReady()) {
			tft.setTextSize(2);
			tft.setTextColor(displayColor(GC9A01A_RED));
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
		}
		if (!menuActive && screenPowerReady()) {
			drawTFT2();
		}
	}
	debugStage(20);
	read();
	debugStage(21);
	updateLogToSD();
	debugStage(22);
	if (csvProvisioned && csvTlsQualified && modemOwner == OWNER_IDLE && !at.busy && !at.done)
		csvQueueTick();
	debugStage(23);
	csvUploadScheduler();
	debugStage(24);
	handleConsole();
	debugStage(25);
	modemInitTick();
	debugStage(26);
	static uint32_t lastKick = 0;
	uint32_t now = millis();
	if (now - lastKick >= 2000) { // nie musisz karmić co iterację
		wdt_reset();
		lastKick = now;
	}

	const uint32_t loopUs = micros() - loopStarted;
	if (loopUs > maxLoopUs)
		maxLoopUs = loopUs;
}
