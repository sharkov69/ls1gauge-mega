#include "Arduino.h"
#include <ctime>
#include "gauge_fixture.h"
#define DBGL(...) ((void)0)
#define DBG(...) ((void)0)
struct DateTime {
  uint32_t epoch;
  DateTime(uint32_t t=1793102400u):epoch(t){}
  std::tm parts()const{time_t t=epoch;std::tm result={};gmtime_s(&result,&t);return result;}
  int year()const{return parts().tm_year+1900;} int month()const{return parts().tm_mon+1;} int day()const{return parts().tm_mday;}
  int hour()const{return parts().tm_hour;} int minute()const{return parts().tm_min;} int second()const{return parts().tm_sec;}
  uint32_t unixtime()const{return epoch;}
};
struct {DateTime now(){return DateTime(1793102400u+millis()/1000);}} rtc;
#include "../../ls1gauge-mega/sd.h"
#include "../../ls1gauge-mega/csv_upload_queue.h"
#include "../../ls1gauge-mega/csv_provision.h"
static int failures=0;
#define CHECK(c) do {if(!(c)){printf("FAIL %d: %s\n",__LINE__,#c);++failures;}}while(0)
int main(){
  uint8_t provision[40];memset(provision,1,sizeof(provision));
  CHECK(!csvConfigChunk(8,provision));CHECK(!csvConfigChunk(1,provision));
  CHECK(csvConfigChunk(0,provision));CHECK(!csvConfigCommit());
  for(uint8_t i=8;i<40;i+=8)CHECK(csvConfigChunk(i,provision+i));
  CHECK(csvConfigCommit());CHECK(!csvConfigChunk(0,provision));CHECK(!csvConfigCommit());
  CHECK(disk["P59.CFG"]->bytes==std::vector<uint8_t>(provision,provision+40));
  CHECK(!csvSaveTlsQualification());
  csvLoadProvisioning();CHECK(csvProvisioned&&!csvTlsQualified);
  CHECK(csvSaveTlsQualification());CHECK(!csvSaveTlsQualification());
  csvLoadProvisioning();CHECK(csvProvisioned&&csvTlsQualified);
  disk.erase("P59.TLS");
  disk.erase("P59.CFG");
  const uint8_t ns[8]={1};csvQueueInit(ns);
  testUs=200000;updateLogToSD();
  auto file=disk["l-261027.csv"];CHECK(file!=nullptr);CHECK(sdUploadSyncedSize==0);
  csvQueueTick();CHECK(!csvQueueReady);
  testUs=1200000;updateLogToSD();CHECK(sdUploadSyncedSize==file->text.size());
  uint32_t synced=sdUploadSyncedSize;
  testUs=1400000;updateLogToSD();CHECK(file->text.size()>synced);CHECK(sdUploadSyncedSize==synced);
  csvQueueInit(ns);csvQueueTick();CHECK(csvQueueReady);CHECK(csvQueueLimit()==synced);
  // No successful sync means no newly readable tail, even after close/reopen.
  file->error=true;testUs=1600000;updateLogToSD();CHECK(sdUploadFaultLatched);CHECK(csvQueueLimit()==0);
  file->error=false;testUs=6600000;updateLogToSD();CHECK(sdUploadFaultLatched);CHECK(sdUploadSyncedSize==synced);
  testUs=7600000;updateLogToSD();CHECK(sdUploadSyncedSize==file->text.size());CHECK(csvQueueLimit()==0);
  // Provisioning must fail closed before any modem can receive the secret.
  csvLoadProvisioning();CHECK(!csvProvisioned&&!csvTlsQualified);
  auto config=std::make_shared<FakeDiskData>();disk["P59.CFG"]=config;
  config->bytes.assign(39,1);csvLoadProvisioning();CHECK(!csvProvisioned&&!csvTlsQualified);
  config->bytes.assign(41,1);csvLoadProvisioning();CHECK(!csvProvisioned&&!csvTlsQualified);
  config->bytes.assign(40,0);config->bytes[32]=1;
  csvLoadProvisioning();CHECK(!csvProvisioned&&!csvTlsQualified);
  config->bytes[0]=1;config->bytes[32]=0;
  csvLoadProvisioning();CHECK(!csvProvisioned&&!csvTlsQualified);
  config->bytes[32]=1;csvLoadProvisioning();CHECK(csvProvisioned&&!csvTlsQualified);
  CHECK(!memcmp(csvDeviceSecret,config->bytes.data(),32));
  auto qualification=std::make_shared<FakeDiskData>();disk["P59.TLS"]=qualification;
  qualification->bytes={'T','L','S','1',1,0,0,0,0,0,0,0};
  qualification->bytes[4]=2;csvLoadProvisioning();CHECK(csvProvisioned&&!csvTlsQualified);
  qualification->bytes[4]=1;qualification->bytes[3]='2';
  csvLoadProvisioning();CHECK(csvProvisioned&&!csvTlsQualified);
  qualification->bytes[3]='1';qualification->bytes.push_back(0);
  csvLoadProvisioning();CHECK(csvProvisioned&&!csvTlsQualified);
  qualification->bytes.resize(11);csvLoadProvisioning();CHECK(csvProvisioned&&!csvTlsQualified);
  qualification->bytes.resize(12);csvLoadProvisioning();CHECK(csvProvisioned&&csvTlsQualified);
  qualification->readError=true;csvLoadProvisioning();CHECK(csvProvisioned&&!csvTlsQualified);
  qualification->readError=false;config->readError=true;
  csvLoadProvisioning();CHECK(!csvProvisioned&&!csvTlsQualified);
  uint8_t zeroSecret[32]={};CHECK(!memcmp(csvDeviceSecret,zeroSecret,32));
  printf("CSV storage failures: %d\n",failures);return failures?1:0;
}
