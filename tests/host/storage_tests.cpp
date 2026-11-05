#include "Arduino.h"
#include <ctime>
#include "gauge_fixture.h"
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
  for (auto& channel : channels) channel.value = 0;
  gpsHigh[0] = gpsHigh[1] = 0;
  // File metadata must use the hardware RTC, not the library's default date.
  FsDateTime::setCallback(sdDateTime);
  // LTE keeps the exact record size and now takes one 1 Hz snapshot per record.
  CHECK(sizeof(LogEntry)==42);CHECK(SREDNIA==1);
  sums[8]=2347;addLogEntry();CHECK(logIndex==1&&logBuffer[0].data[8]==2347);
  LogEntry saved=logBuffer[0];auto generation=logGeneration;
  channels[8].value=2347;
  channels[INJ1].setRaw(19,0);channels[INJ2].setRaw(65535,0);channels[SA].setRaw(108,0);
  channels[PRNDL].setRaw(8,0);channels[TCC].setRaw(128,0);channels[SHIFT_MODE].setRaw(7,0);
  channels[AC_STATE].setRaw(11,0);channels[FAN_STATE].setRaw(3,0);channels[KR].setRaw(91,0);channels[CL_STATE].setRaw(35,0);
  testUs=199000;updateLogToSD();CHECK(disk.empty());
  testUs=200000;updateLogToSD();
  auto file=disk["l-261027.csv"];CHECK(file!=nullptr);
  if(!file)return 1;
  CHECK(file->date==FS_DATE(2026,10,27));CHECK(file->time==FS_TIME(12,0,0));
  CHECK(file->text.find("Datetime;")==0);
  CHECK(file->text.find("Uptime_ms")==std::string::npos);
  CHECK(file->text.find(";Timing Advance (SAE);PRNDL Status;TCC Applied;Trans Shift Mode;")!=std::string::npos);
  CHECK(file->text.find(";Fan 2;AC Active;AC Clutch Engaged;")!=std::string::npos);
  CHECK(file->text.find("2026-10-27 12:00:00,000;")!=std::string::npos);
  CHECK(file->text.find(";-10,0;8;1;7;0,290;1000,000;1;1;8,00;1;1;1;1;")!=std::string::npos);
  CHECK(std::count(file->text.begin(),file->text.end(),'\n')==2);
  testUs=399000;updateLogToSD();CHECK(sdRows==1);
  for(int i=2;i<=5;++i){testUs=i*200000;updateLogToSD();}
  CHECK(sdRows==5);CHECK(file->syncs==0);
  CHECK(file->text.find("2026-10-27 12:00:00,800;")!=std::string::npos);
  testUs=1200000;updateLogToSD();CHECK(file->syncs==1);
  testUs=3200000;updateLogToSD();CHECK(sdRows==7); // One row, not ten copies after a delay.
  CHECK(file->text.substr(file->text.size()-28)==std::string(27,';')+"\n"); // Stale extras are empty.
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
  channels[1].value=123;channels[3].value=138;channels[4].value=1475;channels[11].value=-125;
  const int32_t latitude=52158545;
  channels[15].value=(int16_t)latitude;gpsHigh[0]=(int16_t)(latitude>>16);
  updateLogToSD();
  CHECK(disk["l-261027.bak"]==old);CHECK(old->text=="Datetime,RPM\nold data\n");
  auto excel=disk["l-261027.csv"];CHECK(excel!=old);
  CHECK(excel->text.find(";1,23;0;13,8;14,75;")!=std::string::npos);
  CHECK(excel->text.find(";-12,5;")!=std::string::npos);
  CHECK(excel->text.find(";52,158545;")!=std::string::npos);
  auto firstLine=excel->text.find('\n');
  CHECK(std::count(excel->text.begin(),excel->text.begin()+firstLine,';')==44);
  CHECK(std::count(excel->text.begin()+firstLine,excel->text.end(),';')==44);
  testUs=2200000;updateLogToSD();CHECK(excel->time==FS_TIME(12,0,2));
  sdLogFile.close();testUs=2400000;updateLogToSD();CHECK(disk["l-261027.csv"]==excel);
  CHECK(std::count(excel->text.begin(),excel->text.end(),'\n')==4); // Header not repeated on reopen.
  sdLogFile.close();disk.erase("l-261027.bak");excel->readError=true;testUs=2600000;
  auto ioErrors=sdErrors;updateLogToSD();CHECK(sdErrors==ioErrors+1);
  CHECK(disk["l-261027.csv"]==excel&&disk.count("l-261027.bak")==0);
  excel->readError=false;sdBackoff=false;disk["l-261027.bak"]=old;
  sdLogFile.close();disk["l-261027.csv"]=old;testUs=2800000;
  auto errors=sdErrors;updateLogToSD();CHECK(sdErrors==errors);
  CHECK(disk["l-261027.b01"]==old&&disk["l-261027.bak"]==old);
  CHECK(disk["l-261027.csv"]!=old); // Existing backup cannot prevent today's new-schema log.
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
