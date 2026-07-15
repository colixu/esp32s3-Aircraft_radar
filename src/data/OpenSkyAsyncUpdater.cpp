#include "OpenSkyAsyncUpdater.h"

#include <WiFi.h>
#include <string.h>

#include "../app/DebugLog.h"

bool OpenSkyAsyncUpdater::begin(const AppConfig &config, const UserSettings &settings, uint32_t requestIntervalMs)
{
    if (taskHandle_ != nullptr)
    {
        config_ = config;
        config_.openSkyLamin = settings.location.queryLatMin;
        config_.openSkyLomin = settings.location.queryLonMin;
        config_.openSkyLamax = settings.location.queryLatMax;
        config_.openSkyLomax = settings.location.queryLonMax;
        requestIntervalMs_ = requestIntervalMs;
        settings_ = settings;
        nextRequestMs_ = millis();
        DebugLog::printf("OpenSkyAsyncUpdater: settings updated, next request now, interval=%lu ms\r\n",
                         static_cast<unsigned long>(requestIntervalMs_));
        return !stopRequested_;
    }

    config_ = config;
    config_.openSkyLamin = settings.location.queryLatMin;
    config_.openSkyLomin = settings.location.queryLonMin;
    config_.openSkyLamax = settings.location.queryLatMax;
    config_.openSkyLomax = settings.location.queryLonMax;
    settings_ = settings;
    requestIntervalMs_ = requestIntervalMs;
    nextRequestMs_ = 0;
    authClient_.begin();
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr)
    {
        DebugLog::println("OpenSkyAsyncUpdater: mutex allocation failed.");
        return false;
    }

    stopRequested_ = false;
    running_ = true;
    const BaseType_t created = xTaskCreatePinnedToCore(taskEntry,
                                                       "OpenSkyUpdateTask",
                                                       12288,
                                                       this,
                                                       1,
                                                       &taskHandle_,
                                                       0);
    if (created != pdPASS)
    {
        running_ = false;
        taskHandle_ = nullptr;
        DebugLog::println("OpenSkyAsyncUpdater: task creation failed.");
        return false;
    }

    DebugLog::println("OpenSkyAsyncUpdater: background task started.");
    return true;
}

void OpenSkyAsyncUpdater::stop()
{
    if (taskHandle_ == nullptr)
    {
        running_ = false;
        updating_ = false;
        stopRequested_ = false;
        return;
    }
    stopRequested_ = true;
}

bool OpenSkyAsyncUpdater::copySnapshot(OpenSkySnapshot &snapshot)
{
    if (mutex_ == nullptr)
    {
        return false;
    }

    bool copied = false;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        if (snapshotPending_)
        {
            snapshot = snapshot_;
            snapshotPending_ = false;
            copied = true;
        }
        xSemaphoreGive(mutex_);
    }
    return copied;
}

bool OpenSkyAsyncUpdater::isRunning() const
{
    return running_;
}

bool OpenSkyAsyncUpdater::isUpdating() const
{
    return updating_;
}

int OpenSkyAsyncUpdater::lastHttpStatus() const
{
    return lastHttpStatus_;
}

uint32_t OpenSkyAsyncUpdater::lastSuccessMs() const
{
    return lastSuccessMs_;
}

const char *OpenSkyAsyncUpdater::lastError() const
{
    return lastError_;
}

bool OpenSkyAsyncUpdater::tokenValid() const
{
    return authClient_.isAuthenticated();
}

uint32_t OpenSkyAsyncUpdater::tokenExpiresInMs() const
{
    return authClient_.tokenExpiresInMs();
}

const char *OpenSkyAsyncUpdater::lastAuthError() const
{
    return authClient_.lastError();
}

void OpenSkyAsyncUpdater::invalidateAuthToken()
{
    authClient_.invalidateToken();
    DebugLog::println("OpenSky auth: token cleared by serial command.");
}

void OpenSkyAsyncUpdater::taskEntry(void *arg)
{
    static_cast<OpenSkyAsyncUpdater *>(arg)->taskLoop();
}

void OpenSkyAsyncUpdater::taskLoop()
{
    OpenSkyProvider openSkyProvider;
    AdsbFiProvider adsbFiProvider;

    while (!stopRequested_)
    {
        const uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (nextRequestMs_ != 0 && now < nextRequestMs_)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        updating_ = true;
        const bool useAdsbFi = settings_.api.provider == ApiProvider::AdsbFi;
        const char *providerName = useAdsbFi ? "adsb.fi" : "OpenSky";
        DebugLog::printf("[%s] API request start\r\n", providerName);
        const uint32_t startMs = millis();
        bool requestOk = false;
        int httpStatus = 0;
        uint8_t aircraftCount = 0;
        bool publishAdsbFi = useAdsbFi;
        bool primaryRateLimited = false;
        if (useAdsbFi)
        {
            requestOk = adsbFiProvider.requestAircraft(settings_);
            httpStatus = adsbFiProvider.httpStatusCode();
            aircraftCount = adsbFiProvider.aircraftCount();
            primaryRateLimited = httpStatus == 429;
            if (!requestOk)
            {
                DebugLog::printf("[adsb.fi] request failed: HTTP %d %s, trying OpenSky fallback\r\n",
                                 httpStatus,
                                 adsbFiProvider.lastError());
                const bool fallbackOk = openSkyProvider.requestStates(config_, settings_, &authClient_);
                DebugLog::printf("[OpenSky fallback] HTTP %d aircraft=%u status=%s\r\n",
                                 openSkyProvider.httpStatusCode(),
                                 openSkyProvider.aircraftCount(),
                                 openSkyProvider.lastError());
                requestOk = fallbackOk;
                httpStatus = openSkyProvider.httpStatusCode();
                aircraftCount = openSkyProvider.aircraftCount();
                publishAdsbFi = false;
                providerName = "OpenSky fallback";
            }
        }
        else
        {
            requestOk = openSkyProvider.requestStates(config_, settings_, &authClient_);
            httpStatus = openSkyProvider.httpStatusCode();
            aircraftCount = openSkyProvider.aircraftCount();
        }
        const uint32_t completedMs = millis();
        const uint32_t durationMs = completedMs - startMs;
        DebugLog::printf("[%s] HTTP %d aircraft=%u interval=%lus duration=%lu ms\r\n",
                         providerName,
                         httpStatus,
                         aircraftCount,
                         static_cast<unsigned long>(requestIntervalMs_ / 1000UL),
                         static_cast<unsigned long>(durationMs));

        if (publishAdsbFi)
        {
            publishSnapshot(adsbFiProvider, requestOk, completedMs, durationMs);
        }
        else
        {
            publishSnapshot(openSkyProvider, requestOk, completedMs, durationMs);
        }
        updating_ = false;

        uint32_t waitMs = requestIntervalMs_;
        if (httpStatus == 429 || primaryRateLimited)
        {
            waitMs = max<uint32_t>(requestIntervalMs_ * 2, primaryRateLimited ? 60000 : 120000);
            DebugLog::printf("[%s] HTTP 429 rate limited, backoff=%lus\r\n",
                             primaryRateLimited ? "adsb.fi" : providerName,
                             static_cast<unsigned long>(waitMs / 1000UL));
        }
        nextRequestMs_ = millis() + waitMs;
    }

    running_ = false;
    taskHandle_ = nullptr;
    vTaskDelete(nullptr);
}

void OpenSkyAsyncUpdater::publishSnapshot(const OpenSkyProvider &provider,
                                          bool requestOk,
                                          uint32_t completedMs,
                                          uint32_t durationMs)
{
    publishSnapshotData(provider.aircraft(),
                        provider.aircraftCount(),
                        provider.rawStateCount(),
                        provider.validPositionCount(),
                        provider.httpStatusCode(),
                        provider.payloadLength(),
                        provider.lastSuccessMs(),
                        provider.lastError(),
                        requestOk,
                        completedMs,
                        durationMs);
}

void OpenSkyAsyncUpdater::publishSnapshot(const AdsbFiProvider &provider,
                                          bool requestOk,
                                          uint32_t completedMs,
                                          uint32_t durationMs)
{
    publishSnapshotData(provider.aircraft(),
                        provider.aircraftCount(),
                        provider.rawStateCount(),
                        provider.validPositionCount(),
                        provider.httpStatusCode(),
                        provider.payloadLength(),
                        provider.lastSuccessMs(),
                        provider.lastError(),
                        requestOk,
                        completedMs,
                        durationMs);
}

void OpenSkyAsyncUpdater::publishSnapshotData(const ApiAircraft *aircraft,
                                              uint8_t aircraftCount,
                                              uint16_t rawStateCount,
                                              uint16_t validPositionCount,
                                              int httpStatusCode,
                                              uint32_t payloadLength,
                                              uint32_t providerLastSuccessMs,
                                              const char *lastError,
                                              bool requestOk,
                                              uint32_t completedMs,
                                              uint32_t durationMs)
{
    OpenSkySnapshot next;
    next.aircraftCount = aircraftCount;
    next.rawStateCount = rawStateCount;
    next.validPositionCount = validPositionCount;
    next.httpStatusCode = httpStatusCode;
    next.payloadLength = payloadLength;
    next.lastSuccessMs = providerLastSuccessMs;
    next.completedMs = completedMs;
    next.durationMs = durationMs;
    next.requestOk = requestOk;
    strncpy(next.lastError, lastError != nullptr ? lastError : "unknown", sizeof(next.lastError) - 1);
    next.lastError[sizeof(next.lastError) - 1] = '\0';

    for (uint8_t i = 0; i < next.aircraftCount; ++i)
    {
        next.aircraft[i] = aircraft[i];
    }

    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        snapshot_ = next;
        snapshotPending_ = true;
        lastHttpStatus_ = next.httpStatusCode;
        lastSuccessMs_ = next.lastSuccessMs;
        strncpy(lastError_, next.lastError, sizeof(lastError_) - 1);
        lastError_[sizeof(lastError_) - 1] = '\0';
        xSemaphoreGive(mutex_);
    }
}
