#include "OpenSkyAsyncUpdater.h"

#include <WiFi.h>
#include <string.h>

#include "../app/DebugLog.h"

namespace
{
    bool timeReached(uint32_t now, uint32_t deadline)
    {
        return static_cast<int32_t>(now - deadline) >= 0;
    }

    bool startsWith(const char *text, const char *prefix)
    {
        if (text == nullptr || prefix == nullptr)
        {
            return false;
        }

        while (*prefix != '\0')
        {
            if (*text != *prefix)
            {
                return false;
            }
            ++text;
            ++prefix;
        }

        return true;
    }

    void printAlways(const char *message)
    {
        const bool wasEnabled = DebugLog::isEnabled();
        DebugLog::setEnabled(true);
        DebugLog::println(message);
        DebugLog::setEnabled(wasEnabled);
    }

    void printfAlways(const char *format, const char *result, int httpStatus)
    {
        const bool wasEnabled = DebugLog::isEnabled();
        DebugLog::setEnabled(true);
        DebugLog::printf(format, result, httpStatus);
        DebugLog::setEnabled(wasEnabled);
    }

    void applyQueryBoxFromSettings(AppConfig &config, const UserSettings &settings)
    {
        config.openSkyLamin = settings.location.queryLatMin;
        config.openSkyLomin = settings.location.queryLonMin;
        config.openSkyLamax = settings.location.queryLatMax;
        config.openSkyLomax = settings.location.queryLonMax;
    }
}

ApiResultStatus classifyApiResult(bool requestOk, int httpStatusCode, uint8_t aircraftCount)
{
    if (!requestOk || httpStatusCode != 200)
    {
        return ApiResultStatus::Error;
    }

    return aircraftCount > 0 ? ApiResultStatus::Success : ApiResultStatus::EmptyOk;
}

ApiErrorKind classifyApiErrorKind(bool requestOk, int httpStatusCode, const char *lastError)
{
    if (requestOk && httpStatusCode == 200)
    {
        return ApiErrorKind::None;
    }

    if (lastError != nullptr)
    {
        if (startsWith(lastError, "AUTH"))
        {
            return ApiErrorKind::AuthError;
        }
        if (startsWith(lastError, "JSON"))
        {
            return ApiErrorKind::JsonError;
        }
        if (strcmp(lastError, "states missing") == 0 ||
            strcmp(lastError, "no aircraft array") == 0)
        {
            return ApiErrorKind::ResponseFormatError;
        }
    }

    if (httpStatusCode == 401 || httpStatusCode == 403)
    {
        return ApiErrorKind::AuthError;
    }

    if (httpStatusCode <= 0)
    {
        return ApiErrorKind::NetworkError;
    }

    return ApiErrorKind::HttpError;
}

bool apiResultIsOk(ApiResultStatus status)
{
    return status == ApiResultStatus::Success ||
           status == ApiResultStatus::EmptyOk;
}

const char *apiResultStatusName(ApiResultStatus status)
{
    switch (status)
    {
        case ApiResultStatus::Success:
            return "API_SUCCESS";
        case ApiResultStatus::EmptyOk:
            return "API_EMPTY_OK";
        case ApiResultStatus::Error:
            return "API_ERROR";
        case ApiResultStatus::NotRequested:
        default:
            return "API_NOT_REQUESTED";
    }
}

const char *apiErrorKindName(ApiErrorKind kind)
{
    switch (kind)
    {
        case ApiErrorKind::NetworkError:
            return "API_NETWORK_ERROR";
        case ApiErrorKind::HttpError:
            return "API_HTTP_ERROR";
        case ApiErrorKind::AuthError:
            return "API_AUTH_ERROR";
        case ApiErrorKind::JsonError:
            return "API_JSON_ERROR";
        case ApiErrorKind::ResponseFormatError:
            return "API_RESPONSE_FORMAT_ERROR";
        case ApiErrorKind::None:
        default:
            return "API_NO_ERROR";
    }
}

bool OpenSkyAsyncUpdater::begin(const AppConfig &config, const UserSettings &settings, uint32_t requestIntervalMs)
{
    if (mutex_ == nullptr)
    {
        mutex_ = xSemaphoreCreateMutex();
        if (mutex_ == nullptr)
        {
            DebugLog::println("OpenSkyAsyncUpdater: mutex allocation failed.");
            return false;
        }
    }

    if (taskHandle_ != nullptr)
    {
        AppConfig updatedConfig = config;
        applyQueryBoxFromSettings(updatedConfig, settings);
        if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            config_ = updatedConfig;
            requestIntervalMs_ = requestIntervalMs;
            settings_ = settings;
            nextRequestMs_ = millis();
            xSemaphoreGive(mutex_);
        }
        else
        {
            DebugLog::println("OpenSkyAsyncUpdater: settings update lock timeout.");
            return false;
        }
        DebugLog::printf("OpenSkyAsyncUpdater: settings updated, next request now, interval=%lu ms\r\n",
                         static_cast<unsigned long>(requestIntervalMs_));
        return !stopRequested_;
    }

    config_ = config;
    applyQueryBoxFromSettings(config_, settings);
    settings_ = settings;
    requestIntervalMs_ = requestIntervalMs;
    nextRequestMs_ = 0;
    authClient_.begin();
    updateAuthStatusCache();

    stopRequested_ = false;
    running_ = true;
    const BaseType_t created = xTaskCreatePinnedToCore(taskEntry,
                                                       "OpenSkyUpdateTask",
                                                       24576,
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
    OpenSkyAsyncStatus status;
    return copyStatus(status) ? status.lastHttpStatus : lastHttpStatus_;
}

uint32_t OpenSkyAsyncUpdater::lastSuccessMs() const
{
    OpenSkyAsyncStatus status;
    return copyStatus(status) ? status.lastSuccessMs : lastSuccessMs_;
}

const char *OpenSkyAsyncUpdater::lastError() const
{
    OpenSkyAsyncStatus status;
    if (copyStatus(status))
    {
        strncpy(lastErrorCopy_, status.lastError, sizeof(lastErrorCopy_) - 1);
        lastErrorCopy_[sizeof(lastErrorCopy_) - 1] = '\0';
    }
    return lastErrorCopy_;
}

uint32_t OpenSkyAsyncUpdater::snapshotPublishFailureCount() const
{
    OpenSkyAsyncStatus status;
    return copyStatus(status) ? status.snapshotPublishFailureCount : snapshotPublishFailureCount_;
}

bool OpenSkyAsyncUpdater::tokenValid() const
{
    OpenSkyAsyncStatus status;
    return copyStatus(status) ? status.tokenValid : false;
}

uint32_t OpenSkyAsyncUpdater::tokenExpiresInMs() const
{
    OpenSkyAsyncStatus status;
    return copyStatus(status) ? status.tokenExpiresInMs : 0;
}

const char *OpenSkyAsyncUpdater::lastAuthError() const
{
    OpenSkyAsyncStatus status;
    if (copyStatus(status))
    {
        strncpy(lastAuthErrorCopy_, status.lastAuthError, sizeof(lastAuthErrorCopy_) - 1);
        lastAuthErrorCopy_[sizeof(lastAuthErrorCopy_) - 1] = '\0';
    }
    return lastAuthErrorCopy_;
}

void OpenSkyAsyncUpdater::invalidateAuthToken()
{
    authInvalidateRequested_ = true;
    if (taskHandle_ == nullptr && mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        authClient_.invalidateToken();
        updateAuthStatusCache();
        authInvalidateRequested_ = false;
        xSemaphoreGive(mutex_);
    }
    DebugLog::println("OpenSky auth: token invalidation requested.");
}

bool OpenSkyAsyncUpdater::copyStatus(OpenSkyAsyncStatus &status) const
{
    if (mutex_ == nullptr)
    {
        status.running = running_;
        status.updating = updating_;
        return false;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) != pdTRUE)
    {
        status.running = running_;
        status.updating = updating_;
        return false;
    }

    status.running = running_;
    status.updating = updating_;
    status.lastHttpStatus = lastHttpStatus_;
    status.lastSuccessMs = lastSuccessMs_;
    status.snapshotPublishFailureCount = snapshotPublishFailureCount_;
    status.primaryProviderErrorCount = primaryProviderErrorCount_;
    status.fallbackAttemptCount = fallbackAttemptCount_;
    status.fallbackSuccessCount = fallbackSuccessCount_;
    status.fallbackFailureCount = fallbackFailureCount_;
    status.tokenValid = tokenValidCache_;
    status.tokenExpiresInMs = tokenExpiresInMsCache_;
    strncpy(status.lastError, lastError_, sizeof(status.lastError) - 1);
    status.lastError[sizeof(status.lastError) - 1] = '\0';
    strncpy(status.lastAuthError, lastAuthError_, sizeof(status.lastAuthError) - 1);
    status.lastAuthError[sizeof(status.lastAuthError) - 1] = '\0';
    xSemaphoreGive(mutex_);
    return true;
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
        handlePendingAuthInvalidation();

        const uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (nextRequestMs_ != 0 && !timeReached(now, nextRequestMs_))
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        AppConfig localConfig;
        UserSettings localSettings;
        uint32_t localRequestIntervalMs = requestIntervalMs_;
        if (!copyRuntimeConfig(localConfig, localSettings, localRequestIntervalMs))
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        updating_ = true;
        const bool useAdsbFi = localSettings.api.provider == ApiProvider::AdsbFi;
        const char *providerName = useAdsbFi ? "adsb.fi" : "OpenSky";
        DebugLog::printf("[%s] API request start\r\n", providerName);
        const uint32_t startMs = millis();
        bool requestOk = false;
        int httpStatus = 0;
        uint8_t aircraftCount = 0;
        bool publishAdsbFi = useAdsbFi;
        bool primaryRateLimited = false;
        bool fallbackAttempted = false;
        bool fallbackSucceeded = false;
        int primaryHttpStatusCode = 0;
        char primaryError[64] = "";
        if (useAdsbFi)
        {
            requestOk = adsbFiProvider.requestAircraft(localSettings);
            httpStatus = adsbFiProvider.httpStatusCode();
            aircraftCount = adsbFiProvider.aircraftCount();
            primaryRateLimited = httpStatus == 429;
            if (!requestOk)
            {
                fallbackAttempted = true;
                primaryHttpStatusCode = httpStatus;
                strncpy(primaryError, adsbFiProvider.lastError(), sizeof(primaryError) - 1);
                primaryError[sizeof(primaryError) - 1] = '\0';
                DebugLog::printf("[adsb.fi] request failed: HTTP %d %s, trying OpenSky fallback\r\n",
                                 httpStatus,
                                 adsbFiProvider.lastError());
                const bool fallbackOk = openSkyProvider.requestStates(localConfig, localSettings, &authClient_);
                fallbackSucceeded = apiResultIsOk(classifyApiResult(fallbackOk,
                                                                     openSkyProvider.httpStatusCode(),
                                                                     openSkyProvider.aircraftCount()));
                DebugLog::printf("[OpenSky fallback] HTTP %d aircraft=%u status=%s\r\n",
                                 openSkyProvider.httpStatusCode(),
                                 openSkyProvider.aircraftCount(),
                                 openSkyProvider.lastError());
                requestOk = fallbackOk;
                httpStatus = openSkyProvider.httpStatusCode();
                aircraftCount = openSkyProvider.aircraftCount();
                publishAdsbFi = false;
                providerName = "OpenSky fallback";
                recordFallbackStats(fallbackSucceeded);
            }
        }
        else
        {
            requestOk = openSkyProvider.requestStates(localConfig, localSettings, &authClient_);
            httpStatus = openSkyProvider.httpStatusCode();
            aircraftCount = openSkyProvider.aircraftCount();
        }
        const uint32_t completedMs = millis();
        const uint32_t durationMs = completedMs - startMs;
        DebugLog::printf("[%s] HTTP %d aircraft=%u interval=%lus duration=%lu ms\r\n",
                         providerName,
                         httpStatus,
                         aircraftCount,
                         static_cast<unsigned long>(localRequestIntervalMs / 1000UL),
                         static_cast<unsigned long>(durationMs));

        if (publishAdsbFi)
        {
            publishSnapshotData(adsbFiProvider.aircraft(),
                                adsbFiProvider.aircraftCount(),
                                adsbFiProvider.rawStateCount(),
                                adsbFiProvider.validPositionCount(),
                                adsbFiProvider.httpStatusCode(),
                                adsbFiProvider.payloadLength(),
                                adsbFiProvider.lastSuccessMs(),
                                adsbFiProvider.lastError(),
                                requestOk,
                                completedMs,
                                durationMs,
                                false,
                                false,
                                0,
                                "");
        }
        else
        {
            publishSnapshotData(openSkyProvider.aircraft(),
                                openSkyProvider.aircraftCount(),
                                openSkyProvider.rawStateCount(),
                                openSkyProvider.validPositionCount(),
                                openSkyProvider.httpStatusCode(),
                                openSkyProvider.payloadLength(),
                                openSkyProvider.lastSuccessMs(),
                                openSkyProvider.lastError(),
                                requestOk,
                                completedMs,
                                durationMs,
                                fallbackAttempted,
                                fallbackSucceeded,
                                primaryHttpStatusCode,
                                primaryError);
        }
        if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            updateAuthStatusCache();
            xSemaphoreGive(mutex_);
        }
        updating_ = false;

        uint32_t waitMs = localRequestIntervalMs;
        if (httpStatus == 429 || primaryRateLimited)
        {
            waitMs = max<uint32_t>(localRequestIntervalMs * 2, primaryRateLimited ? 60000 : 120000);
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

void OpenSkyAsyncUpdater::handlePendingAuthInvalidation()
{
    if (!authInvalidateRequested_)
    {
        return;
    }

    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        authClient_.invalidateToken();
        authInvalidateRequested_ = false;
        updateAuthStatusCache();
        xSemaphoreGive(mutex_);
        DebugLog::println("OpenSky auth: token cleared by updater task.");
    }
}

void OpenSkyAsyncUpdater::updateAuthStatusCache()
{
    tokenValidCache_ = authClient_.isAuthenticated();
    tokenExpiresInMsCache_ = authClient_.tokenExpiresInMs();
    strncpy(lastAuthError_, authClient_.lastError(), sizeof(lastAuthError_) - 1);
    lastAuthError_[sizeof(lastAuthError_) - 1] = '\0';
}

void OpenSkyAsyncUpdater::recordFallbackStats(bool fallbackSucceeded)
{
    if (mutex_ == nullptr)
    {
        return;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return;
    }

    ++primaryProviderErrorCount_;
    ++fallbackAttemptCount_;
    if (fallbackSucceeded)
    {
        ++fallbackSuccessCount_;
    }
    else
    {
        ++fallbackFailureCount_;
    }
    xSemaphoreGive(mutex_);
}

bool OpenSkyAsyncUpdater::copyRuntimeConfig(AppConfig &config, UserSettings &settings, uint32_t &requestIntervalMs)
{
    if (mutex_ == nullptr)
    {
        DebugLog::println("OpenSkyAsyncUpdater: runtime config mutex missing.");
        return false;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        DebugLog::println("OpenSkyAsyncUpdater: runtime config lock timeout.");
        return false;
    }

    config = config_;
    settings = settings_;
    requestIntervalMs = requestIntervalMs_;
    xSemaphoreGive(mutex_);
    return true;
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
                        durationMs,
                        false,
                        false,
                        0,
                        "");
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
                        durationMs,
                        false,
                        false,
                        0,
                        "");
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
                                              uint32_t durationMs,
                                              bool fallbackAttempted,
                                              bool fallbackSucceeded,
                                              int primaryHttpStatusCode,
                                              const char *primaryError)
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
    next.resultStatus = classifyApiResult(requestOk, httpStatusCode, aircraftCount);
    next.errorKind = classifyApiErrorKind(requestOk, httpStatusCode, lastError);
    next.fallbackAttempted = fallbackAttempted;
    next.fallbackSucceeded = fallbackSucceeded;
    next.primaryHttpStatusCode = primaryHttpStatusCode;
    next.primaryErrorKind = fallbackAttempted ?
                            classifyApiErrorKind(false, primaryHttpStatusCode, primaryError) :
                            ApiErrorKind::None;
    strncpy(next.lastError, lastError != nullptr ? lastError : "unknown", sizeof(next.lastError) - 1);
    next.lastError[sizeof(next.lastError) - 1] = '\0';
    strncpy(next.primaryError, primaryError != nullptr ? primaryError : "", sizeof(next.primaryError) - 1);
    next.primaryError[sizeof(next.primaryError) - 1] = '\0';

    for (uint8_t i = 0; i < next.aircraftCount; ++i)
    {
        next.aircraft[i] = aircraft[i];
    }

    if (mutex_ == nullptr)
    {
        ++snapshotPublishFailureCount_;
        printAlways("SNAPSHOT_PUBLISH_ERROR mutex missing");
        return;
    }

    const uint32_t publishStartedMs = millis();
    while (true)
    {
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(250)) == pdTRUE)
        {
            if (!snapshotPending_)
            {
                snapshot_ = next;
                snapshotPending_ = true;
                lastHttpStatus_ = next.httpStatusCode;
                lastSuccessMs_ = next.lastSuccessMs;
                strncpy(lastError_, next.lastError, sizeof(lastError_) - 1);
                lastError_[sizeof(lastError_) - 1] = '\0';
                xSemaphoreGive(mutex_);
                return;
            }
            xSemaphoreGive(mutex_);
        }
        if (stopRequested_ && millis() - publishStartedMs >= 1500)
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ++snapshotPublishFailureCount_;
    printfAlways("SNAPSHOT_PUBLISH_ERROR stopped before publish result=%s http=%d\r\n",
                 apiResultStatusName(next.resultStatus),
                 next.httpStatusCode);
}
