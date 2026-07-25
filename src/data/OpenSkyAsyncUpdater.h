#pragma once

#include <Arduino.h>

#include "../app/AppConfig.h"
#include "../app/UserSettings.h"
#include "AdsbFiProvider.h"
#include "OpenSkyAuthClient.h"
#include "OpenSkyProvider.h"

enum class ApiResultStatus : uint8_t
{
    NotRequested,
    Success,
    EmptyOk,
    Error
};

enum class ApiErrorKind : uint8_t
{
    None,
    NetworkError,
    HttpError,
    AuthError,
    JsonError,
    ResponseFormatError
};

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
    ApiResultStatus resultStatus = ApiResultStatus::NotRequested;
    ApiErrorKind errorKind = ApiErrorKind::None;
    bool fallbackAttempted = false;
    bool fallbackSucceeded = false;
    int primaryHttpStatusCode = 0;
    ApiErrorKind primaryErrorKind = ApiErrorKind::None;
    char lastError[64] = "not requested";
    char primaryError[64] = "";
};

struct OpenSkyAsyncStatus
{
    bool running = false;
    bool updating = false;
    int lastHttpStatus = 0;
    uint32_t lastSuccessMs = 0;
    uint32_t snapshotPublishFailureCount = 0;
    uint32_t primaryProviderErrorCount = 0;
    uint32_t fallbackAttemptCount = 0;
    uint32_t fallbackSuccessCount = 0;
    uint32_t fallbackFailureCount = 0;
    bool tokenValid = false;
    uint32_t tokenExpiresInMs = 0;
    char lastError[64] = "not requested";
    char lastAuthError[80] = "not requested";
};

ApiResultStatus classifyApiResult(bool requestOk, int httpStatusCode, uint8_t aircraftCount);
ApiErrorKind classifyApiErrorKind(bool requestOk, int httpStatusCode, const char *lastError);
bool apiResultIsOk(ApiResultStatus status);
const char *apiResultStatusName(ApiResultStatus status);
const char *apiErrorKindName(ApiErrorKind kind);

class OpenSkyAsyncUpdater
{
public:
    bool begin(const AppConfig &config, const UserSettings &settings, uint32_t requestIntervalMs);
    void stop();

    bool copySnapshot(OpenSkySnapshot &snapshot);
    bool isRunning() const;
    bool isUpdating() const;
    int lastHttpStatus() const;
    uint32_t lastSuccessMs() const;
    const char *lastError() const;
    uint32_t snapshotPublishFailureCount() const;
    bool tokenValid() const;
    uint32_t tokenExpiresInMs() const;
    const char *lastAuthError() const;
    void invalidateAuthToken();
    bool copyStatus(OpenSkyAsyncStatus &status) const;

private:
    AppConfig config_;
    UserSettings settings_;
    OpenSkyAuthClient authClient_;
    uint32_t requestIntervalMs_ = 60000;
    volatile uint32_t nextRequestMs_ = 0;
    TaskHandle_t taskHandle_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    OpenSkySnapshot snapshot_;
    volatile bool running_ = false;
    volatile bool stopRequested_ = false;
    volatile bool updating_ = false;
    bool snapshotPending_ = false;
    int lastHttpStatus_ = 0;
    uint32_t lastSuccessMs_ = 0;
    volatile uint32_t snapshotPublishFailureCount_ = 0;
    uint32_t primaryProviderErrorCount_ = 0;
    uint32_t fallbackAttemptCount_ = 0;
    uint32_t fallbackSuccessCount_ = 0;
    uint32_t fallbackFailureCount_ = 0;
    bool tokenValidCache_ = false;
    uint32_t tokenExpiresInMsCache_ = 0;
    char lastError_[64] = "not requested";
    char lastAuthError_[80] = "not requested";
    volatile bool authInvalidateRequested_ = false;
    mutable char lastErrorCopy_[64] = "not requested";
    mutable char lastAuthErrorCopy_[80] = "not requested";

    static void taskEntry(void *arg);
    void taskLoop();
    void handlePendingAuthInvalidation();
    void updateAuthStatusCache();
    void recordFallbackStats(bool fallbackSucceeded);
    bool copyRuntimeConfig(AppConfig &config, UserSettings &settings, uint32_t &requestIntervalMs);
    void publishSnapshot(const OpenSkyProvider &provider,
                         bool requestOk,
                         uint32_t completedMs,
                         uint32_t durationMs);
    void publishSnapshot(const AdsbFiProvider &provider,
                         bool requestOk,
                         uint32_t completedMs,
                         uint32_t durationMs);
    void publishSnapshotData(const ApiAircraft *aircraft,
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
                             const char *primaryError);
};
