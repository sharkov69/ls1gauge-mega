#include "Arduino.h"
#define private public
#include "../../libraries/J1850-VPW-WC/j1850vpw-wc.cpp"
#undef private
#include "../../ls1gauge-mega/vpw_reply.h"
static int failures=0;
#define CHECK(c) do { if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;} }while(0)
// Inject transitions at the GPIO boundary, including the initial idle and SOF.
void edge(J1850VPW& v,uint32_t duration,int level) {testUs+=duration;pins[3]=level;v.onRxChanged();}
void frame(J1850VPW& v,const uint8_t* bytes,size_t n) {
    edge(v,300,LOW);edge(v,200,HIGH);
    bool passive=true;
    for(size_t i=0;i<n;++i)for(int bit=7;bit>=0;--bit){
        bool one=(bytes[i]&(1<<bit))!=0;
        edge(v,passive?(one?128:64):(one?64:128),passive?LOW:HIGH);
        passive=!passive;
    }
}
int main(){
    uint8_t good[]={0x48,0x6b,0x10,0x41,0x0d,0x32,0};
    good[6]=crc(good,6); // CRC primitive is not the behavior under test here.
    uint8_t bad[8]={};memcpy(bad,good,7);bad[6]^=1;
    {
        J1850VPW v;v.init(3,5);uint8_t out[BS]={};
        v._storage.push(bad,7);v._storage.push(good,7);
        CHECK(v.tryGetReceivedFrame(out)==7);
        CHECK(memcmp(out,good,7)==0);
    }
    {
        testUs=0;pins[3]=HIGH;J1850VPW v;v.init(3,5);uint8_t out[BS]={};
        frame(v,good,7);testUs+=300;
        CHECK(v.tryGetReceivedFrame(out)==7); // No subsequent frame/edge required.
        CHECK(memcmp(out,good,7)==0);
        CHECK(v.tryGetReceivedFrame(out)==0);
    }
    {
        uint8_t rpm[]={0x48,0x6b,0x10,0x41,0x0c,0x1f,0x40,0};rpm[7]=crc(rpm,7);
        testUs=0;pins[3]=HIGH;J1850VPW v;v.init(3,5);uint8_t out[BS]={};
        frame(v,rpm,8);testUs+=300;
        CHECK(v.tryGetReceivedFrame(out)==8);CHECK(memcmp(out,rpm,8)==0);
    }
    {
        Storage q;uint8_t data[8]={},out[8]={};
        for(int i=0;i<40;++i){data[0]=(uint8_t)i;q.push(data,8);}
        for(int i=8;i<40;++i){CHECK(q.tryPopItem(out)==8);CHECK(out[0]==i);}
        CHECK(q.tryPopItem(out)==0);CHECK(q.dropped==8);
        q.push(data,0);q.push(data,BS+1);CHECK(q.tryPopItem(out)==0);
    }
    {
        J1850VPW v;v.init(3,5);testStepUs=1;pins[3]=HIGH;
        CHECK(!v.send(nullptr,1,1));CHECK(!v.send(good,0,1));CHECK(!v.send(good,BS,1));
        pins[3]=LOW;CHECK(!v.send(good,6,1));CHECK(pins[5]==HIGH);
        v._sofRead=true;v._byte=2;v._bit=3;
        CHECK(!v.send(good,6,1));CHECK(v._sofRead);CHECK(v._byte==2&&v._bit==3);
        pins[3]=HIGH;CHECK(v.send(good,6,1));CHECK(pins[5]==HIGH);testStepUs=0;
    }
    {
        testUs=0xfffffff0u;pins[3]=HIGH;J1850VPW v;v.init(3,5);uint8_t out[8]={};
        frame(v,good,7);testUs+=300;CHECK(v.tryGetReceivedFrame(out)==7);
    }
    {
        uint8_t reply[]={0x48,0x6b,0x10,0x41,0x0d,50,0,0};
        CHECK(validGaugeReply(reply,7));CHECK(!validGaugeReply(reply,6));
        CHECK(!validGaugeReply(reply,8));CHECK(!validGaugeReply(nullptr,7));
        reply[2]=0x11;CHECK(!validGaugeReply(reply,7));reply[2]=0x10;
        reply[4]=0x0c;CHECK(validGaugeReply(reply,8));CHECK(!validGaugeReply(reply,7));
        reply[0]=0x6c;reply[1]=0xf1;reply[3]=0x62;reply[4]=0x11;reply[5]=0x41;
        CHECK(validGaugeReply(reply,8));CHECK(!validGaugeReply(reply,7));
        reply[5]=0x42;CHECK(!validGaugeReply(reply,8));
        reply[5]=0x41;reply[1]=0xf2;CHECK(!validGaugeReply(reply,8));
    }
    for(uint8_t n=12;n<=13;++n){
        uint8_t packet[14]={0x6c,0xf1,0x10,0x6a,0xfe,0x1f,0x40,0x80,0x30,0x60,0x22,0};
        packet[n-1]=crc(packet,n-1);
        testUs=0;pins[3]=HIGH;J1850VPW v;v.init(3,5);uint8_t out[14]={};
        frame(v,packet,n);testUs+=300;
        CHECK(v.tryGetReceivedFrame(out)==n);CHECK(memcmp(out,packet,n)==0);
    }
    printf("VPW failures: %d\n",failures);return failures?1:0;
}
