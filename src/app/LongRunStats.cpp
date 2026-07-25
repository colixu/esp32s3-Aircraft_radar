#include "LongRunStats.h"

#include <esp_system.h>
#include <string.h>

#include "DebugLog.h"
#include "FeatureFlags.h"

namespace
{
    void printAlways(const char *message)
    {
        const bool wasEnabled = DebugLog::isEnabled();
        DebugLog::setEnabled(true);
        DebugLog::println(message);
        DebugLog::setEnabled(wasEnabled);
    }

    void printfAlways(const char *format, unsigned int expected, unsigned int actual)
    {
        const bool wasEnabled = DebugLog::isEnabled();
        DebugLog::setEnabled(true);
        DebugLog::printf(format, expected, actual);
        DebugLog::setEnabled(wasEnabled);
    }

    const char *resetReasonText(uint8_t reason)
    {
        switch (static_cast<esp_reset_reason_t>(reason))
        {
            case ESP_RST_POWERON:
                return "POWERON";
            case ESP_RST_EXT:
                return "EXT";
            case ESP_RST_SW:
                return "SW";
            case ESP_RST_PANIC:
                return "PANIC";
            case ESP_RST_INT_WDT:
                return "INT_WDT";
            case ESP_RST_TASK_WDT:
                return "TASK_WDT";
            case ESP_RST_WDT:
                return "WDT";
            case ESP_RST_DEEPSLEEP:
                return "DEEPSLEEP";
            case ESP_RST_BROWNOUT:
                return "BROWNOUT";
            case ESP_RST_SDIO:
                return "SDIO";
            default:
                return "UNKNOWN";
        }
    }

    void printTemperature(int16_t temperatureCx10)
    {
        if (temperatureCx10 == INT16_MIN)
        {
            DebugLog::print("NAN");
            return;
        }

        DebugLog::printf("%.1fC", static_cast<float>(temperatureCx10) / 10.0f);
    }
}

bool LongRunStats::begin()
{
#if ENABLE_LONG_RUN_STATS
    ready_ = preferences_.begin(kNamespace, false);
    if (!ready_)
    {
        printAlways("NVS_WRITE_ERROR LongRunStats Preferences.begin failed");
        return false;
    }

    LongRunStatsData loaded;
    const size_t readBytes = preferences_.getBytes(kBlobKey, &loaded, sizeof(loaded));
    if (readBytes == sizeof(loaded) && loaded.version == kVersion)
    {
        data_ = loaded;
    }
    else
    {
        data_ = LongRunStatsData();
        save();
    }
    return true;
#else
    ready_ = false;
    return false;
#endif
}

void LongRunStats::recordBoot(uint8_t resetReason,
                              uint32_t unixTime,
                              uint32_t uptimeSeconds,
                              int16_t temperatureCx10)
{
#if ENABLE_LONG_RUN_STATS
    ++data_.bootCount;
    data_.lastResetReason = resetReason;
    data_.lastBootUnixTime = unixTime;
    touchEvent(unixTime, uptimeSeconds);
    captureHeap();
    captureTemperature(temperatureCx10);
    save();
#else
    (void)resetReason;
    (void)unixTime;
    (void)uptimeSeconds;
    (void)temperatureCx10;
#endif
}

void LongRunStats::recordWifiLost(uint32_t unixTime,
                                  uint32_t uptimeSeconds,
                                  int16_t temperatureCx10)
{
#if ENABLE_LONG_RUN_STATS
    ++data_.wifiLostCount;
    touchEvent(unixTime, uptimeSeconds);
    captureHeap();
    captureTemperature(temperatureCx10);
    save();
#else
    (void)unixTime;
    (void)uptimeSeconds;
    (void)temperatureCx10;
#endif
}

void LongRunStats::recordWifiReconnected(uint32_t unixTime,
                                         uint32_t uptimeSeconds,
                                         uint32_t lostSeconds,
                                         int16_t temperatureCx10)
{
#if ENABLE_LONG_RUN_STATS
    ++data_.wifiReconnectCount;
    data_.totalWifiLostSeconds += lostSeconds;
    if (lostSeconds > data_.longestWifiLostSeconds)
    {
        data_.longestWifiLostSeconds = lostSeconds;
    }
    touchEvent(unixTime, uptimeSeconds);
    captureHeap();
    captureTemperature(temperatureCx10);
    save();
#else
    (void)unixTime;
    (void)uptimeSeconds;
    (void)lostSeconds;
    (void)temperatureCx10;
#endif
}

void LongRunStats::recordApiSuccess()
{
#if ENABLE_LONG_RUN_STATS
    if (data_.currentApiConsecutiveErrors != 0)
    {
        data_.currentApiConsecutiveErrors = 0;
        save();
    }
#endif
}

void LongRunStats::recordApiError(uint32_t unixTime,
                                  uint32_t uptimeSeconds,
                                  int16_t httpCode,
                                  int16_t temperatureCx10)
{
#if ENABLE_LONG_RUN_STATS
    ++data_.apiErrorCount;
    ++data_.currentApiConsecutiveErrors;
    if (data_.currentApiConsecutiveErrors > data_.maxApiConsecutiveErrors)
    {
        data_.maxApiConsecutiveErrors = data_.currentApiConsecutiveErrors;
    }
    data_.lastHttpCode = httpCode;
    touchEvent(unixTime, uptimeSeconds);
    captureHeap();
    captureTemperature(temperatureCx10);
    save();
#else
    (void)unixTime;
    (void)uptimeSeconds;
    (void)httpCode;
    (void)temperatureCx10;
#endif
}

void LongRunStats::recordHeapLow(uint32_t unixTime,
                                 uint32_t uptimeSeconds,
                                 uint32_t thresholdBytes,
                                 int16_t temperatureCx10)
{
#if ENABLE_LONG_RUN_STATS
    (void)thresholdBytes;
    ++data_.heapLowCount;
    touchEvent(unixTime, uptimeSeconds);
    captureHeap();
    captureTemperature(temperatureCx10);
    save();
#else
    (void)unixTime;
    (void)uptimeSeconds;
    (void)thresholdBytes;
    (void)temperatureCx10;
#endif
}

void LongRunStats::updateBootTemperature(int16_t temperatureCx10)
{
#if ENABLE_LONG_RUN_STATS
    if (temperatureCx10 == INT16_MIN)
    {
        return;
    }

    data_.lastEventTemperatureCx10 = temperatureCx10;
    captureTemperature(temperatureCx10);
    save();
#else
    (void)temperatureCx10;
#endif
}

bool LongRunStats::updateBootUnixTimeIfUnknown(uint32_t bootUnixTime,
                                               uint32_t uptimeSeconds,
                                               int16_t temperatureCx10)
{
#if ENABLE_LONG_RUN_STATS
    if (data_.lastBootUnixTime != 0 || bootUnixTime < 1700000000UL)
    {
        return data_.lastBootUnixTime != 0;
    }

    data_.lastBootUnixTime = bootUnixTime;
    touchEvent(bootUnixTime + uptimeSeconds, uptimeSeconds);
    captureHeap();
    captureTemperature(temperatureCx10);
    save();
    return true;
#else
    (void)bootUnixTime;
    (void)uptimeSeconds;
    (void)temperatureCx10;
    return false;
#endif
}

void LongRunStats::updateUptime(uint32_t uptimeSeconds)
{
#if ENABLE_LONG_RUN_STATS
    if (uptimeSeconds > data_.maxUptimeSeconds)
    {
        data_.maxUptimeSeconds = uptimeSeconds;
        save();
    }
#else
    (void)uptimeSeconds;
#endif
}

void LongRunStats::printStats() const
{
#if ENABLE_LONG_RUN_STATS
    DebugLog::println("LongRunStats:");
    DebugLog::printf("  bootCount=%lu resetReason=%s(%u)\r\n",
                     static_cast<unsigned long>(data_.bootCount),
                     resetReasonText(data_.lastResetReason),
                     data_.lastResetReason);
    DebugLog::printf("  maxUptime=%lus\r\n",
                     static_cast<unsigned long>(data_.maxUptimeSeconds));
    DebugLog::printf("  wifiLost=%lu reconnect=%lu totalLost=%lus longestLost=%lus\r\n",
                     static_cast<unsigned long>(data_.wifiLostCount),
                     static_cast<unsigned long>(data_.wifiReconnectCount),
                     static_cast<unsigned long>(data_.totalWifiLostSeconds),
                     static_cast<unsigned long>(data_.longestWifiLostSeconds));
    DebugLog::printf("  apiError=%lu lastHttp=%d consecutive=%lu maxConsecutive=%lu\r\n",
                     static_cast<unsigned long>(data_.apiErrorCount),
                     data_.lastHttpCode,
                     static_cast<unsigned long>(data_.currentApiConsecutiveErrors),
                     static_cast<unsigned long>(data_.maxApiConsecutiveErrors));
    DebugLog::printf("  heapLow=%lu minFreeHeap=%lu minMaxAllocHeap=%lu\r\n",
                     static_cast<unsigned long>(data_.heapLowCount),
                     static_cast<unsigned long>(data_.minFreeHeapEver == UINT32_MAX ? 0 : data_.minFreeHeapEver),
                     static_cast<unsigned long>(data_.minMaxAllocHeapEver == UINT32_MAX ? 0 : data_.minMaxAllocHeapEver));
    DebugLog::print("  maxTemp=");
    printTemperature(data_.maxTemperatureCx10);
    DebugLog::print(" lastEventTemp=");
    printTemperature(data_.lastEventTemperatureCx10);
    DebugLog::println();
    DebugLog::printf("  lastBootUnix=%lu lastEventUnix=%lu lastEventUptime=%lus\r\n",
                     static_cast<unsigned long>(data_.lastBootUnixTime),
                     static_cast<unsigned long>(data_.lastEventUnixTime),
                     static_cast<unsigned long>(data_.lastEventUptimeSeconds));
#else
    DebugLog::println("LongRunStats disabled.");
#endif
}

void LongRunStats::clear()
{
#if ENABLE_LONG_RUN_STATS
    data_ = LongRunStatsData();
    save();
#endif
}

void LongRunStats::captureHeap()
{
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t maxAllocHeap = ESP.getMaxAllocHeap();
    if (freeHeap < data_.minFreeHeapEver)
    {
        data_.minFreeHeapEver = freeHeap;
    }
    if (maxAllocHeap < data_.minMaxAllocHeapEver)
    {
        data_.minMaxAllocHeapEver = maxAllocHeap;
    }
}

void LongRunStats::captureTemperature(int16_t temperatureCx10)
{
    if (temperatureCx10 == INT16_MIN)
    {
        return;
    }

    data_.lastEventTemperatureCx10 = temperatureCx10;
    if (temperatureCx10 > data_.maxTemperatureCx10)
    {
        data_.maxTemperatureCx10 = temperatureCx10;
    }
}

void LongRunStats::touchEvent(uint32_t unixTime, uint32_t uptimeSeconds)
{
    data_.lastEventUnixTime = unixTime;
    data_.lastEventUptimeSeconds = uptimeSeconds;
    if (uptimeSeconds > data_.maxUptimeSeconds)
    {
        data_.maxUptimeSeconds = uptimeSeconds;
    }
}

void LongRunStats::save()
{
#if ENABLE_LONG_RUN_STATS
    if (!ready_)
    {
        return;
    }

    data_.version = kVersion;
    const size_t written = preferences_.putBytes(kBlobKey, &data_, sizeof(data_));
    if (written != sizeof(data_))
    {
        printfAlways("NVS_WRITE_ERROR LongRunStats putBytes expected=%u actual=%u\r\n",
                     static_cast<unsigned int>(sizeof(data_)),
                     static_cast<unsigned int>(written));
    }
#endif
}
