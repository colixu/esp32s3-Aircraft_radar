#include "TimeManager.h"

#include <time.h>

#include "DebugLog.h"

bool TimeManager::begin(const UserSettings &settings)
{
    timezoneOffsetMinutes_ = settings.schedule.timezoneOffsetMinutes;
    timeSynced_ = false;
    lastCheckMs_ = 0;

    configTime(0, 0, "pool.ntp.org", "time.google.com");
    started_ = true;
    DebugLog::println("TimeManager: NTP sync started.");

    update();
    return true;
}

void TimeManager::update()
{
    const uint32_t nowMs = millis();
    if (lastCheckMs_ != 0 && nowMs - lastCheckMs_ < 1000)
    {
        return;
    }
    lastCheckMs_ = nowMs;

    time_t now = 0;
    time(&now);
    const bool synced = now > 1700000000;
    if (synced && !timeSynced_)
    {
        DebugLog::printf("TimeManager: NTP synced. unix=%lu\r\n", static_cast<unsigned long>(now));
    }
    timeSynced_ = synced;
}

bool TimeManager::isTimeSynced() const
{
    return timeSynced_;
}

uint32_t TimeManager::getUnixTime() const
{
    time_t now = 0;
    time(&now);
    return static_cast<uint32_t>(now);
}

int16_t TimeManager::getLocalMinutesOfDay() const
{
    if (!timeSynced_)
    {
        return -1;
    }

    int32_t localSeconds = static_cast<int32_t>(getUnixTime()) +
                           static_cast<int32_t>(timezoneOffsetMinutes_) * 60;
    localSeconds %= 24L * 3600L;
    if (localSeconds < 0)
    {
        localSeconds += 24L * 3600L;
    }

    return static_cast<int16_t>(localSeconds / 60);
}

void TimeManager::formatLocalTime(char *buffer, size_t bufferSize) const
{
    if (buffer == nullptr || bufferSize == 0)
    {
        return;
    }

    const int16_t minutes = getLocalMinutesOfDay();
    if (minutes < 0)
    {
        snprintf(buffer, bufferSize, "--:--");
        return;
    }

    snprintf(buffer, bufferSize, "%02d:%02d", minutes / 60, minutes % 60);
}
