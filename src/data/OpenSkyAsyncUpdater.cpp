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
    OpenSkyProvider provider;

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
        DebugLog::println("API request start");
        const uint32_t startMs = millis();
        const bool requestOk = provider.requestStates(config_, settings_, &authClient_);
        const uint32_t completedMs = millis();
        const uint32_t durationMs = completedMs - startMs;
        DebugLog::printf("API request done, duration=%lu ms\r\n", static_cast<unsigned long>(durationMs));

        publishSnapshot(provider, requestOk, completedMs, durationMs);
        updating_ = false;

        uint32_t waitMs = requestIntervalMs_;
        if (provider.httpStatusCode() == 429)
        {
            waitMs = max<uint32_t>(requestIntervalMs_ * 2, 120000);
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
    OpenSkySnapshot next;
    next.aircraftCount = provider.aircraftCount();
    next.rawStateCount = provider.rawStateCount();
    next.validPositionCount = provider.validPositionCount();
    next.httpStatusCode = provider.httpStatusCode();
    next.payloadLength = provider.payloadLength();
    next.lastSuccessMs = provider.lastSuccessMs();
    next.completedMs = completedMs;
    next.durationMs = durationMs;
    next.requestOk = requestOk;
    strncpy(next.lastError, provider.lastError(), sizeof(next.lastError) - 1);
    next.lastError[sizeof(next.lastError) - 1] = '\0';

    const ApiAircraft *source = provider.aircraft();
    for (uint8_t i = 0; i < next.aircraftCount; ++i)
    {
        next.aircraft[i] = source[i];
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
