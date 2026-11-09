#include "Arduino.h"
#include "SdFat.h"
#include <vector>
#ifndef DEBUG_MODEM
#define DEBUG_MODEM 0
#endif
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
#define DBG2(...) ((void)0)
#define DBGL2(...) ((void)0)
#include "gauge_fixture.h"
void encodeCoord(float deg,int16_t& low,int16_t& high){int32_t v=(int32_t)(deg*1e6f);low=(int16_t)v;high=(int16_t)(v>>16);}
int freeRam(){return 2000;}
unsigned clockUpdates = 0;
bool parseModemClock(const char *line, uint32_t &utc) {
 if (strcmp(line,"+CCLK: \"26/11/08,22:00:00+00\"")) return false;
 utc=1794175200UL; return true;
}
bool syncRtcUtc(uint32_t) { ++clockUpdates; return true; }
#include "../../ls1gauge-mega/modem.h"
SdFat SD;
bool csvProvisioned=true, csvTlsQualified=true, csvQueueReady=true;
uint8_t csvDeviceSecret[32]={1};
uint8_t csvQueueId[16]={1,0,13,10,0,255};
char csvQueueName[13]="00000001.CSV";
uint32_t testLimit=1300;
uint32_t csvQueueLimit(){return testLimit;}
void csvQueueRelease(){csvQueueReady=false;}
#include "../../ls1gauge-mega/modem-csv.h"
static int failures=0;
#define CHECK(c) do {if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
struct Peer {
 std::string pending,body;
 std::vector<std::string> bodies,commands;
 unsigned remaining=0,remote=0;
 int status=200,length=24;
 bool dropAction=false,wrongId=false;
 bool ready=true,dropTerm=false,dropInit=false;
 bool ntpFailure=false,ntpSilent=false;
 bool documentedRead=true,omitReadTerminator=false;
 std::string readHeader="+HTTPREAD: 24";
 std::string failCommand;
 void inject(const std::string &s){for(char c:s)Serial1.rx.push_back(c);}
 void step(){
  testUs+=1000;Serial1.room=2;Serial.room=63;
  csvUploadScheduler();modemInitTick();
  CHECK(Serial1.tx.size()<=MODEM_BUDGET);CHECK(Serial1.blockedWrites==0);
  std::string bytes=Serial1.tx;Serial1.tx.clear();
  for(char c:bytes){
   if(remaining){body+=c;if(--remaining==0){bodies.push_back(body);unsigned n=(uint8_t)body[56]+256u*(uint8_t)body[57];if(n)remote+=n;body.clear();inject("\r\nOK\r\n");}continue;}
   if(c=='\r')continue;if(c!='\n'){pending+=c;continue;}
   commands.push_back(pending);
   if(pending=="AT+HTTPINIT")CHECK(at.timeout==120000UL);
   if ((pending=="AT+HTTPTERM" && dropTerm) || (pending=="AT+HTTPINIT" && dropInit)) {}
   else if(pending==failCommand)inject("ERROR\r\n");
   else if(pending=="AT+CNTP") { if(!ntpSilent) inject(ntpFailure ? "OK\r\n+CNTP: 6\r\n" : "OK\r\n+CNTP: 0\r\n"); }
   else if(pending=="AT+CCLK?") inject("+CCLK: \"26/11/08,22:00:00+00\"\r\nOK\r\n");
   else if(pending=="AT+CPAS") inject(ready ? "+CPAS: 0\r\nOK\r\n" : "OK\r\n");
   else if(pending=="AT+CPIN?") inject("+CPIN: READY\r\nOK\r\n");
   else if(pending=="AT+CGATT?") inject("+CGATT: 1\r\nOK\r\n");
   else if(pending=="AT+NETOPEN?") inject("+NETOPEN: 1\r\nOK\r\n");
   else if(pending=="AT+CGPS?") inject("+CGPS: 1,1\r\nOK\r\n");
   else if(pending=="AT+HTTPACTION=1") {if(!dropAction)inject("OK\r\n+HTTPACTION: 1,"+std::to_string(status)+","+std::to_string(length)+"\r\n");}
   else if(pending=="AT+HTTPREAD=0,24"){
    std::string ack="P59A";ack.append((char*)csvQueueId,16);if(wrongId)ack[4]^=1;
    for(int i=0;i<4;++i)ack+=(char)(remote>>(8*i));
    inject((documentedRead ? "OK\r\n" : "")+readHeader+"\r\n"+ack+
           (omitReadTerminator ? "" : documentedRead ? "\r\n+HTTPREAD: 0\r\n" : "\r\nOK\r\n"));
   } else if(pending.rfind("AT+HTTPDATA=",0)==0){remaining=atoi(pending.c_str()+12);inject("DOWNLOAD\r\n");}
   else inject("OK\r\n");
   pending.clear();
  }
 }
};
void reset(){
 Serial1={};Serial={};at={};testUs=0;testStepUs=0;testMillisOffset=0;
 modemOwner=OWNER_IDLE;modemOnline=initReady=true;nextNetCheck=1000000;
 csvHttpState=CSV_IDLE;csvFileStarted=false;csvSessionConfigured=false;csvReadHeader=false;
 csvReleaseAfterTerm=false;csvRetryAfter=0;csvAckCount=0;logUploadErrors=0;
 csvProvisioned=csvTlsQualified=csvQueueReady=true;csvUploader=CsvUpload();
 testLimit=1300;
 modemTimeNext=1000000;modemTimeStep=0;clockUpdates=0;
 modemPowerNext=0;modemPowerStep=0;tries=0;
 disk[csvQueueName]=std::make_shared<FakeDiskData>();disk[csvQueueName]->bytes.resize(1300,'x');
}
int main(){
 reset();modemTimeNext=0;Peer timed;
 for(int i=0;i<15000&&csvQueueReady;++i)timed.step();
 CHECK(clockUpdates==1&&!csvQueueReady&&csvUploader.offset()==1300);
 CHECK(timed.commands.front()=="AT+CNTP=\"pool.ntp.org\",0");
 for(bool silentTime : {false,true}) {
  reset();modemTimeNext=0;Peer unavailable;unavailable.ntpFailure=true;unavailable.ntpSilent=silentTime;
  for(int i=0;i<45000&&csvQueueReady;++i)unavailable.step();
  CHECK(clockUpdates==0&&!csvQueueReady&&csvUploader.offset()==1300);
 }
 // Open-drain PWRKEY, shutdown grace, probe before another power-on pulse.
 reset();startModemPowerCycle();
 CHECK(modemOwner==OWNER_POWER&&pinModes[12]==OUTPUT&&pins[12]==LOW);
 testUs=2999000;modemInitTick();CHECK(pinModes[12]==OUTPUT);
 testUs=3000000;modemInitTick();CHECK(pinModes[12]==INPUT&&pins[12]==LOW);
 testUs=32999000;modemInitTick();CHECK(!at.busy);
 testUs=33000000;modemInitTick();CHECK(at.busy);
 at.busy=false;at.done=true;at.result=true;modemInitTick();
 CHECK(modemOwner==OWNER_INIT&&pinModes[12]==INPUT);
 reset();startModemPowerCycle();testUs=3000000;modemInitTick();
 testUs=33000000;modemInitTick();at.busy=false;at.done=true;at.result=false;modemInitTick();
 CHECK(pinModes[12]==OUTPUT&&pins[12]==LOW);
 testUs=33500000;modemInitTick();CHECK(pinModes[12]==INPUT);
 testUs=48500000;modemInitTick();CHECK(modemOwner==OWNER_INIT);
 CHECK(waitedMs==0);
 // Repeated unanswered AT triggers recovery, explicit ERROR / no network does not.
 reset();modemOwner=OWNER_INIT;stepIdx=0;tries=4;at.done=true;at.responded=false;
 modemInitTick();CHECK(modemOwner==OWNER_POWER);
 reset();modemOwner=OWNER_INIT;stepIdx=0;tries=4;at.done=true;at.responded=true;
 modemInitTick();CHECK(modemOwner==OWNER_INIT);
 CHECK(strcmp_P("+CPIN: READY",initStep(2).expected)==0);
 CHECK(strcmp_P("AT+CGATT?",initStep(5).command)==0);
 reset();modemOwner=OWNER_INIT;stepIdx=5;tries=4;at.done=true;at.result=false;at.responded=true;
 modemInitTick();CHECK(stepIdx==5&&tries==0);
 reset();modemOwner=OWNER_INIT;stepIdx=5;tries=4;at.done=true;at.responded=false;
 modemInitTick();CHECK(stepIdx==0&&tries==0);
 reset();modemOwner=OWNER_INIT;stepIdx=0;tries=4;at.done=true;modemPowerNext=120000;
 modemInitTick();CHECK(modemOwner==OWNER_INIT);
 reset();modemOwner=OWNER_NET_ATTACH;at.done=true;at.result=true;
 modemInitTick();CHECK(modemOnline&&modemOwner==OWNER_IDLE&&!at.busy);
 reset();csvTlsQualified=false;Peer disabled;for(int i=0;i<1000;++i)disabled.step();CHECK(disabled.bodies.empty()&&disabled.commands.empty());
 reset();Peer ok;for(int i=0;i<15000&&csvQueueReady;++i)ok.step();
 CHECK(!csvQueueReady);CHECK(logUploadErrors==0);CHECK(ok.bodies.size()==3);CHECK(csvUploader.offset()==1300);
 if(ok.bodies.size()==3){CHECK(ok.bodies[0].size()==58);CHECK(ok.bodies[1].size()==1082);CHECK(ok.bodies[2].size()==334);}
 CHECK(std::count(ok.commands.begin(),ok.commands.end(),"AT+CSSLCFG=\"authmode\",0,1")==1);
 CHECK(std::count(ok.commands.begin(),ok.commands.end(),"AT+HTTPTERM")==1);
 CHECK(std::find(ok.commands.begin(),ok.commands.end(),"AT+HTTPPARA=\"URL\",\"https://camaro.it-tiger.pl/v1/chunks\"")!=ok.commands.end());
 reset();Peer legacy;legacy.documentedRead=false;
 for(int i=0;i<15000&&csvQueueReady;++i)legacy.step();CHECK(!csvQueueReady&&csvUploader.offset()==1300);
 // SIM7600E-L1C LE20CB03: initial OK, DATA header, binary ACK, zero terminator.
 reset();Peer hardware;hardware.readHeader="+HTTPREAD: DATA,24";
 for(int i=0;i<15000&&csvQueueReady;++i)hardware.step();
 CHECK(!csvQueueReady&&csvUploader.offset()==1300&&logUploadErrors==0);
 for(const char *header : {"+HTTPREAD: DATA,25", "+HTTPREAD: DATA,24junk",
                          "+HTTPREAD: DATA,4294967296", "+HTTPREAD: DATA,-1"}) {
  reset();Peer malformed;malformed.readHeader=header;
  for(int i=0;i<15000&&modemOwner!=OWNER_RECOVERY;++i)malformed.step();
  CHECK(modemOwner==OWNER_RECOVERY&&csvUploader.offset()==0&&csvQueueReady);
 }
 reset();Peer incomplete;incomplete.omitReadTerminator=true;
 for(int i=0;i<15000&&modemOwner!=OWNER_RECOVERY;++i)incomplete.step();
 CHECK(modemOwner==OWNER_RECOVERY&&csvUploader.offset()==0&&csvQueueReady);
 CHECK(incomplete.bodies.size()==1);
 reset();Peer ahead;ahead.remote=2000;
 for(int i=0;i<15000&&csvQueueReady;++i)ahead.step();CHECK(!csvQueueReady&&csvUploader.offset()==0);
 CHECK(std::count(ahead.commands.begin(),ahead.commands.end(),"AT+HTTPTERM")==1);
 CHECK(logUploadErrors==1&&!csvReleaseAfterTerm&&!csvFileStarted);
 reset();disk[csvQueueName]->readError=true;Peer unreadable;
 for(int i=0;i<15000&&csvQueueReady;++i)unreadable.step();CHECK(!csvQueueReady&&unreadable.bodies.size()==1);
 reset();testLimit=32768;disk[csvQueueName]->bytes.resize(testLimit,'x');Peer burst;
 for(int i=0;i<25000;++i){burst.step();if(!modemHttpPending()&&!burst.bodies.empty())break;}
 CHECK(burst.bodies.size()==16);CHECK(csvQueueReady);CHECK(modemOwner==OWNER_IDLE);
 CHECK(csvUploader.offset()==15*CsvUpload::CHUNK_SIZE);CHECK((int32_t)(csvRetryAfter-millis())>0);
 for(int mode=0;mode<5;++mode){
  reset();Peer bad;
  if(mode==0)bad.failCommand="AT+CSSLCFG=\"authmode\",0,1";
  if(mode==1)bad.status=401;
  if(mode==2)bad.length=25;
  if(mode==3)bad.wrongId=true;
  if(mode==4)bad.dropAction=true;
  for(int i=0;i<65000&&modemOwner!=OWNER_RECOVERY;++i)bad.step();
  CHECK(modemOwner==OWNER_RECOVERY);CHECK(csvQueueReady);CHECK(csvUploader.offset()==0);
  if(mode==0)CHECK(bad.bodies.empty());
  // Recovery probes after the payload guard, without waiting 130 seconds.
  auto count=bad.commands.size();
  for(int i=0;i<5400;++i)bad.step();
  CHECK(bad.commands.size()==count);
  for(int i=0;i<10000&&modemOwner==OWNER_RECOVERY;++i)bad.step();
  CHECK(modemOwner==OWNER_INIT);
  CHECK(std::find(bad.commands.begin()+count,bad.commands.end(),"AT+CPAS")!=bad.commands.end());
 }
 // Bare OK and late HTTP results cannot establish command readiness or advance ACKs.
 reset();recoverModem();Peer notReady;notReady.ready=false;
 for(int i=0;i<20000;++i){
  if(i==6000)notReady.inject("+HTTPACTION: 1,200,24\r\nOK\r\n");
  notReady.step();
 }
 CHECK(modemOwner==OWNER_RECOVERY&&csvUploader.offset()==0);
 CHECK(std::count(notReady.commands.begin(),notReady.commands.end(),"AT+HTTPINIT")==0);
 notReady.ready=true;
 for(int i=0;i<25000&&csvQueueReady;++i)notReady.step();
 CHECK(!csvQueueReady&&csvUploader.offset()==1300);
 // A delayed DOWNLOAD restarts the full input-window guard.
 reset();recoverModem();Peer late;
 for(int i=0;i<5000;++i)late.step();
 late.inject("DOWNLOAD\r\n");
 for(int i=0;i<5400;++i)late.step();
 CHECK(late.commands.empty());
 for(int i=0;i<2000&&modemOwner==OWNER_RECOVERY;++i)late.step();
 CHECK(modemOwner==OWNER_INIT);
 // HTTPTERM may report no session, but HTTPINIT still has to succeed.
 reset();recoverModem();Peer absent;absent.failCommand="AT+HTTPTERM";absent.dropInit=true;
 for(int i=0;i<130000;++i)absent.step();
 CHECK(modemOwner==OWNER_RECOVERY&&csvUploader.offset()==0);
 CHECK(absent.bodies.empty());
 // No response to HTTPTERM is not the same as its explicit ERROR response.
 reset();recoverModem();Peer silent;silent.dropTerm=true;
 strcpy(at.line,"ERROR");
 for(int i=0;i<130000;++i)silent.step();
 CHECK(modemOwner==OWNER_RECOVERY);
 CHECK(std::count(silent.commands.begin(),silent.commands.end(),"AT+HTTPINIT")==0);
 // The unsigned quiet interval also works across millis() wraparound.
 reset();testMillisOffset=UINT32_MAX-2000;recoverModem();Peer wrapped;
 for(int i=0;i<10000&&modemOwner==OWNER_RECOVERY;++i)wrapped.step();
 CHECK(modemOwner==OWNER_INIT);
 // Full TX ring expires without sending commands into HTTPDATA or changing the packet.
 reset();csvHttpState=CSV_BODY;modemOwner=OWNER_HTTP;csvBodyOffset=0;csvBodySize=58;csvBodySince=0;
 csvUploader.begin(csvDeviceSecret,csvQueueId);
 Serial1.room=0;for(int i=0;i<4600&&modemOwner!=OWNER_RECOVERY;++i){testUs+=1000;modemHttpTick();}
 CHECK(millis()>=4500);CHECK(modemOwner==OWNER_RECOVERY);CHECK(Serial1.blockedWrites==0);CHECK(waitedMs==0);
 reset();Peer replaced;for(int i=0;i<3000&&csvHttpState!=CSV_BODY;++i)replaced.step();
 CHECK(csvHttpState==CSV_BODY);csvQueueId[15]^=1;replaced.step();
 CHECK(modemOwner==OWNER_RECOVERY);CHECK(csvUploader.offset()==0);csvQueueId[15]^=1;
 // Strict numeric parser rejects overflow and trailing data.
 const char *numbers[]={"4294967296","24junk","-1","24"};
 for(int i=0;i<4;++i){const char *p=numbers[i];uint32_t n;CHECK(csvNumber(p,n,0)==(i==3));}
 printf("CSV modem failures: %d\n",failures);return failures?1:0;
}
