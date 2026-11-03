#include "Arduino.h"
#define CMD_SIZE 17
#define DBGL(...) ((void)0)
#define DBG(...) ((void)0)
uint8_t rtcDay = 27;
struct DateTime {
  int year()const{return 2026;} int month()const{return 10;} int day()const{return rtcDay;}
  int hour()const{return 12;} int minute()const{return 0;} int second()const{return 0;}
};
struct {DateTime now(){return {};}} rtc;
struct {int16_t wartosc=0,lastValue=0;} cmd[CMD_SIZE];
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
  // LTE keeps the exact record size and now takes one 1 Hz snapshot per record.
  CHECK(sizeof(LogEntry)==42);CHECK(SREDNIA==1);
  sums[8]=2347;addLogEntry();CHECK(logIndex==1&&logBuffer[0].data[8]==2347);
  LogEntry saved=logBuffer[0];auto generation=logGeneration;
  cmd[8].wartosc=2347;
  testUs=199000;updateLogToSD();CHECK(disk.empty());
  testUs=200000;updateLogToSD();
  auto file=disk["f-261027.csv"];CHECK(file!=nullptr);
  if(!file)return 1;
  CHECK(file->text.find("Datetime,Uptime_ms,")==0);
  CHECK(file->text.find("2026-10-27 12:00:00,200,")!=std::string::npos);
  CHECK(std::count(file->text.begin(),file->text.end(),'\n')==2);
  testUs=399000;updateLogToSD();CHECK(sdRows==1);
  for(int i=2;i<=5;++i){testUs=i*200000;updateLogToSD();}
  CHECK(sdRows==5);CHECK(file->syncs==0);
  testUs=1200000;updateLogToSD();CHECK(file->syncs==1);
  testUs=3200000;updateLogToSD();CHECK(sdRows==7); // One row, not ten copies after a delay.
  CHECK(memcmp(&saved,&logBuffer[0],sizeof(saved))==0);
  CHECK(logIndex==1&&logGeneration==generation); // SD cannot consume/reset the LTE buffer.
  file->error=true;testUs=3400000;updateLogToSD();CHECK(sdErrors==1);
  auto failedSize=file->text.size();testUs=3600000;updateLogToSD();CHECK(file->text.size()==failedSize);
  file->error=false;testUs=8400000;updateLogToSD();CHECK(sdRows==8);
  auto previousSyncs=file->syncs;rtcDay=28;testUs=8600000;updateLogToSD();
  CHECK(file->syncs==previousSyncs+1);CHECK(disk.count("f-261028.csv")==1);
  CHECK(disk["f-261028.csv"]->text.find("2026-10-28 12:00:00,8600,")!=std::string::npos);
  testUs=0;testMillisOffset=0xfffffff0u;updateLogToSD();auto beforeWrap=sdRows;
  testMillisOffset=0xb8u;updateLogToSD();CHECK(sdRows==beforeWrap+1);testMillisOffset=0;
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
