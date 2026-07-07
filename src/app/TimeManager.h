#pragma once

#include <Arduino.h>

#include "UserSettings.h"

class TimeManager
{
public:
    bool begin(const UserSettings &settings);
    void update();

    bool isTimeSynced() const;
    int16_t getLocalMinutesOfDay() const;
    uint32_t getUnixTime() const;
    void formatLocalTime(char *buffer, size_t bufferSize) const;

private:
    int16_t timezoneOffsetMinutes_ = 0;
    bool started_ = false;
    bool timeSynced_ = false;
    uint32_t lastCheckMs_ = 0;
};
