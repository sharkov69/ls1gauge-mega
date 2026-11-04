#include "Arduino.h"
#include <ctime>
#define CMD_SIZE 17
#define DBGL(...) ((void)0)
#define DBG(...) ((void)0)
uint8_t rtcDay = 27;
struct DateTime {
  uint32_t epoch;
  DateTime(uint32_t t=1793102400u):epoch(t){}
  std::tm parts()const{time_t t=epoch;std::tm result={};gmtime_s(&result,&t);return result;}
  int year()const{return parts().tm_year+1900;} int month()const{return parts().tm_mon+1;} int day()const{return parts().tm_mday;}
  int hour()const{return parts().tm_hour;} int minute()const{return parts().tm_min;} int second()const{return parts().tm_sec;}
  uint32_t unixtime()const{return epoch;}
};
struct {DateTime now(){return DateTime(1793102400u+(rtcDay-27)*86400u+millis()/1000);}} rtc;
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
  // File metadata must use the hardware RTC, not the library's default date.
  FsDateTime::setCallback(sdDateTime);
  // LTE keeps the exact record size and now takes one 1 Hz snapshot per record.
  CHECK(sizeof(LogEntry)==42);CHECK(SREDNIA==1);
  sums[8]=2347;addLogEntry();CHECK(logIndex==1&&logBuffer[0].data[8]==2347);
  LogEntry saved=logBuffer[0];auto generation=logGeneration;
  cmd[8].wartosc=2347;
  testUs=199000;updateLogToSD();CHECK(disk.empty());
  testUs=200000;updateLogToSD();
  auto file=disk["l-261027.csv"];CHECK(file!=nullptr);
  if(!file)return 1;
  CHECK(file->date==FS_DATE(2026,10,27));CHECK(file->time==FS_TIME(12,0,0));
  CHECK(file->text.find("Datetime;")==0);
  CHECK(file->text.find("Uptime_ms")==std::string::npos);
  CHECK(file->text.find("2026-10-27 12:00:00,000;")!=std::string::npos);
  CHECK(std::count(file->text.begin(),file->text.end(),'\n')==2);
  testUs=399000;updateLogToSD();CHECK(sdRows==1);
  for(int i=2;i<=5;++i){testUs=i*200000;updateLogToSD();}
  CHECK(sdRows==5);CHECK(file->syncs==0);
  CHECK(file->text.find("2026-10-27 12:00:00,800;")!=std::string::npos);
  testUs=1200000;updateLogToSD();CHECK(file->syncs==1);
  testUs=3200000;updateLogToSD();CHECK(sdRows==7); // One row, not ten copies after a delay.
  CHECK(memcmp(&saved,&logBuffer[0],sizeof(saved))==0);
  CHECK(logIndex==1&&logGeneration==generation); // SD cannot consume/reset the LTE buffer.
  file->error=true;testUs=3400000;updateLogToSD();CHECK(sdErrors==1);
  auto failedSize=file->text.size();testUs=3600000;updateLogToSD();CHECK(file->text.size()==failedSize);
  file->error=false;testUs=8400000;updateLogToSD();CHECK(sdRows==8);
  auto previousSyncs=file->syncs;rtcDay=28;testUs=8600000;updateLogToSD();
  CHECK(file->syncs==previousSyncs+1);CHECK(disk.count("l-261028.csv")==1);
  CHECK(disk["l-261028.csv"]->text.find("2026-10-28 12:00:08,000;")!=std::string::npos);
  testUs=0;testMillisOffset=0xfffffff0u;updateLogToSD();auto beforeWrap=sdRows;
  testMillisOffset=0xb8u;updateLogToSD();CHECK(sdRows==beforeWrap+1);testMillisOffset=0;
  // Continuous elapsed time crosses midnight and millis rollover without jumping.
  SdSampleClock timeline;
  timeline.update(1793145599u,0xfffffff0u);
  timeline.update(1793145599u,0xb8u);
  CHECK(timeline.seconds==1793145599u&&timeline.fraction==200);
  timeline.update(1793145600u,0x3d8u);
  CHECK(timeline.seconds==1793145600u&&timeline.fraction==0);
  CHECK(DateTime(timeline.seconds).day()==28&&DateTime(timeline.seconds).hour()==0);
  timeline.update(1793149200u,0x4a0u);
  CHECK(timeline.seconds==1793149200u&&timeline.fraction==0); // Manual RTC correction.
  // Keep old CSV intact, with an explicit backup instead of mixed columns.
  sdLogFile.close();disk.clear();sdClock={};sdLastSample=0;sdBackoff=false;
  rtcDay=27;testUs=200000;
  auto old=std::make_shared<FakeDiskData>();old->text="Datetime,RPM\nold data\n";
  disk["l-261027.csv"]=old;
  cmd[1].wartosc=123;cmd[3].wartosc=138;cmd[4].wartosc=1475;cmd[11].wartosc=-125;
  const int32_t latitude=52158545;
  cmd[15].wartosc=(int16_t)latitude;cmd[15].lastValue=(int16_t)(latitude>>16);
  updateLogToSD();
  CHECK(disk["l-261027.bak"]==old);CHECK(old->text=="Datetime,RPM\nold data\n");
  auto excel=disk["l-261027.csv"];CHECK(excel!=old);
  CHECK(excel->text.find(";1,23;0;13,8;14,75;")!=std::string::npos);
  CHECK(excel->text.find(";-12,5;")!=std::string::npos);
  CHECK(excel->text.find(";52,158545;")!=std::string::npos);
  auto firstLine=excel->text.find('\n');
  CHECK(std::count(excel->text.begin(),excel->text.begin()+firstLine,';')==17);
  CHECK(std::count(excel->text.begin()+firstLine,excel->text.end(),';')==17);
  testUs=2200000;updateLogToSD();CHECK(excel->time==FS_TIME(12,0,2));
  sdLogFile.close();testUs=2400000;updateLogToSD();CHECK(disk["l-261027.csv"]==excel);
  CHECK(std::count(excel->text.begin(),excel->text.end(),'\n')==4); // Header not repeated on reopen.
  sdLogFile.close();disk.erase("l-261027.bak");excel->readError=true;testUs=2600000;
  auto ioErrors=sdErrors;updateLogToSD();CHECK(sdErrors==ioErrors+1);
  CHECK(disk["l-261027.csv"]==excel&&disk.count("l-261027.bak")==0);
  excel->readError=false;sdBackoff=false;disk["l-261027.bak"]=old;
  sdLogFile.close();disk["l-261027.csv"]=old;testUs=2800000;
  auto errors=sdErrors;updateLogToSD();CHECK(sdErrors==errors+1);
  CHECK(disk["l-261027.csv"]==old&&disk["l-261027.bak"]==old); // No overwrite on backup conflict.
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
