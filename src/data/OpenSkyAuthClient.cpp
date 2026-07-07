#include "OpenSkyAuthClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

#include "../app/DebugLog.h"

namespace
{
    constexpr const char *kTokenUrl =
        "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
    constexpr uint32_t kRefreshMarginSeconds = 30;
    constexpr uint32_t kDefaultExpiresInSeconds = 1800;
}

bool OpenSkyAuthClient::begin()
{
    clear();
    return true;
}

void OpenSkyAuthClient::clear()
{
    accessToken_[0] = '\0';
    tokenExpireMs_ = 0;
    setError("not requested");
}

bool OpenSkyAuthClient::hasCredentials(const UserSettings &settings) const
{
    return settings.api.openSkyClientId[0] != '\0' &&
           settings.api.openSkyClientSecret[0] != '\0';
}

bool OpenSkyAuthClient::getValidToken(const UserSettings &settings, char *outToken, size_t outTokenSize)
{
    if (outToken == nullptr || outTokenSize == 0)
    {
        setError("token output missing");
        return false;
    }
    outToken[0] = '\0';

    if (!hasCredentials(settings))
    {
        setError("AUTH CONFIG MISSING");
        DebugLog::println("OpenSky auth: missing client id or secret");
        return false;
    }

    if (!tokenIsValid())
    {
        if (!requestToken(settings))
        {
            return false;
        }
    }

    strncpy(outToken, accessToken_, outTokenSize - 1);
    outToken[outTokenSize - 1] = '\0';
    return outToken[0] != '\0';
}

void OpenSkyAuthClient::invalidateToken()
{
    accessToken_[0] = '\0';
    tokenExpireMs_ = 0;
}

bool OpenSkyAuthClient::isAuthenticated() const
{
    return tokenIsValid();
}

uint32_t OpenSkyAuthClient::tokenExpiresInMs() const
{
    const uint32_t now = millis();
    if (!tokenIsValid() || tokenExpireMs_ <= now)
    {
        return 0;
    }

    return tokenExpireMs_ - now;
}

const char *OpenSkyAuthClient::lastError() const
{
    return lastError_;
}

bool OpenSkyAuthClient::requestToken(const UserSettings &settings)
{
    DebugLog::println("OpenSky auth: requesting token");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    if (!https.begin(client, kTokenUrl))
    {
        setError("AUTH TOKEN BEGIN ERR");
        DebugLog::println("OpenSky auth: HTTPS begin failed");
        return false;
    }

    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=client_credentials&client_id=";
    body += urlEncode(settings.api.openSkyClientId);
    body += "&client_secret=";
    body += urlEncode(settings.api.openSkyClientSecret);

    const int httpStatus = https.POST(body);
    const String payload = https.getString();
    https.end();

    if (httpStatus != 200)
    {
        snprintf(lastError_, sizeof(lastError_), "AUTH TOKEN HTTP %d", httpStatus);
        DebugLog::printf("OpenSky auth: token failed, http=%d\r\n", httpStatus);
        invalidateToken();
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        snprintf(lastError_, sizeof(lastError_), "AUTH TOKEN JSON %s", error.c_str());
        DebugLog::println(lastError_);
        invalidateToken();
        return false;
    }

    const char *token = doc["access_token"] | "";
    if (token[0] == '\0')
    {
        setError("AUTH TOKEN MISSING");
        DebugLog::println("OpenSky auth: access_token missing");
        invalidateToken();
        return false;
    }

    strncpy(accessToken_, token, sizeof(accessToken_) - 1);
    accessToken_[sizeof(accessToken_) - 1] = '\0';

    uint32_t expiresIn = doc["expires_in"] | kDefaultExpiresInSeconds;
    if (expiresIn > kRefreshMarginSeconds)
    {
        expiresIn -= kRefreshMarginSeconds;
    }
    tokenExpireMs_ = millis() + expiresIn * 1000UL;
    setError("OK");
    DebugLog::printf("OpenSky auth: token OK, expires_in=%lu\r\n",
                     static_cast<unsigned long>(doc["expires_in"] | kDefaultExpiresInSeconds));
    return true;
}

bool OpenSkyAuthClient::tokenIsValid() const
{
    return accessToken_[0] != '\0' && tokenExpireMs_ != 0 && millis() < tokenExpireMs_;
}

String OpenSkyAuthClient::urlEncode(const char *value) const
{
    String encoded;
    if (value == nullptr)
    {
        return encoded;
    }

    const char *hex = "0123456789ABCDEF";
    while (*value != '\0')
    {
        const uint8_t c = static_cast<uint8_t>(*value++);
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += static_cast<char>(c);
        }
        else
        {
            encoded += '%';
            encoded += hex[(c >> 4) & 0x0F];
            encoded += hex[c & 0x0F];
        }
    }

    return encoded;
}

void OpenSkyAuthClient::setError(const char *message)
{
    strncpy(lastError_, message, sizeof(lastError_) - 1);
    lastError_[sizeof(lastError_) - 1] = '\0';
}
