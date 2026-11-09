#include "Arduino.h"
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
int freeRam(){return 2700;}
#include "../../ls1gauge-mega/modem.h"
bool csvAtByte(uint8_t) { return false; }
bool csvAtLine(const char*) { return false; }
bool csvFastService() { return false; }
uint32_t csvRetryAfter=0;
bool parseModemClock(const char*,uint32_t&) { return false; }
bool syncRtcUtc(uint32_t) { return false; }
void modemHttpTick() {}
bool modemHttpPending() { return false; }
void cancelLogSend() {}
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
        Serial.room=(testUs/1000)%3==0?0:63; // Slow/disconnected USB must not affect modem.
        modemInitTick();
        CHECK(Serial1.blockedWrites==0);
        CHECK(Serial.blockedWrites==0);
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
    modemTimeNext=1000000;
#if DEBUG_MODEM
    debugBreadcrumb.magic=0;
    debugBootBegin(0);CHECK(debugPrevious.magic==0);
    debugStage(21);modemOwner=OWNER_HTTP;at.phase=2;
    debugWatchdogExpired();debugStage(26);debugBootBegin(0);
    CHECK(debugPrevious.magic==0x59d1&&debugPrevious.watchdog==1);
    CHECK(debugPrevious.stage==21&&debugPrevious.owner==OWNER_HTTP&&debugPrevious.phase==2);
    CHECK(debugBreadcrumb.watchdog==0&&debugBreadcrumb.stage==1);
    Serial={};Serial.room=0;modemDebugEnabled=true;debugBootReport();
    CHECK(debugBootPending&&Serial.tx.empty()&&Serial.blockedWrites==0);
    Serial.room=63;debugBootReport();
    CHECK(!debugBootPending&&Serial.tx.find("wdt=1 stage=21 owner=5 phase=2")!=std::string::npos);
    debugStage(18);debugBootBegin(0);
    CHECK(debugPrevious.watchdog==0&&debugPrevious.stage==18);
    debugWatchdogExpired();debugBootBegin(1);
    CHECK(debugPrevious.magic==0&&debugPrevious.watchdog==0);
    modemDebugEnabled=false;Serial={};debugBootReport();
    CHECK(Serial.tx.empty());debugBootPending=false;
    modemOwner=OWNER_INIT;at={};
    EEPROM.reset();modemDebugLoad();CHECK(!modemDebugEnabled);
    Serial={};modemDebugRecord('T','E',"disabled");CHECK(Serial.tx.empty());
    CHECK(!modemDebugCommand("DEBUG ON rubbish"));
    fakeEepromReady=false;CHECK(modemDebugCommand("DEBUG ON"));
    modemDebugControlTick();CHECK(modemDebugEnabled&&EEPROM.writes==0&&EEPROM.blocked==0);
    fakeEepromReady=true;Serial.room=0;modemDebugControlTick();
    CHECK(EEPROM.writes==1&&EEPROM.bytes[18]==0xa5);CHECK(Serial.tx.empty());
    modemDebugControlTick();CHECK(EEPROM.blocked==0); // Don't poll EEPROM by blocking reads.
    fakeEepromReady=true;modemDebugControlTick();CHECK(Serial.tx.empty());
    Serial.room=63;modemDebugControlTick();CHECK(Serial.tx=="DEBUG ON SAVED\n");
    Serial={};modemDebugCommand("DEBUG ON");modemDebugControlTick();CHECK(EEPROM.writes==1);
    modemDebugEnabled=false;modemDebugLoad();CHECK(modemDebugEnabled); // Boot persistence.
    Serial={};modemDebugCommand("DEBUG OFF");CHECK(!modemDebugEnabled);
    modemDebugRecord('T','E',"disabled");CHECK(Serial.tx.empty());
    modemDebugControlTick();CHECK(EEPROM.writes==2);fakeEepromReady=true;
    modemDebugControlTick();CHECK(Serial.tx=="DEBUG OFF SAVED\n");
    modemDebugEnabled=true;modemDebugLoad();CHECK(!modemDebugEnabled);
    Serial={};modemDebugCommand("DEBUG STATUS");modemDebugControlTick();
    CHECK(Serial.tx=="DEBUG OFF SAVED\n"&&EEPROM.writes==2);
    Serial={};modemDebugCommand("DEBUG ON");modemDebugControlTick();
    modemDebugCommand("DEBUG OFF");fakeEepromReady=true;modemDebugControlTick();
    fakeEepromReady=true;modemDebugControlTick();CHECK(EEPROM.bytes[18]==0x5a&&!modemDebugEnabled);
    CHECK(EEPROM.blocked==0&&Serial.blockedWrites==0);
    CHECK(EEPROM.bytes[17]==255&&EEPROM.bytes[19]==255);
    Serial={};modemDebugCommand("DEBUG ON");modemDebugControlTick();
    EEPROM.bytes[18]=0x5a;fakeEepromReady=true;modemDebugControlTick();
    CHECK(Serial.tx=="DEBUG SAVE ERROR\n"); // Never acknowledge a failed read-back as SAVED.
    Serial={};modemDebugCommand("DEBUG OFF");modemDebugControlTick();
    modemDebugEnabled=true; // Remaining suite exercises enabled diagnostics.
    testUs=0;testStepUs=0;Serial={};Serial.room=0;
    modemDebugRecord('T','E',"test");
    CHECK(Serial.tx.empty()&&Serial.blockedWrites==0&&modemDebugDropped==1);
    Serial.room=63;modemDebugRecord('T','E',"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    CHECK(Serial.tx.size()<=63&&Serial.tx.back()=='\n');CHECK(modemDebugTruncated==1);
    Serial={};Serial.room=63;
    for(char c:std::string("+CPIN: READY\r\n"))modemDebugRx(c,false);
    CHECK(Serial.tx.find("RX +CPIN: READY")!=std::string::npos);
    Serial={};Serial.room=63;modemDebugMuted=true;
    for(char c:std::string("ABCDEFGHIJKL\r\n"))modemDebugRx(c,false);
    CHECK(Serial.tx.empty());modemDebugMuted=false;
    Serial={};
#endif
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
    int32_t lat=(int32_t)((uint32_t)(uint16_t)gpsHigh[LAT - LAT]<<16)|(uint16_t)channels[LAT].value;
    CHECK(lat>52158000&&lat<52159000);
    auto saved=channels[LAT].value;
    char nofix[]="+CGPSINFO: ,,,,,,,,";CHECK(!parseGpsLine(nofix));CHECK(channels[LAT].value==saved);
    char malformed[]="+CGPSINFO: 5260.0,N,02100.0,E,1,2,3,4,5";CHECK(!parseGpsLine(malformed));
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
    CHECK(waitedMs==0);
#if DEBUG_MODEM
    CHECK(Serial.blockedWrites==0);
    CHECK(Serial.tx.find("ABCDEFGHIJKL")==std::string::npos);
    CHECK(Serial.tx.find("CQ AT+CPIN?")!=std::string::npos);
    CHECK(modemDebugDropped>0);
    // Full USB during timeout still produces a finished transaction.
    Serial.room=0;Serial1={};at={};testStepUs=0;testUs=0;testMillisOffset=0;
    beginAT(PSTR("AT"),AT_OK,100);
    for(int i=0;i<400&&!at.done;++i){testUs+=1000;pumpAT();}
    CHECK(at.done&&!at.result&&Serial.blockedWrites==0);
#endif
    printf("Modem failures: %d\n",failures);return failures?1:0;
}
