#pragma once

#include <Arduino.h>

#include "../app/AppConfig.h"
#include "OpenSkyProvider.h"

struct OpenSkySnapshot
{
    ApiAircraft aircraft[OpenSkyProvider::kMaxAircraft];
    uint8_t aircraftCount = 0;
    uint16_t rawStateCount = 0;
    uint16_t validPositionCount = 0;
    int httpStatusCode = 0;
    uint32_t payloadLength = 0;
    uint32_t lastSuccessMs = 0;
    uint32_t completedMs = 0;
    uint32_t durationMs = 0;
    bool requestOk = false;
    char lastError[64] = "not requested";
};

class OpenSkyAsyncUpdater
{
public:
    bool begin(const AppConfig &config, uint32_t requestIntervalMs);
    void stop();

    bool copySnapshot(OpenSkySnapshot &snapshot);
    bool isRunning() const;
    bool isUpdating() const;
    int lastHttpStatus() const;
    uint32_t lastSuccessMs() const;
    const char *lastError() const;

private:
    AppConfig config_;
    uint32_t requestIntervalMs_ = 60000;
    TaskHandle_t taskHandle_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    OpenSkySnapshot snapshot_;
    volatile bool running_ = false;
    volatile bool stopRequested_ = false;
    volatile bool updating_ = false;
    bool snapshotPending_ = false;
    int lastHttpStatus_ = 0;
    uint32_t lastSuccessMs_ = 0;
    char lastError_[64] = "not requested";

    static void taskEntry(void *arg);
    void taskLoop();
    void publishSnapshot(const OpenSkyProvider &provider,
                         bool requestOk,
                         uint32_t completedMs,
                         uint32_t durationMs);
};
