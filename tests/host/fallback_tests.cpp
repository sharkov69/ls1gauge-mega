#include "Arduino.h"
#include <vector>

struct Transmission {
    uint32_t at;
    uint16_t pid;
};
std::vector<Transmission> transmissions;
bool transmitOk = true;
constexpr uint8_t BS = 13;
class J1850VPW {
public:
    bool send(uint8_t* bytes, uint8_t size, int) {
        uint16_t pid = size == 5 ? bytes[4] : (uint16_t)(bytes[4] << 8) | bytes[5];
        transmissions.push_back({millis(), pid});
        return transmitOk;
    }
    uint8_t tryGetReceivedFrame(uint8_t*) { return 0; }
};
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
inline long random(long a, long) { return a; }
uint8_t pause = 75;
uint16_t counter = 0;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#include "../../ls1gauge-mega/cmd.h"
#undef min
#undef max

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

// Explicit legacy order followed by additional channels, excluding local sensors/GPS.
const uint8_t ids[] = {AIR, MAP, TPS, BATT, O22, SPEED, TRANS, OIL, RPM, WATER, O2,
    INJ1, INJ2, SA, PRNDL, TCC, SHIFT_MODE, AC_STATE, FAN_STATE, KR, CL_STATE};
const uint16_t intervals[] = {5000,250,250,5000,250,250,5000,5000,250,5000,250,
    1000,1000,250,1000,1000,1000,2000,2000,250,2000};

void reset(uint32_t now) {
    testUs = 0;
    testMillisOffset = now;
    dpid = VpwDpid();
    dpid.state = VpwDpid::LEGACY;
    lastSend = (uint16_t)(now - pause);
    lastRxEnd = (uint16_t)(now - 10);
    for (uint8_t i=0; i<POLL_COUNT; ++i) lastRequestAt[i] = (uint16_t)now;
    transmitOk = true;
    transmissions.clear();
}

int main() {
    CHECK(POLL_COUNT == 21);
    if (POLL_COUNT != 21) return 1;
    for (uint8_t i=0; i<21; ++i) CHECK(fieldForChannel(ids[i]).intervalMs == intervals[i]);
    for (uint32_t now : {10000u, 65836u, 0xffffff00u}) {
        for (uint8_t i=0; i<21; ++i) {
            reset(now);
            lastRequestAt[i] = (uint16_t)(now - intervals[i] + 1);
            send();
            CHECK(transmissions.empty()); // This PID is not due yet.
            lastRequestAt[i] = (uint16_t)(now - intervals[i]);
            send();
            CHECK(transmissions.size() == 1);
            if (!transmissions.empty()) CHECK(transmissions.back().pid == fieldForChannel(ids[i]).pid);
            CHECK(lastRequestAt[i] == (uint16_t)now);
        }
    }
    reset(10000);
    lastRequestAt[8] = 10000 - intervals[8] - 100; // RPM overdue 100 ms.
    lastRequestAt[19] = 10000 - intervals[19] - 200; // KR overdue 200 ms.
    send();
    CHECK(transmissions.size() == 1 && transmissions.back().pid == 0x11a6);

    reset(10000);
    lastRequestAt[11] = 8000;
    transmitOk = false;
    send();
    CHECK(lastRequestAt[11] == 8000); // Failed transmission cannot consume the deadline.
    reset(10000);
    lastRequestAt[11] = 8000;
    lastSend = 9990;
    send();
    CHECK(transmissions.empty());
    lastSend = 9900;
    lastRxEnd = 9995;
    send();
    CHECK(transmissions.empty());

    reset(10000);
    for (uint32_t elapsed=0; elapsed<120000; elapsed+=5) {
        testMillisOffset = 10000 + elapsed;
        send();
    }
    unsigned counts[21] = {};
    uint32_t seen[21] = {};
    for (const auto& tx : transmissions) {
        for (uint8_t i=0; i<21; ++i) {
            if (tx.pid != fieldForChannel(ids[i]).pid) continue;
            if (counts[i]) CHECK(tx.at - seen[i] >= intervals[i]);
            seen[i] = tx.at;
            ++counts[i];
        }
    }
    for (uint8_t i=0; i<21; ++i) CHECK(counts[i] > 10);
    CHECK(counts[19] > counts[11] && counts[11] > counts[17]); // KR > INJ1 > A/C.
    printf("Fallback failures: %d\n", failures);
    return failures ? 1 : 0;
}
