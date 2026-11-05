#pragma once
class TimeSpan {
public:
    explicit TimeSpan(int) {}
};
class DateTime {
public:
    int year() const { return 2026; }
    int month() const { return 11; }
    int day() const { return 5; }
    int dayOfTheWeek() const { return 4; }
    int hour() const { return 12; }
    int minute() const { return 34; }
    int second() const { return 15; }
    DateTime operator+(TimeSpan) const { return {}; }
    DateTime operator-(TimeSpan) const { return {}; }
};
class RTC_DS3231 {
public:
    DateTime now() const { return {}; }
    void adjust(DateTime) {}
};
