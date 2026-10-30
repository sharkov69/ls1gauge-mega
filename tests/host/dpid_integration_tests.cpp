#include "Arduino.h"
#define private public
#include "../../libraries/J1850-VPW-WC/j1850vpw-wc.cpp"
#undef private
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
inline long random(long a,long){return a;}
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
uint8_t pause=75,counter=0,counter2=0;
#include "../../ls1gauge-mega/cmd.h"
static int failures=0;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
void inject(uint8_t* frame,uint8_t n){frame[n-1]=crc(frame,n-1);vpw._storage.push(frame,n);read();}
int main(){
  uint8_t fe[]={0x6c,0xf1,0x10,0x6a,0xfe,0x1f,0x40,128,60,80,100,0};
  uint8_t fd[]={0x6c,0xf1,0x10,0x6a,0xfd,100,200,140,120,60,100,0};
  vpw.init(3,5);dpid.state=VpwDpid::STREAM;
  inject(fe,12);inject(fd,12);
  CHECK(counter==2);CHECK(cmd[RPM].wartosc==2000);CHECK(cmd[TPS].wartosc==50);
  CHECK(cmd[MAP].wartosc==60);CHECK(cmd[SPEED].wartosc==80);CHECK(cmd[SPARK].wartosc==33);
  CHECK(cmd[O2].wartosc==1375);CHECK(cmd[O22].wartosc==1720);CHECK(cmd[BATT].wartosc==140);
  CHECK(cmd[WATER].wartosc==80);CHECK(cmd[AIR].wartosc==20);CHECK(cmd[TRANS].wartosc==60);
  int16_t expected[11];for(int i=0;i<11;++i){expected[i]=cmd[i].wartosc;cmd[i].wartosc=-32768;}
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
  for(int i=0;i<11;++i)CHECK(cmd[i].wartosc==expected[i]);
  dpid.state=VpwDpid::STREAM;fd[5]=200;
  for(int i=0;i<4;++i)inject(fd,12);
  CHECK(cmd[O2].wartosc==1720);CHECK(cmd[O22].wartosc==1720);
  fe[5]=0;fe[6]=0;fe[2]=0x11;inject(fe,12);CHECK(cmd[RPM].wartosc==2000);
  fe[2]=0x10;inject(fe,11);CHECK(cmd[RPM].wartosc==2000);
  dpid=VpwDpid();dpid.ecuSeen();lastSend=0;lastRxEnd=101;czas=100;testUs=101000;
  testStepUs=1;pins[3]=HIGH;auto oldTx=counter2;send();testStepUs=0;
  CHECK(counter2==oldTx); // Fresh RX must not look 65535 ms old after clock advance.
  printf("DPID integration failures: %d\n",failures);return failures?1:0;
}
