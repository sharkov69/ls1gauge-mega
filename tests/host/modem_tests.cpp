#include "Arduino.h"
#include <vector>
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
#define DBG2(...) ((void)0)
#define DBGL2(...) ((void)0)
enum { LAT=15,LON=16,CMD_SIZE=17 };
struct CmdValue {volatile int16_t wartosc=0;int16_t lastValue=0;} cmd[17];
uint16_t czas=0;
void encodeCoord(float deg,volatile int16_t& low,int16_t& high){int32_t v=(int32_t)(deg*1e6f);low=(int16_t)v;high=(int16_t)(v>>16);}
#include "../../ls1gauge-mega/modem.h"
struct DateTime {uint16_t year()const{return 2026;}uint8_t month()const{return 10;}};
struct {DateTime now(){return {};}} rtc;
#define LOG_ENTRIES 30
#define TOKEN_LENGTH 12
char token[13]="ABCDEFGHIJKL";
struct LogEntry {uint8_t day,hour,minute,second;int16_t data[19];};
LogEntry logBuffer[30]={};
uint8_t logIndex=0;
uint16_t logGeneration=0;
#include "../../ls1gauge-mega/modem-post.h"
static int failures=0;
#define CHECK(c) do {if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
// UART peer: only the hardware boundary is replaced. The real state machines run.
struct Peer {
    std::string pending,body;
    std::vector<std::string> bodies;
    std::vector<std::string> commands;
    unsigned remaining=0;
    void step() {
        testUs+=1000; Serial1.room=2;
        modemInitTick();
        CHECK(Serial1.blockedWrites==0);
        auto bytes=Serial1.tx;Serial1.tx.clear();
        for(char c:bytes){
            if(remaining){body+=c;if(--remaining==0){bodies.push_back(body);body.clear();Serial1.inject("\r\nOK\r\n");}continue;}
            if(c=='\r')continue;
            if(c!='\n'){pending+=c;continue;}
            commands.push_back(pending);
            if(pending=="AT+CPIN?")Serial1.inject("+CPIN: READY\r\nOK\r\n");
            else if(pending=="AT+CREG?")Serial1.inject("+CREG: 0,1\r\nOK\r\n");
            else if(pending=="AT+NETOPEN?")Serial1.inject("+NETOPEN: 1\r\nOK\r\n");
            else if(pending=="AT+CGPS?")Serial1.inject("+CGPS: 1,1\r\nOK\r\n");
            else if(pending=="AT+CGATT?")Serial1.inject("+CGATT: 1\r\nOK\r\n");
            else if(pending=="AT+CGPSINFO")Serial1.inject("+CGPSINFO: 5209.512719,N,02100.000000,E,271026,120000.0,100.0,0.0,0.0\r\nOK\r\n");
            else if(pending=="AT+HTTPACTION=0")Serial1.inject("OK\r\n+HTTPACTION: 0,403,0\r\n");
            else if(pending=="AT+HTTPACTION=1")Serial1.inject("OK\r\n+HTTPACTION: 1,200,0\r\n");
            else if(pending.rfind("AT+HTTPDATA=",0)==0){remaining=(unsigned)atoi(pending.c_str()+12);Serial1.inject("DOWNLOAD\r\n");}
            else Serial1.inject("OK\r\n");
            pending.clear();
        }
    }
};
int main(){
    testUs=0;testStepUs=1000;Serial1.room=64;
    getGPSInfo();
    if(testUs>2000||waitedMs){printf("FAIL: GPS blocks loop (%u us)\n",testUs);return 1;}
    testStepUs=0;Serial1.room=0;Serial1.blockedWrites=0;
    modemInitStart();modemInitTick();
    if(Serial1.blockedWrites){puts("FAIL: init blocks on full UART");return 1;}
    Serial1={};testUs=0;modemInitStart();Peer peer;
    for(int i=0;i<12000&&!modemInitDone();++i)peer.step();
    CHECK(modemInitDone());
    if(!modemInitDone())return 1;
    getGPSInfo();
    for(int i=0;i<1000;++i)peer.step();
    int32_t lat=(int32_t)((uint32_t)(uint16_t)cmd[LAT].lastValue<<16)|(uint16_t)cmd[LAT].wartosc;
    CHECK(lat>52158000&&lat<52159000);
    auto saved=cmd[LAT].wartosc;
    char nofix[]="+CGPSINFO: ,,,,,,,,";CHECK(!parseGpsLine(nofix));CHECK(cmd[LAT].wartosc==saved);
    char malformed[]="+CGPSINFO: 5260.0,N,02100.0,E,1,2,3,4,5";CHECK(!parseGpsLine(malformed));
    for(int i=0;i<30;++i){logBuffer[i].day=27;logBuffer[i].second=(uint8_t)(i*2);logBuffer[i].data[0]=(int16_t)(100+i);}
    ++logGeneration;logIndex=0;startSendLog();
    logBuffer[0].data[0]=999; // Already snapshotted first half must not change.
    for(int i=0;i<12000&&modemHttpPending();++i){getGPSInfo();peer.step();}
    CHECK(!modemHttpPending());CHECK(peer.bodies.size()==2);CHECK(logUploadErrors==0);
    if(peer.bodies.size()==2){
        CHECK(peer.bodies[0].size()==647);CHECK(peer.bodies[1].size()==647);
        CHECK(peer.bodies[0].substr(0,12)=="ABCDEFGHIJKL");
        CHECK((uint8_t)peer.bodies[0][12]==0xea);CHECK((uint8_t)peer.bodies[0][13]==7);
        CHECK((uint8_t)peer.bodies[0][19]==100);CHECK((uint8_t)peer.bodies[1][19]==115);
        for(auto& body:peer.bodies){auto crc=crc16_ccitt((const uint8_t*)body.data(),645);CHECK((uint8_t)body[645]==(crc&255));CHECK((uint8_t)body[646]==(crc>>8));}
    }
    // An old second half must never be copied after acquisition overwrites it.
    Serial1={};at={};modemOwner=OWNER_HTTP;logState=PART2;logPart=0;
    uploadGeneration=logGeneration;logIndex=16;
    auto errors=logUploadErrors;modemHttpTick();
    CHECK(logState==DONE);CHECK(logUploadErrors==errors+1);
    logState=PART2;logIndex=0;++logGeneration;modemHttpTick();
    CHECK(logState==DONE);CHECK(logUploadErrors==errors+2);
    // No response and full UART must expire, including millis() rollover.
    for(int full=0;full<2;++full){
        Serial1={};Serial1.room=full?0:64;at={};testUs=0;testMillisOffset=0xffffff80u;
        beginAT(PSTR("AT"),AT_OK,100);
        for(int i=0;i<400&&!at.done;++i){testUs+=1000;pumpAT();}
        CHECK(at.done&&!at.result);CHECK(Serial1.blockedWrites==0);
    }
    testMillisOffset=0;
    // Overflow is ignored as one complete line; a later valid response survives.
    Serial1={};at={};modemOwner=OWNER_IDLE;beginAT(PSTR("AT"));pumpAT();
    std::string oversized(180,'X');oversized+="OK\r\nOK\r\n";Serial1.inject(oversized.c_str());
    for(int i=0;i<300&&!at.done;++i){testUs+=1000;pumpAT();}
    CHECK(at.done&&at.result);
    // Continuous unsolicited traffic cannot keep the final drain alive forever.
    Serial1={};at={};beginAT(PSTR("AT"));finishAT(false);
    for(int i=0;i<300&&!at.done;++i){Serial1.inject("noise\r\n");testUs+=1000;pumpAT();}
    CHECK(at.done&&!at.result);
    // Simulate a 63-byte RX ring at 115200 baud, and 8 ms slow GUI/TX work.
    Serial1={};Serial1.rxCapacity=63;at={};testUs=0;modemOwner=OWNER_GPS;
    beginAT(PSTR("AT+CGPSINFO"),PSTR("+CGPSINFO:"),1000);
    std::string gpsReply="+CGPSINFO: 5209.512719,N,02100.000000,E,271026,120000.0,100.0,0.0,0.0\r\nOK\r\n";
    size_t delivered=0;uint32_t wireStart=0;bool sent=false;
    for(int i=0;i<2000&&modemOwner==OWNER_GPS;++i){
        testUs+=modemNeedsFastService()?100:8000;
        if(at.phase==2&&!sent){sent=true;wireStart=testUs;}
        if(sent){while(delivered<gpsReply.size()&&(testUs-wireStart)>=(delivered+1)*87){char one[]={gpsReply[delivered++],0};Serial1.inject(one);}}
        Serial1.room=64;modemInitTick();
    }
    CHECK(delivered==gpsReply.size());CHECK(Serial1.droppedRx==0);
    CHECK(modemOwner!=OWNER_GPS);
    // A late DOWNLOAD must not be followed by an AT retry in its payload window.
    Serial1={};at={};modemOwner=OWNER_HTTP;logState=SEND_DATA_CMD;
    at.done=true;at.result=false;modemHttpTick();
    CHECK(modemOwner==OWNER_RECOVERY);CHECK(!modemHttpPending());
    testUs+=100000;Serial1.inject("DOWNLOAD\r\n");modemInitTick();
    auto quiet=testUs;
    for(int i=0;i<5400;++i){testUs+=1000;modemInitTick();}
    CHECK(Serial1.tx.empty());CHECK(modemOwner==OWNER_RECOVERY);
    testUs=quiet+5500000;modemInitTick();CHECK(modemOwner==OWNER_INIT);
    CHECK(waitedMs==0);
    printf("Modem failures: %d\n",failures);return failures?1:0;
}
