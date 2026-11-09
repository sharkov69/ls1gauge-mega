#include "Arduino.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cassert>
#ifndef TRACE_FILE
#define TRACE_FILE ".build/host/display-trace.txt"
#endif
#define private public
#include "../../libraries/J1850-VPW-WC/j1850vpw-wc.cpp"
#undef private
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
unsigned randomCalls = 0;
inline long random(long a, long) { ++randomCalls; return a; }
inline long random(long) { ++randomCalls; return 0; }
uint8_t pause = 75;
uint16_t counter = 0;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define constrain(v,a,b) ((v)<(a)?(a):((v)>(b)?(b):(v)))
#include "../../ls1gauge-mega/cmd.h"
using __FlashStringHelper = char;
constexpr int GC9A01A_BLACK=0, GC9A01A_WHITE=65535, GC9A01A_RED=63488,
              GC9A01A_GREEN=2016, GC9A01A_YELLOW=65504, GC9A01A_CYAN=2047,
              GC9A01A_DARKGREY=0x7bef, GC9A01A_BLUE=31,
              GC9A01A_SLPIN=16, GC9A01A_DISPOFF=40,
              GC9A01A_DISPON=41, GC9A01A_SLPOUT=17;
std::ostringstream trace;
class Adafruit_GFX {
    int id, sx=1, sy=1;
public:
    explicit Adafruit_GFX(int id): id(id) {}
    template<class... Args> void record(const char* operation, Args... args) {
        trace << id << ':' << operation;
        ((trace << ',' << args), ...);
        trace << '\n';
    }
    void setFont(const void* font=nullptr) { record("font", font != nullptr); }
    void setTextSize(int x) { sx=sy=x; record("size",x,x); }
    void setTextSize(int x,int y) { sx=x;sy=y;record("size",x,y); }
    void setTextColor(int color) { record("color",color); }
    void setCursor(int x,int y) { record("cursor",x,y); }
    void fillRect(int x,int y,int w,int h,int c) { record("fillRect",x,y,w,h,c); }
    void drawRect(int x,int y,int w,int h,int c) { record("drawRect",x,y,w,h,c); }
    void drawCircle(int x,int y,int r,int c) { record("circle",x,y,r,c); }
    void drawFastHLine(int x,int y,int w,int c) { record("hline",x,y,w,c); }
    void drawLine(int x,int y,int x2,int y2,int c) { record("line",x,y,x2,y2,c); }
    void fillScreen(int c) { record("fillScreen",c); }
    void startWrite() {}
    void endWrite() {}
    void writeCommand(uint8_t c) { record("command",unsigned(c)); }
    void fillRoundRect(int x,int y,int w,int h,int r,int c) { record("roundRect",x,y,w,h,r,c); }
    template<class T> void print(T value) { record("print",value); }
    void print(double value,int decimals) {
        std::ostringstream text; text << std::fixed << std::setprecision(decimals) << value;
        record("print",text.str());
    }
    template<class T> void println(T value) { print(value); record("newline"); }
    void write(int value) { record("write",value); }
    void getTextBounds(const char* text,int,int,int16_t* x,int16_t* y,uint16_t* w,uint16_t* h) {
        *x=*y=0; *w=(uint16_t)(strlen(text)*6*sx); *h=8*sy;
    }
} tft(1), tft2(2);
int Neue_Leights28pt7b;
#define clock traceClock
bool guiStarted=true, guiMode=true, clockMode=true, clock=true, menuActive=false;
constexpr int16_t KAT=210;
int16_t lastKat=KAT,lastKat2=KAT;
void wdt_reset() {}
long map(long v,long a,long b,long c,long d) { return (v-a)*(d-c)/(b-a)+c; }
void obrotomierz(int a,int b,int c,bool first=true) { trace << "arc:" << a << ',' << b << ',' << c << ',' << first << '\n'; }
#ifdef BEFORE_REFACTOR
#include "../../ls1gauge-mega/gauge_indicators.h"
#else
void drawClimateIndicators(bool force=false);
#endif
#include "../../ls1gauge-mega/rtc.h"
#include "../../ls1gauge-mega/display.h"
#ifdef UI_TEST
#define BUTTON1_PIN 27
#define BUTTON2_PIN 29
#define BUTTON3_PIN 31
#define LOGO
bool demo=false, bmpDrawing=false;
uint16_t guiStartTime=0;
void drawStartScreen() { tft2.record("logo"); }
void nextBMP() { bmpDrawing=true; }
void resetStub() {}
void (*resetFunc)()=resetStub;
#include "../../ls1gauge-mega/menu.h"
void buttons(uint16_t time, uint8_t mask) {
    testUs=(uint32_t)time*1000;
    pins[BUTTON1_PIN]=(mask&1)?LOW:HIGH;
    pins[BUTTON2_PIN]=(mask&2)?LOW:HIGH;
    pins[BUTTON3_PIN]=(mask&4)?LOW:HIGH;
    handleMenuButtons(time);
}
#endif
void measurement(int id,int16_t value) {
#ifdef BEFORE_REFACTOR
    cmd[id].wartosc=value;
#else
    channels[id].set(value);
#endif
}
void status(int index,uint16_t value) {
#ifdef BEFORE_REFACTOR
    extra.set(index,value,millis());
#else
    channels[BASE_CHANNEL_COUNT+index].setRaw(value,millis());
#endif
}
int main() {
#ifdef UI_TEST
    rtc.value = DateTime(2026,11,9,12,0,0);
    assert(rtcNow().unixtime() == rtc.value.unixtime());
    const uint32_t savedTime = rtc.value.unixtime();
    Wire.status = 2; testMillisOffset = 3500;
    assert(rtcNow().unixtime() == savedTime + 3);
    assert(rtcReadErrors == 1);
    Wire.status = 0; Wire.count = 3;
    assert(rtcNow().unixtime() == savedTime + 3);
    Wire.count = 7; Wire.badIndex = 0; Wire.badValue = 0x6a;
    assert(rtcNow().unixtime() == savedTime + 3);
    Wire.badIndex = -1; rtc.value = DateTime(2053,4,18);
    assert(rtcNow().unixtime() == savedTime + 3);
    rtc.value = DateTime(savedTime + 4);
    assert(rtcNow().unixtime() == savedTime + 4);
    const unsigned reads = Wire.requests;
    assert(rtcCachedNow().unixtime() == savedTime + 4);
    assert(Wire.requests == reads);
    rtcTimeValid = false; rtc.value = DateTime(2000,1,1);
    rtcNow(); assert(!rtcTimeValid);
    rtc.value = DateTime(2026,11,9,12,0,0);
    Wire.badIndex = 2; Wire.badValue = 0x72;
    DateTime reading; assert(readRtcTime(reading) && reading.hour() == 12);
    Wire.badValue = 0x52;
    assert(readRtcTime(reading) && reading.hour() == 0);
    Wire.badIndex = -1;
    testMillisOffset = 0xfffffff0u;
    setRtcTime(DateTime(savedTime));
    testMillisOffset = 0x00000ba8u;
    assert(rtcCachedNow().unixtime() == savedTime + 3);
    testMillisOffset = 0; rtcTimeValid = false;
    uint32_t utc;
    assert(parseModemClock("+CCLK: \"26/11/08,22:00:00+04\"", utc));
    assert(utc==DateTime(2026,11,8,21,0,0).unixtime());
    assert(parseModemClock("+CCLK: \"26/11/08,22:00:00-04\"", utc));
    assert(utc==DateTime(2026,11,8,23,0,0).unixtime());
    for (const char *bad : {"", "+CCLK: \"26/02/29,22:00:00+00\"",
         "+CCLK: \"26/11/08,24:00:00+00\"", "+CCLK: \"26/11/08,22:00:00+99\"",
         "+CCLK: \"26/11/08,22:00:00+00\"junk"})
        assert(!parseModemClock(bad,utc));
    assert(parseModemClock("+CCLK: \"24/02/29,22:00:00+00\"",utc));
    assert(!warsawSummerTime(DateTime(2026,3,29,0,59,59)));
    assert(warsawSummerTime(DateTime(2026,3,29,1,0,0)));
    assert(warsawSummerTime(DateTime(2026,10,25,0,59,59)));
    assert(!warsawSummerTime(DateTime(2026,10,25,1,0,0)));
    utc=DateTime(2026,11,8,21,0,0).unixtime();
    for (int delta : {-31,-30,0,30,31}) {
        rtc.value=DateTime(utc+3600+delta); rtc.adjustments=0; rtcTimeValid=false;
        assert(syncRtcUtc(utc)==(abs(delta)>30));
        assert(rtc.adjustments==(abs(delta)>30 ? 1u : 0u));
    }
    rtc.value=DateTime(2026,3,29,1,30,0); EEPROM.bytes[0]=0; rtcTimeValid=false;
    sprawdzLetni(); assert(rtc.now().hour()==1 && EEPROM.bytes[0]==0);
    rtc.value=DateTime(2026,3,29,2,30,0); rtcTimeValid=false;
    sprawdzLetni(); assert(rtc.now().hour()==3 && EEPROM.bytes[0]==1);
    rtc.value=DateTime(2026,10,25,2,30,0); EEPROM.bytes[0]=1; rtcTimeValid=false;
    sprawdzLetni(); assert(rtc.now().hour()==2 && EEPROM.bytes[0]==1);
    rtc.value=DateTime(2026,10,25,3,30,0); rtcTimeValid=false;
    sprawdzLetni(); assert(rtc.now().hour()==2 && EEPROM.bytes[0]==0);
    rtc.value=DateTime(); now=DateTime(); tick=false;
    menuActive=true;
    drawClimateIndicators(true);
    drawTFT2();
    if (!trace.str().empty()) { puts("FAIL: clock/icons overwrite menu"); return 1; }
    menuActive=false;
    guiStarted=false;
    drawClimateIndicators(true);
    drawTFT2();
    if (!trace.str().empty()) { puts("FAIL: clock/icons overwrite logo"); return 1; }
    guiStarted=true;
    measurement(TEMP_IN,-32768); measurement(TEMP_OUT,-32768);
    measurement(TEMP_AC,-32768); measurement(HUM,-32768);
    drawClockGUI();
    trace.str(""); trace.clear();
    drawTFT2();
    if (trace.str().find("size,1,2")!=std::string::npos) {
        puts("FAIL: missing sensors use condensed text"); return 1;
    }
    trace.str(""); trace.clear();
    measurement(TEMP_AC,-999);
    drawClockGUI();
    drawTFT2();
    if (trace.str().find("print,Vent:")==std::string::npos) {
        puts("FAIL: Vent row absent after redraw"); return 1;
    }
    #define UI_CHECK(c) do { if (!(c)) { printf("FAIL UI line %d: %s\n",__LINE__,#c); return 1; } } while(0)
    buttons(500,2);
    UI_CHECK(menuActive && !demo && menuPosition==0);
    buttons(1000,2);
    UI_CHECK(menuActive && !bmpDrawing); // Holding OK must not select Show pictures.
    buttons(1100,0);
    buttons(1500,4);
    UI_CHECK(menuPosition==1 && !demo);
    buttons(1600,0); buttons(2000,4);
    buttons(2100,0); buttons(2500,4);
    UI_CHECK(menuPosition==3 && !demo);
    buttons(2600,0); buttons(3000,2);
    UI_CHECK(demo && EEPROM.read(EEPROM_DEMO_ADDR)==1);
    const unsigned calls=randomCalls;
    updateDemo(3499); UI_CHECK(randomCalls==calls);
    updateDemo(3500); UI_CHECK(randomCalls>calls);
    const unsigned oneStep=randomCalls;
    for (uint16_t t=3501;t<4000;++t) updateDemo(t);
    UI_CHECK(randomCalls==oneStep); // Fast redraw cannot speed up simulation.
    buttons(3100,0); buttons(8001,0);
    UI_CHECK(!menuActive);
    buttons(8500,4); UI_CHECK(demo); // No demo shortcut outside the menu.
    buttons(8600,0); buttons(9000,2);
    buttons(9100,0); buttons(9500,2);
    UI_CHECK(!menuActive && !guiStarted && bmpDrawing);
    trace.str(""); trace.clear();
    updateStartupScreen(15000);
    UI_CHECK(!guiStarted && trace.str().empty());
    bmpDrawing=false;
    updateStartupScreen(19999); UI_CHECK(!guiStarted);
    updateStartupScreen(20000); UI_CHECK(guiStarted);
    UI_CHECK(trace.str().find("print,Vent:")!=std::string::npos);
    menuActive=true; menuPosition=3; handleMenuOption(3);
    menuStartTime=20500;
    UI_CHECK(!demo && EEPROM.read(EEPROM_DEMO_ADDR)==0);
    // Inactive menu entries and navigation have exactly five positions.
    UI_CHECK(MENU_OPTION_COUNT==5);
    buttons(21000,0); buttons(21500,4); UI_CHECK(menuPosition==4);
    buttons(21600,0); buttons(22000,4); UI_CHECK(menuPosition==0);
    puts("UI drawing regressions passed");
    return 0;
#else
    // Missing values, both layouts, signed values, flags, freshness, all selector states.
    for (int mode=0;mode<2;++mode) {
        guiMode=clockMode=mode;
        if(guiMode)drawGUI();else drawGUI2();
        drawClockGUI();drawTFT2();
        for(int i=0;i<11;++i)display(i,i==8?2347:i==4||i==10?1475:80);
        display(11,12600);display(12,139);
        measurement(TEMP_IN,-125);measurement(TEMP_OUT,-1000);
        measurement(TEMP_AC,235);measurement(HUM,505);
        drawTFT2();drawTFT2(); // Unchanged values must remain cached.
    }
    guiMode=clockMode=true;drawGUI();drawClockGUI();
    dpid.state=VpwDpid::STREAM;
    for(int gear: {1,2,4,8,16,32,64,0}) {
        status(0,19);status(1,19);status(2,108);status(3,gear);
        status(4,128);status(5,7);status(6,1);status(7,3);
        drawFirstExtra();drawClimateIndicators();
        drawFirstExtra();drawClimateIndicators();
    }
    status(0,20);status(1,65535);status(2,255);status(4,0);status(5,6);status(6,0);status(7,0);
    drawFirstExtra();drawClimateIndicators();
    testUs+=501000;drawFirstExtra();drawClimateIndicators();
    dpid.state=VpwDpid::LEGACY;drawFirstExtra();drawClimateIndicators();
    FILE* file=fopen(TRACE_FILE,"wb");if(!file)return 2;
    const auto output=trace.str();fwrite(output.data(),1,output.size(),file);fclose(file);
#endif
}
