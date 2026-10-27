#include "Arduino.h"
#define CMD_SIZE 17
#define DBGL(...) ((void)0)
#define DBG(...) ((void)0)
struct DateTime {
  int year()const{return 2026;} int month()const{return 10;} int day()const{return 27;}
  int hour()const{return 12;} int minute()const{return 0;} int second()const{return 0;}
};
struct {DateTime now(){return {};}} rtc;
#include "../../ls1gauge-mega/sd.h"
#define GC9A01A_BLACK 0
struct {
  unsigned pixels=0, windows=0;
  uint16_t color565(uint8_t r,uint8_t g,uint8_t b){return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
  void fillScreen(int){} void startWrite(){} void endWrite(){}
  void setAddrWindow(uint8_t,uint8_t,uint8_t w,int){++windows;if(w>16)abort();}
  void SPI_WRITE16(uint16_t){++pixels;}
} tft;
#include "../../ls1gauge-mega/drawbmp.h"
static int failures=0;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
void put32(std::vector<uint8_t>& b,size_t i,uint32_t value){for(int j=0;j<4;++j)b[i+j]=(uint8_t)(value>>(j*8));}
int main(){
  for(int i=0;i<30;++i){logBuffer[i].day=27;logBuffer[i].hour=12;logBuffer[i].second=(uint8_t)i;}
  logIndex=30;++logGeneration;dumpLogToSD();logIndex=0;
  auto file=disk["l-261027.csv"];
  CHECK(sdPending);updateLogToSD();CHECK(sdNext==1&&sdPending);
  for(int i=1;i<30;++i)updateLogToSD();
  CHECK(!sdPending&&sdErrors==0);
  CHECK(std::count(file->text.begin(),file->text.end(),'\n')==31);
  CHECK(file->text.find("2026-10-27 12:00:29")!=std::string::npos);
  logIndex=30;++logGeneration;dumpLogToSD();logIndex=1;updateLogToSD();
  CHECK(!sdPending&&sdErrors==1); // First unread record was overwritten.
  logIndex=30;dumpLogToSD();logIndex=0;file->error=true;updateLogToSD();
  CHECK(!sdPending&&sdErrors==2);
  auto bmp=std::make_shared<FakeDiskData>();disk["test.bmp"]=bmp;
  // Width 17 exercises partial chunks and three padding bytes per row.
  bmp->bytes.resize(54+54*2);auto& b=bmp->bytes;
  b[0]='B';b[1]='M';put32(b,10,54);put32(b,14,40);put32(b,18,17);put32(b,22,2);b[26]=1;b[28]=24;
  startDrawBMP("test.bmp");CHECK(bmpDrawing);
  continueDrawBMP();CHECK(tft.pixels==16&&bmpDrawing);
  continueDrawBMP();CHECK(tft.pixels==17&&bmpRow==1);
  continueDrawBMP();continueDrawBMP();CHECK(tft.pixels==34&&!bmpDrawing);
  put32(b,18,256);startDrawBMP("test.bmp");CHECK(!bmpDrawing);
  put32(b,18,17);b.resize(60);startDrawBMP("test.bmp");continueDrawBMP();CHECK(!bmpDrawing);
  startDrawBMP("missing.bmp");CHECK(!bmpDrawing);
  printf("Storage failures: %d\n",failures);return failures?1:0;
}
