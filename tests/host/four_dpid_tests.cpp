#include "Arduino.h"
#include <vector>
#include "gauge_fixture.h"
static int failures=0;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
static std::vector<uint8_t> sent;
static bool busFree=true;
bool tx(uint8_t* p,uint8_t n){if(!busFree)return false;sent.assign(p,p+n);return true;}
void reply(VpwDpid& d,std::initializer_list<uint8_t> p,uint32_t now){auto b=std::vector<uint8_t>(p);d.receive(b.data(),(uint8_t)b.size(),now);}
void startFirstGroup(VpwDpid& d,uint32_t now){
 d.ecuSeen();d.tick(now,true,tx);reply(d,{0x6c,0xf1,0x10,0x6a,0,0},now);
 for(uint8_t i=0;i<21;++i){d.tick(now,true,tx);reply(d,{0x6c,0xf1,0x10,0x6c,sent[4],sent[5],0},now);}
 d.tick(now,true,tx);
}
int main(){
 CHECK(sizeof(DPID_FIELDS)/sizeof(DPID_FIELDS[0])==21);
 if(failures)return 1;
 VpwDpid d;d.ecuSeen();d.tick(100,true,tx);reply(d,{0x6c,0xf1,0x10,0x6a,0,0},100);
 const uint16_t ids[]={0xc,0x11,0xb,0xd,0x115c,0x114b,0x114e,0x1141,5,15,0x1940,0x125a,0x125b,14,0x1113,0x1900,0x1973,0x1100,0x1106,0x11a6,0x1105};
 const uint8_t selectors[]={0x4a,0x59,0x61,0x69,0x71,0x49,0x51,0x59,0x61,0x69,0x71,0x4a,0x5a,0x69,0x71,0x49,0x51,0x59,0x61,0x69,0x71};
 for(uint8_t i=0;i<21;++i){
  d.tick(100,true,tx);uint8_t id=i<5?0xfe:i<11?0xfd:i<15?0xfc:0xfb;
  CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x2c,id,selectors[i],(uint8_t)(ids[i]>>8),(uint8_t)ids[i],255,255}));
  reply(d,{0x6c,0xf1,0x11,0x6c,id,selectors[i],0},100);CHECK(d.definitionIndex()==i);
  reply(d,{0x6c,0xf1,0x10,0x6c,id,(uint8_t)(selectors[i]^8),0},100);CHECK(d.definitionIndex()==i);
  reply(d,{0x6c,0xf1,0x10,0x6c,id,selectors[i]},100);CHECK(d.definitionIndex()==i);
  reply(d,{0x6c,0xf1,0x10,0x6c,id,selectors[i],0},100);CHECK(d.definitionIndex()==i+1);
 }
 d.tick(100,true,tx);CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x2a,0x14,0xfe,0xfd,0xfc,0xfb}));
 reply(d,{0x6c,0xf1,0x10,0x7f,0x2a,0x24,0xfe,0xfd,0xfc,0xfb,0x23,0},101);
 d.tick(102,true,tx);CHECK(sent[4]==0x14);
 reply(d,{0x6c,0xf1,0x10,0x7f,0x2a,0x14,0xfe,0xfd,0xfc,0xfb,0x23,0},103);
 d.tick(104,true,tx);CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x2a,0x24,0xfe,0xfd,0xfc,0xfb}));
 uint8_t frame[]={0x6c,0xf1,0x10,0x6a,0xfe,1,2,3,4,5,6,0};
 CHECK(d.receive(frame,12,105)==-1);
 reply(d,{0x6c,0xf1,0x10,0x7f,0x2a,0x24,0xfe,0xfd,0xfc,0xfb,0x23,0},106);
 for(uint8_t i=0;i<4;i++){frame[4]=0xfe-i;CHECK(d.receive(frame,12,110+i)==i);if(i<3)CHECK(!d.active());}
 CHECK(d.active());CHECK(dpidChannelCount(0)==5);CHECK(dpidChannelCount(1)==6);CHECK(dpidChannelCount(2)==4);CHECK(dpidChannelCount(3)==6);
 sent.clear();d.tick(2114,true,tx);CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x3f}));
 frame[4]=0xfb;d.receive(frame,12,3000);d.tick(3111,true,tx);CHECK(!d.active());CHECK(!d.directPolling());CHECK(sent[3]==0x2a&&sent[4]==0);
 d.tick(6000,true,tx);CHECK(!d.directPolling());reply(d,{0x6c,0xf1,0x10,0x6a,0,0},6001);CHECK(d.directPolling());
 d.receive(frame,12,6002);CHECK(!d.directPolling());
 VpwDpid busy;busy.ecuSeen();busFree=false;sent.clear();busy.tick(0,true,tx);busy.tick(5000,true,tx);CHECK(sent.empty()&&!busy.directPolling());
 busFree=true;busy.tick(5001,true,tx);reply(busy,{0x6c,0xf1,0x10,0x6a,0,0},5002);busy.tick(5003,true,tx);
 reply(busy,{0x6c,0xf1,0x10,0x7f,0x2c,0xfe,0x4a,0,0x0c,255,255,0x31,0},5004);CHECK(busy.directPolling());
 VpwDpid timeout;startFirstGroup(timeout,0xfffffff0u);timeout.tick(0x800,true,tx);CHECK(!timeout.directPolling());CHECK(sent[3]==0x2a&&sent[4]==0);
 reply(timeout,{0x6c,0xf1,0x10,0x6a,0,0},0x801);CHECK(timeout.directPolling());
 VpwDpid second;startFirstGroup(second,100);reply(second,{0x6c,0xf1,0x10,0x7f,0x2a,0x14,0xfe,0xfd,0xfc,0xfb,0x23,0},101);second.tick(102,true,tx);
 reply(second,{0x6c,0xf1,0x10,0x7f,0x2a,0x24,0xfe,0xfd,0xfc,0xfb,0x12,0},103);CHECK(!second.directPolling());second.tick(104,true,tx);CHECK(sent[4]==0);
 reply(second,{0x6c,0xf1,0x10,0x6a,0,0},105);CHECK(second.directPolling());
 // A transient setup timeout must not leave a responding ECM in fallback forever.
 VpwDpid retry;
 retry.ecuSeen(); retry.tick(100, true, tx);
 reply(retry, {0x6c,0xf1,0x10,0x6a,0,0}, 101);
 retry.tick(102, true, tx);
 reply(retry, {0x6c,0xf1,0x10,0x6c,sent[4],sent[5],0}, 102);
 CHECK(retry.definitionIndex() == 1);
 retry.tick(103, true, tx);
 retry.tick(2103, true, tx);
 CHECK(retry.directPolling());
 sent.clear(); retry.tick(40000, true, tx);
 CHECK(retry.directPolling() && sent.empty());
 testMillisOffset = 32102; retry.ecuSeen(); CHECK(retry.directPolling());
 testMillisOffset = 32103; retry.ecuSeen(); CHECK(!retry.directPolling());
 CHECK(retry.definitionIndex() == 0);
 retry.tick(32102, true, tx); CHECK(sent[3] == 0x2a && sent[4] == 0);
 startFirstGroup(retry, 32103);
 reply(retry,{0x6c,0xf1,0x10,0x7f,0x2a,0x14,0xfe,0xfd,0xfc,0xfb,0x23,0},32104);
 retry.tick(32105,true,tx);
 reply(retry,{0x6c,0xf1,0x10,0x7f,0x2a,0x24,0xfe,0xfd,0xfc,0xfb,0x23,0},32106);
 for(uint8_t i=0;i<4;++i){frame[4]=0xfe-i;retry.receive(frame,12,32107+i);}
 CHECK(retry.active());
 testMillisOffset = 0;
 printf("Four DPID failures: %d\n",failures);return failures?1:0;
}
