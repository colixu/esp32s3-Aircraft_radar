#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "UserSettings.h"

struct LongRunStatsData
{
    uint32_t version = 1;
    uint32_t bootCount = 0;
    uint32_t wifiLostCount = 0;
    uint32_t wifiReconnectCount = 0;
    uint32_t apiErrorCount = 0;
    uint32_t heapLowCount = 0;
    uint32_t totalWifiLostSeconds = 0;
    uint32_t longestWifiLostSeconds = 0;
    uint32_t maxApiConsecutiveErrors = 0;
    uint32_t currentApiConsecutiveErrors = 0;
    uint32_t minFreeHeapEver = UINT32_MAX;
    uint32_t minMaxAllocHeapEver = UINT32_MAX;
    int16_t maxTemperatureCx10 = INT16_MIN;
    uint32_t maxUptimeSeconds = 0;
    uint32_t lastBootUnixTime = 0;
    uint32_t lastEventUnixTime = 0;
    uint32_t lastEventUptimeSeconds = 0;
    int16_t lastEventTemperatureCx10 = INT16_MIN;
    int16_t lastHttpCode = 0;
    uint8_t lastResetReason = 0;
};

class LongRunStats
{
public:
    bool begin();
    void recordBoot(uint8_t resetReason, uint32_t unixTime, uint32_t uptimeSeconds, int16_t temperatureCx10);
    void recordWifiLost(uint32_t unixTime, uint32_t uptimeSeconds, int16_t temperatureCx10);
    void recordWifiReconnected(uint32_t unixTime,
                               uint32_t uptimeSeconds,
                               uint32_t lostSeconds,
                               int16_t temperatureCx10);
    void recordApiSuccess();
    void recordApiError(uint32_t unixTime,
                        uint32_t uptimeSeconds,
                        int16_t httpCode,
                        int16_t temperatureCx10);
    void recordHeapLow(uint32_t unixTime,
                       uint32_t uptimeSeconds,
                       uint32_t thresholdBytes,
                       int16_t temperatureCx10);
    void updateBootTemperature(int16_t temperatureCx10);
    bool updateBootUnixTimeIfUnknown(uint32_t bootUnixTime,
                                     uint32_t uptimeSeconds,
                                     int16_t temperatureCx10);
    void updateUptime(uint32_t uptimeSeconds);
    void printStats() const;
    void clear();

private:
    static constexpr uint32_t kVersion = 1;
    static constexpr const char *kNamespace = "longrun";
    static constexpr const char *kBlobKey = "stats";

    Preferences preferences_;
    LongRunStatsData data_;
    bool ready_ = false;

    void captureHeap();
    void captureTemperature(int16_t temperatureCx10);
    void touchEvent(uint32_t unixTime, uint32_t uptimeSeconds);
    void save();
};
