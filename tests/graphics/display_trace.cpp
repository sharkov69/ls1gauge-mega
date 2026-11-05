#include "Arduino.h"
#include <sstream>
#include <iomanip>
#define private public
#include "../../libraries/J1850-VPW-WC/j1850vpw-wc.cpp"
#undef private
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
inline long random(long a, long) { return a; }
inline long random(long) { return 0; }
uint8_t pause = 75;
uint16_t counter = 0;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define constrain(v,a,b) ((v)<(a)?(a):((v)>(b)?(b):(v)))
#include "../../ls1gauge-mega/cmd.h"
using __FlashStringHelper = char;
constexpr int GC9A01A_BLACK=0, GC9A01A_WHITE=65535, GC9A01A_RED=63488,
              GC9A01A_GREEN=2016, GC9A01A_YELLOW=65504, GC9A01A_CYAN=2047;
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
bool guiStarted=true, guiMode=true, clockMode=true, clock=true;
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
}
