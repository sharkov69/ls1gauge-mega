#include "Arduino.h"
#include <vector>
#include "../../ls1gauge-mega/vpw_dpid.h"
static int failures=0;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
static bool busFree=true;
static std::vector<uint8_t> sent;
bool tx(uint8_t* p,uint8_t n){if(!busFree)return false;sent.assign(p,p+n);return true;}
void reply(VpwDpid& d,std::initializer_list<uint8_t> p,uint32_t now){auto b=std::vector<uint8_t>(p);d.receive(b.data(),(uint8_t)b.size(),now);}
void configure(VpwDpid& d,uint32_t now){
  d.ecuSeen();d.tick(now,true,tx);
  CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x2a,0}));
  reply(d,{0x6c,0xf1,0x10,0x6a,0,0},now);
  const uint8_t definitions[11][7]={
    {0x2c,0xfe,0x4a,0x00,0x0c,0xff,0xff},
    {0x2c,0xfe,0x59,0x00,0x11,0xff,0xff},
    {0x2c,0xfe,0x61,0x00,0x0b,0xff,0xff},
    {0x2c,0xfe,0x69,0x00,0x0d,0xff,0xff},
    {0x2c,0xfe,0x71,0x11,0x5c,0xff,0xff},
    {0x2c,0xfd,0x49,0x11,0x4b,0xff,0xff},
    {0x2c,0xfd,0x51,0x11,0x4e,0xff,0xff},
    {0x2c,0xfd,0x59,0x11,0x41,0xff,0xff},
    {0x2c,0xfd,0x61,0x00,0x05,0xff,0xff},
    {0x2c,0xfd,0x69,0x00,0x0f,0xff,0xff},
    {0x2c,0xfd,0x71,0x19,0x40,0xff,0xff}};
  for(int i=0;i<11;++i){
    d.tick(now,true,tx);CHECK(sent.size()==10);
    CHECK(memcmp(sent.data()+3,definitions[i],7)==0);
    // A different selector, ECU or length must not advance the definition.
    uint8_t ack[]={0x6c,0xf1,0x11,0x6c,definitions[i][1],definitions[i][2],0};
    d.receive(ack,7,now);CHECK(d.definitionIndex()==i);
    ack[2]=0x10;d.receive(ack,6,now);CHECK(d.definitionIndex()==i);
    ack[5]^=8;d.receive(ack,7,now);CHECK(d.definitionIndex()==i);ack[5]^=8;
    d.receive(ack,7,now);CHECK(d.definitionIndex()==i+1);
  }
  d.tick(now,true,tx);
  CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x2a,0x14,0xfe,0xfd,0xfe,0xfd}));
  reply(d,{0x6c,0xf1,0x10,0x7f,0x2a,0x14,0xfe,0xfd,0xfe,0xfd,0x23,0},now);
  CHECK(!d.active());
}
int main(){
  VpwDpid d;CHECK(d.directPolling());configure(d,100);
  uint8_t fe[]={0x6c,0xf1,0x10,0x6a,0xfe,0x1f,0x40,0x80,0x30,0x60,0x22,0};
  CHECK(d.receive(fe,11,200)==-1);CHECK(!d.active());
  CHECK(d.receive(fe,12,200)==0);CHECK(!d.active());
  fe[4]=0xfd;CHECK(d.receive(fe,12,210)==1);CHECK(d.active());CHECK(!d.directPolling());
  sent.clear();d.tick(2210,true,tx);CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x3f}));
  // Losing just FE stops both streams; no direct polling before confirmed STOP.
  d.receive(fe,12,3000);d.tick(3201,true,tx);CHECK(!d.active());CHECK(!d.directPolling());
  CHECK(sent==std::vector<uint8_t>({0x6c,0x10,0xf1,0x2a,0}));
  d.tick(6000,true,tx);CHECK(!d.directPolling());
  reply(d,{0x6c,0xf1,0x10,0x6a,0,0},6001);CHECK(d.directPolling());
  CHECK(d.receive(fe,12,6100)==-1);
  CHECK(!d.directPolling()); // Late traffic must be stopped, not mixed with polling.
  // Busy TX is not a completed request and cannot consume an ACK or timeout.
  VpwDpid busy;busy.ecuSeen();busFree=false;busy.tick(0,true,tx);busy.tick(10000,true,tx);
  CHECK(!busy.directPolling());busFree=true;busy.tick(10001,true,tx);
  reply(busy,{0x6c,0xf1,0x10,0x6a,0,0},10002);
  busy.tick(10003,true,tx);
  reply(busy,{0x6c,0xf1,0x10,0x7f,0x2c,0xfe,0x4a,0,0x0c,0xff,0xff,0x31,0},10004);
  CHECK(busy.directPolling());
  // Timeout while starting may have left a running ECU stream.
  VpwDpid timeout;configure(timeout,0xfffffff0u);
  timeout.tick(0x800,true,tx);CHECK(!timeout.directPolling());
  reply(timeout,{0x6c,0xf1,0x10,0x6a,0,0},0x801);CHECK(timeout.directPolling());
  VpwDpid noecu;noecu.ecuSeen();noecu.tick(0,true,tx);noecu.tick(2001,false,tx);
  CHECK(noecu.directPolling()); // No stream observed and no START ever sent.
  noecu.receive(fe,12,2002);CHECK(!noecu.directPolling());
  printf("DPID failures: %d\n",failures);return failures?1:0;
}
