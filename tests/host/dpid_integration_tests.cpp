#include "gauge_fixture.h"
static int failures=0;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
void inject(uint8_t* frame,uint8_t n){frame[n-1]=crc(frame,n-1);vpw._storage.push(frame,n);read();}
int main(){
  CHECK(CHANNEL_COUNT == 27 && BASE_CHANNEL_COUNT == 17);
  channels[INJ1].setRaw(65535, 0xfffffff0u);
  CHECK(channels[INJ1].raw() == 65535);
  CHECK(channels[INJ1].fresh(0x10u, 32));
  CHECK(!channels[INJ1].fresh(0x11u, 32));
  channels[SA].set(-10);
  CHECK(channels[SA].value == -10 && channels[SA].valid);
  uint8_t fe[]={0x6c,0xf1,0x10,0x6a,0xfe,0x1f,0x40,128,60,80,100,0};
  uint8_t fd[]={0x6c,0xf1,0x10,0x6a,0xfd,100,200,140,120,60,100,0};
  uint8_t fc[]={0x6c,0xf1,0x10,0x6a,0xfc,0x12,0x34,0xab,0xcd,108,8,0};
  uint8_t fb[]={0x6c,0xf1,0x10,0x6a,0xfb,128,7,11,3,91,35,0};
  vpw.init(3,5);dpid.state=VpwDpid::STREAM;
  inject(fe,12);inject(fd,12);
  CHECK(counter==11);CHECK(channels[RPM].value==2000);CHECK(channels[TPS].value==50);
  CHECK(channels[MAP].value==60);CHECK(channels[SPEED].value==80);CHECK(channels[OIL].value==33);
  CHECK(channels[O2].value==1375);CHECK(channels[O22].value==1720);CHECK(channels[BATT].value==140);
  CHECK(channels[WATER].value==80);CHECK(channels[AIR].value==20);CHECK(channels[TRANS].value==60);
  inject(fc,12);inject(fb,12);CHECK(counter==21);
  CHECK(channels[TCC].flagActive(0x80,millis()));
  uint8_t tccReply[]={0x6c,0xf1,0x10,0x62,0x19,0x00,0x7f,0};
  decodeGaugeReply(tccReply,sizeof(tccReply));
  CHECK(!channels[TCC].flagActive(0x80,millis()));
  tccReply[6]=0x80;decodeGaugeReply(tccReply,sizeof(tccReply));
  CHECK(channels[TCC].flagActive(0x80,millis()));
  CHECK(channels[INJ1].raw()==0x1234&&channels[INJ2].raw()==0xabcd);
  CHECK(((int16_t)channels[SA].raw() - 128)==-20);CHECK(channels[PRNDL].raw()==8);
  CHECK(channels[FAN_STATE].flagActive(1,millis())&&channels[FAN_STATE].flagActive(2,millis()));
  CHECK(channels[AC_STATE].flagActive(8,millis())&&channels[CL_STATE].flagActive(32,millis()));
  CHECK(!channels[FAN_STATE].flagActive(1,millis()+501));
  CHECK(channels[FAN_STATE].flagActive(1,millis()+2500,3000)); // Ordinary fallback flags span a full poll round.
  CHECK(!channels[FAN_STATE].flagActive(1,millis()+3001,3000));
  channels[INJ1].value=(int16_t)19;channels[INJ2].value=(int16_t)19;CHECK(fuelCut(millis()));
  channels[INJ2].value=(int16_t)20;CHECK(!fuelCut(millis()));
  const uint8_t directPw[]={0x6c,0xf1,0x10,0x62,0x12,0x5b,0xff,0xff,0};
  CHECK(!validGaugeReply(directPw,8));CHECK(validGaugeReply(directPw,9));
  decodeGaugeReply(directPw,9);CHECK(channels[INJ2].raw()==65535);
  int16_t expected[11];for(int i=0;i<11;++i){expected[i]=channels[i].value;channels[i].value=-32768;}
  // Direct replies and the two DPID packets must yield the same eleven values.
  dpid.state=VpwDpid::LEGACY;o2[0]={};o2[1]={};
  const uint16_t pids[]={0xc,0x11,0xb,0xd,0x115c,0x114b,0x114e,0x1141,5,0xf,0x1940};
  const uint8_t raw[][2]={{0x1f,0x40},{128,0},{60,0},{80,0},{100,0},{100,0},{200,0},{140,0},{120,0},{60,0},{100,0}};
  for(int i=0;i<11;++i){
    uint8_t p[8]={0x6c,0xf1,0x10,0,0,0,0,0};uint8_t n;
    if(pids[i]>255){p[3]=0x62;p[4]=(uint8_t)(pids[i]>>8);p[5]=(uint8_t)pids[i];p[6]=raw[i][0];n=8;}
    else {p[3]=0x41;p[4]=(uint8_t)pids[i];p[5]=raw[i][0];p[6]=raw[i][1];n=i==0?8:7;}
    inject(p,n);
  }
  for(int i=0;i<11;++i)CHECK(channels[i].value==expected[i]);
  dpid.state=VpwDpid::STREAM;fd[5]=200;
  for(int i=0;i<4;++i)inject(fd,12);
  CHECK(channels[O2].value==1720);CHECK(channels[O22].value==1720);
  fe[5]=0;fe[6]=0;fe[2]=0x11;inject(fe,12);CHECK(channels[RPM].value==2000);
  fe[2]=0x10;inject(fe,11);CHECK(channels[RPM].value==2000);
  dpid=VpwDpid();dpid.ecuSeen();lastSend=0;lastRxEnd=101;czas=100;testUs=101000;
  testStepUs=1;pins[3]=HIGH;auto previousSend=lastSend;send();testStepUs=0;
  CHECK(lastSend==previousSend); // Fresh RX must not look 65535 ms old after clock advance.
  counter=0;dpid.state=VpwDpid::STREAM;
  fe[2]=0x10;
  for(int i=0;i<300;++i){inject(fe,12);inject(fd,12);}
  CHECK(counter==3300); // 5 Hz * 60 seconds * eleven decoded channel samples.
  setVpwListenOnly(true);
  CHECK(passiveDpid.found()==0);
  const auto oldSend=lastSend;
  uint8_t tx[]={0x6c,0x10,0xf1,0x3f};
  CHECK(!transmitVpw(tx,4)); send(); CHECK(lastSend==oldSend);
  // Every current channel, reordered and spread over 21 different packets.
  for(uint8_t i=0;i<DPID_FIELD_COUNT;++i){
    const auto f=dpidField(i);
    const uint8_t packet=0x80+i, selector=0x40|(3<<3)|f.length;
    uint8_t def[]={0x6c,0x10,0xf2,0x2c,packet,selector,(uint8_t)(f.pid>>8),(uint8_t)f.pid,0xff,0xff,0};
    inject(def,sizeof(def));
    uint8_t data[]={0x6c,0xf2,0x10,0x6a,packet,0,0,80,80,0,0,0};
    auto before=counter; inject(data,sizeof(data)); CHECK(counter==before);
    uint8_t ack[]={0x6c,0xf2,0x10,0x6c,packet,selector,0};
    inject(ack,sizeof(ack)); inject(data,sizeof(data)); CHECK(counter==before);
    const uint8_t group = 0x14 + (i % 4) * 0x10;
    uint8_t start[]={0x6c,0x10,0xf2,0x2a,group,packet,0};
    inject(start,sizeof(start));
    uint8_t ready[]={0x6c,0xf2,0x10,0x7f,0x2a,group,packet,0x23,0};
    inject(ready,sizeof(ready)); inject(data,sizeof(data));
    CHECK(counter==before+1); CHECK(passiveDpid.found()==i+1);
  }
  // Redefining a slot as an unknown PID must discard its former meaning.
  uint8_t unknown[]={0x6c,0x10,0xf2,0x2c,0x80,0x5a,0x7e,0x77,0xff,0xff,0};
  inject(unknown,sizeof(unknown)); CHECK(passiveDpid.found()==20);
  uint8_t stop[]={0x6c,0x10,0xf2,0x2a,0,0};
  inject(stop,sizeof(stop)); CHECK(passiveDpid.found()==0);
  // Multiple definitions in one packet must survive subsequent definitions.
  uint8_t rpmDef[]={0x6c,0x10,0xf2,0x2c,0xfe,0x4a,0,0x0c,0xff,0xff,0};
  uint8_t rpmAck[]={0x6c,0xf2,0x10,0x6c,0xfe,0x4a,0};
  inject(rpmDef,sizeof(rpmDef));inject(rpmAck,sizeof(rpmAck));
  uint8_t tpsDef[]={0x6c,0x10,0xf2,0x2c,0xfe,0x59,0,0x11,0xff,0xff,0};
  uint8_t tpsAck[]={0x6c,0xf2,0x10,0x6c,0xfe,0x59,0};
  inject(tpsDef,sizeof(tpsDef));inject(tpsAck,sizeof(tpsAck));
  CHECK(passiveDpid.slot[0]==0x4a && passiveDpid.slot[1]==0x59);
  // Wrong tester and late ACK cannot install a pending definition.
  setVpwListenOnly(true);inject(rpmDef,sizeof(rpmDef));
  rpmAck[1]=0xf1;inject(rpmAck,sizeof(rpmAck));CHECK(passiveDpid.slot[0]==0);
  rpmAck[1]=0xf2;testMillisOffset+=2001;inject(rpmAck,sizeof(rpmAck));
  CHECK(passiveDpid.slot[0]==0);
  setVpwListenOnly(false);CHECK(!vpwListenOnly && dpid.state==VpwDpid::WAIT_ECU);
  printf("DPID integration failures: %d\n",failures);return failures?1:0;
}
