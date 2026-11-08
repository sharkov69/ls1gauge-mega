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
bool csvQueueNewFile(const char*) { return true; }
bool csvQueueRename(const char*,const char*) { return true; }
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
  for (auto& channel : channels) channel.set(0);
  gpsHigh[0] = gpsHigh[1] = 0;
  // File metadata must use the hardware RTC, not the library's default date.
  FsDateTime::setCallback(sdDateTime);
  channels[8].value=2347;
  channels[INJ1].setRaw(19,0);channels[INJ2].setRaw(65535,0);channels[SA].setRaw(108,0);
  channels[PRNDL].setRaw(8,0);channels[TCC].setRaw(128,0);channels[SHIFT_MODE].setRaw(7,0);
  channels[AC_STATE].setRaw(11,0);channels[FAN_STATE].setRaw(3,0);channels[KR].setRaw(91,0);channels[CL_STATE].setRaw(35,0);
  testUs=199000;updateLogToSD();CHECK(disk.empty());
  testUs=200000;updateLogToSD();
  auto file=disk["l-261027.csv"];CHECK(file!=nullptr);
  if(!file)return 1;
  CHECK(file->date==FS_DATE(2026,10,27));CHECK(file->time==FS_TIME(12,0,0));
  CHECK(file->text.substr(0, file->text.find('\n')) == "Datetime;Control Module Voltage;Engine RPM (SAE);Vehicle Speed (SAE);Throttle Position (SAE);Intake Air Temp (SAE);Engine Coolant Temp (SAE);Trans Fluid Temp;Intake Manifold Absolute Pressure (SAE);Engine Oil Pressure;Timing Advance (SAE);AFR 1;AFR 2;Vent Temp;Vent Humidity(%);Outside Temp;Inside Temp;Latitude;Longitude;PRNDL Status;TCC Applied;Trans Shift Mode;Injector Pulse Width Avg. Bank 1;Injector Pulse Width Avg. Bank 2;AC Requested;AC Active;AC Clutch Engaged;AC Disable (Pressure);AC Disable (WOT);Fan 1;Fan 2;Knock Retard;Closed Loop Active;Reverse Inhibited;Fuel Trim Learn;O2 B1S1 Ready;O2 B2S1 Ready");
  CHECK(file->text.find("Uptime_ms")==std::string::npos);
  CHECK(file->text.find(";PRNDL Status;TCC Applied;Trans Shift Mode;")!=std::string::npos);
  CHECK(file->text.find(";Fan 1;Fan 2;Knock Retard;")!=std::string::npos);
  CHECK(file->text.find("2026-10-27 12:00:00,000;")!=std::string::npos);
  CHECK(file->text.find(";8;1;7;0,290;1000,000;1;1;1;0;0;1;1;8,00;1;1;1;1;0")!=std::string::npos);
  CHECK(std::count(file->text.begin(),file->text.end(),'\n')==2);
  testUs=399000;updateLogToSD();CHECK(sdRows==1);
  for(int i=2;i<=5;++i){testUs=i*200000;updateLogToSD();}
  CHECK(sdRows==5);CHECK(file->syncs==0);
  CHECK(file->text.find("2026-10-27 12:00:00,800;")!=std::string::npos);
  testUs=1200000;updateLogToSD();CHECK(file->syncs==1);
  testUs=3200000;updateLogToSD();CHECK(sdRows==7); // One row, not ten copies after a delay.
  CHECK(file->text.substr(file->text.rfind('\n', file->text.size()-2)+1).find(";8;1;7;0,290;1000,000;1;1;1;0;0;1;1;8,00;1;1;1;1;0")!=std::string::npos); // Last received values survive the old 3 s timeout.
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
  CHECK(excel->text.find(";13,8;2347;0;0;0;0;0;1,23;0,0;-10,0;0,00;14,75;")!=std::string::npos);
  CHECK(excel->text.find(";-12,5;")!=std::string::npos);
  CHECK(excel->text.find(";52,158545;")!=std::string::npos);
  auto firstLine=excel->text.find('\n');
  CHECK(std::count(excel->text.begin(),excel->text.begin()+firstLine,';')==36);
  CHECK(std::count(excel->text.begin()+firstLine,excel->text.end(),';')==36);
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

  // Never received is blank; a real zero and old readings remain numeric.
  sdLogFile.close(); disk.clear(); sdClock={}; sdLastSample=0; sdBackoff=false;
  for (auto& channel : channels) channel = Channel{};
  testUs=200000; updateLogToSD();
  auto missing=disk["l-261027.csv"];
  CHECK(missing->text.substr(missing->text.find('\n')+1)=="2026-10-27 12:00:00,000"+std::string(36,';')+"\n");
  channels[BATT].set(0); channels[RPM].set(1234); channels[SA].setRaw(128,millis());
  channels[FAN_STATE].setRaw(0,millis()); channels[INJ1].setRaw(32768,millis());
  channels[LAT].set((int16_t)0x8000); gpsHigh[0]=1;
  testUs=400000; updateLogToSD();
  auto retained=missing->text.substr(missing->text.rfind('\n',missing->text.size()-2)+1);
  CHECK(retained.find(";0,0;1234;")!=std::string::npos);
  CHECK(retained.find(";0,098304;")!=std::string::npos);
  CHECK(retained.find("500,008")!=std::string::npos);
  testUs=10000000; updateLogToSD();
  auto later=missing->text.substr(missing->text.rfind('\n',missing->text.size()-2)+1);
  CHECK(retained.substr(23)==later.substr(23));
  channels[RPM].set(0); testUs=10200000; updateLogToSD();
  CHECK(missing->text.substr(missing->text.rfind('\n',missing->text.size()-2)+1).find(";0,0;0;")!=std::string::npos);
  // Exercise real PID decoding: continuous zero stops both logs, not ECU reads.
  auto rpmReply=[](uint16_t rpm,uint32_t ms){
    testUs=0;testMillisOffset=ms;
    uint16_t raw=rpm*4;
    uint8_t reply[]={0x6c,0xf1,0x10,0x41,0x0c,(uint8_t)(raw>>8),(uint8_t)raw,0};
    decodeGaugeReply(reply,sizeof(reply));
  };
  rpmReply(800,20000);updateLogToSD();
  rpmReply(0,21000);updateLogToSD();
  rpmReply(0,30000);updateLogToSD(); // Repeated zeros must not restart the timer.
  testMillisOffset=31000;auto rows=sdRows;updateLogToSD();CHECK(sdRows==rows+1);
  auto beforePause=missing->text.size();auto syncs=missing->syncs;
  testMillisOffset=31001;updateLogToSD();
  CHECK(missing->text.size()==beforePause);CHECK(!sdLogFile);CHECK(missing->syncs==syncs+1);
  testMillisOffset=200000;updateLogToSD();CHECK(missing->text.size()==beforePause);
  rpmReply(1,200001);updateLogToSD();CHECK(missing->text.size()==beforePause);
  rpmReply(2,200002);updateLogToSD();CHECK(sdRows==rows+2);CHECK(sdLogFile);
  CHECK(missing->text.find("Datetime;")==missing->text.rfind("Datetime;")); // One header.
  // A short stop and a nonzero reading reset the continuous-zero timer.
  rpmReply(0,210000);updateLogToSD();rpmReply(1,215000);updateLogToSD();
  rpmReply(0,215001);updateLogToSD();testMillisOffset=220002;
  rows=sdRows;updateLogToSD();CHECK(sdRows==rows+1);
  rpmReply(800,220003);updateLogToSD();
  // Unsigned elapsed time must also work across millis() rollover.
  rpmReply(0,0xfffffff0u);updateLogToSD();
  testMillisOffset=9984;updateLogToSD();CHECK(sdLogFile);
  testMillisOffset=9985;updateLogToSD();CHECK(!sdLogFile);
  // DPID uses the same RPM decoder and must resume on the first packet.
  uint8_t packet[]={0,8,0,0,0,0};decodeDpidPacket(0,packet);
  rows=sdRows;updateLogToSD();CHECK(sdRows==rows+1);
  printf("Storage failures: %d\n",failures);return failures?1:0;
}
