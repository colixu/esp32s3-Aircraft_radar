#pragma once

#include <Arduino.h>

#include "../app/UserSettings.h"

class OpenSkyAuthClient
{
public:
    bool begin();
    void clear();

    bool hasCredentials(const UserSettings &settings) const;
    bool getValidToken(const UserSettings &settings, char *outToken, size_t outTokenSize);
    void invalidateToken();

    bool isAuthenticated() const;
    uint32_t tokenExpiresInMs() const;
    const char *lastError() const;

private:
    static constexpr size_t kTokenSize = 2048;

    char accessToken_[kTokenSize] = "";
    uint32_t tokenExpireMs_ = 0;
    char lastError_[80] = "not requested";

    bool requestToken(const UserSettings &settings);
    bool tokenIsValid() const;
    String urlEncode(const char *value) const;
    void setError(const char *message);
};
