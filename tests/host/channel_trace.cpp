#include "Arduino.h"
#include <vector>
std::vector<uint8_t> output;
void emit16(uint16_t v) { output.push_back((uint8_t)v); output.push_back((uint8_t)(v>>8)); }
constexpr uint8_t BS=13;
class J1850VPW {
public:
    bool send(uint8_t* data,uint8_t size,int) {
        emit16((uint16_t)millis());output.push_back(size);
        output.insert(output.end(),data,data+size);return true;
    }
    uint8_t tryGetReceivedFrame(uint8_t*) { return 0; }
};
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
inline long random(long a,long) { return a; }
uint8_t pause=75;
uint16_t counter=0;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#include "../../ls1gauge-mega/cmd.h"
int main() {
    // Fallback timing at supported pause settings, including 16-bit wraparound.
    dpid.state=VpwDpid::LEGACY;
    for (int setting : {25,75,125}) {
        pause=setting;
        for (uint32_t t=0;t<140000;t+=5) { testUs=t*1000;send(); }
    }
    // Full one-byte domain, including unsigned injector values above INT16_MAX.
    for (int v=0;v<256;++v) {
        uint8_t payload[6];memset(payload,v,sizeof(payload));
        for(uint8_t packet=0;packet<4;++packet)decodeDpidPacket(packet,payload);
#ifdef BEFORE_REFACTOR
        for (int i=0;i<11;++i)emit16((uint16_t)cmd[i].wartosc);
        for (int i=0;i<10;++i)emit16(extra.raw[i]);
#else
        for (int i=0;i<11;++i)emit16(channels[i].raw());
        for (int i=BASE_CHANNEL_COUNT;i<CHANNEL_COUNT;++i)emit16(channels[i].raw());
#endif
    }
    FILE* file=fopen(TRACE_FILE,"wb");if(!file)return 2;
    fwrite(output.data(),1,output.size(),file);fclose(file);
}
