#pragma once
#include <ctime>
class TimeSpan {
public:
    int seconds;
    explicit TimeSpan(int value) : seconds(value) {}
};
class DateTime {
    uint32_t epoch;
    bool valid = true;
    std::tm parts() const {
        time_t value = epoch;
        std::tm result = {};
        gmtime_s(&result, &value);
        return result;
    }
public:
    DateTime(uint32_t value = 1793882055UL) : epoch(value) {}
    DateTime(uint16_t y, uint8_t m, uint8_t d, uint8_t h=0, uint8_t mi=0, uint8_t s=0) {
        std::tm value = {};
        value.tm_year=y-1900; value.tm_mon=m-1; value.tm_mday=d;
        value.tm_hour=h; value.tm_min=mi; value.tm_sec=s;
        epoch=(uint32_t)_mkgmtime(&value);
        valid=year()==y && month()==m && day()==d && hour()==h && minute()==mi && second()==s;
    }
    int year() const { return parts().tm_year+1900; }
    int month() const { return parts().tm_mon+1; }
    int day() const { return parts().tm_mday; }
    int dayOfTheWeek() const { return parts().tm_wday; }
    int hour() const { return parts().tm_hour; }
    int minute() const { return parts().tm_min; }
    int second() const { return parts().tm_sec; }
    bool isValid() const { return valid; }
    uint32_t unixtime() const { return epoch; }
    DateTime operator+(TimeSpan span) const { return DateTime(epoch+span.seconds); }
    DateTime operator-(TimeSpan span) const { return DateTime(epoch-span.seconds); }
};
class RTC_DS3231 {
public:
    DateTime value;
    unsigned adjustments = 0;
    DateTime now() const { return value; }
    void adjust(DateTime date) { value=date; ++adjustments; }
};
