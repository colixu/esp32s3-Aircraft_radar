#include "OpenSkyProvider.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../app/DebugLog.h"

bool OpenSkyProvider::requestStates(const AppConfig &config)
{
    UserSettings anonymousSettings;
    loadDefaultUserSettings(anonymousSettings, config);
    anonymousSettings.api.accountMode = ApiAccountMode::Anonymous;
    return requestStates(config, anonymousSettings, nullptr);
}

bool OpenSkyProvider::requestStates(const AppConfig &config, const UserSettings &settings, OpenSkyAuthClient *authClient)
{
    clearAircraft();
    payloadLength_ = 0;
    httpStatusCode_ = 0;
    rawStateCount_ = 0;
    validPositionCount_ = 0;

    const String url = buildUrl(config);
    DebugLog::println("OpenSky request:");
    DebugLog::println(url.c_str());

    char token[2048] = "";
    const bool useAuth = settings.api.accountMode == ApiAccountMode::OpenSkyClient;
    if (useAuth)
    {
        if (authClient == nullptr || !authClient->getValidToken(settings, token, sizeof(token)))
        {
            httpStatusCode_ = 0;
            setError(authClient != nullptr ? authClient->lastError() : "AUTH ERR");
            DebugLog::printf("OpenSky request skipped: %s\r\n", lastError_);
            return false;
        }
    }

    String payload;
    if (!performStatesRequest(url, useAuth ? token : nullptr, payload))
    {
        if (useAuth && httpStatusCode_ == 401 && authClient != nullptr)
        {
            DebugLog::println("OpenSky auth: 401 received, refresh token and retry");
            authClient->invalidateToken();
            token[0] = '\0';
            if (authClient->getValidToken(settings, token, sizeof(token)))
            {
                if (performStatesRequest(url, token, payload))
                {
                    DebugLog::println("OpenSky auth: retry OK");
                }
                else
                {
                    DebugLog::println("OpenSky auth: retry failed");
                    return false;
                }
            }
            else
            {
                setError(authClient->lastError());
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    if (httpStatusCode_ == 429)
    {
        setError("rate limited 429");
        DebugLog::println("OpenSky rate limit reached. Wait before retrying.");
        return false;
    }

    if (httpStatusCode_ != 200)
    {
        snprintf(lastError_, sizeof(lastError_), "HTTP %d", httpStatusCode_);
        DebugLog::println(lastError_);
        return false;
    }

    if (!parsePayload(payload))
    {
        return false;
    }

    lastSuccessMs_ = millis();
    setError("OK");
    DebugLog::printf("Parsed aircraft: %u\r\n", aircraftCount_);
    printSummary();
    return true;
}

bool OpenSkyProvider::performStatesRequest(const String &url, const char *bearerToken, String &payload)
{
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    if (!https.begin(client, url))
    {
        setError("HTTPS begin failed");
        DebugLog::println(lastError_);
        return false;
    }

    if (bearerToken != nullptr && bearerToken[0] != '\0')
    {
        String authorization = "Bearer ";
        authorization += bearerToken;
        https.addHeader("Authorization", authorization);
    }

    httpStatusCode_ = https.GET();
    DebugLog::printf("HTTP status: %d\r\n", httpStatusCode_);

    if (httpStatusCode_ <= 0)
    {
        snprintf(lastError_, sizeof(lastError_), "HTTP error %d", httpStatusCode_);
        DebugLog::println(lastError_);
        https.end();
        return false;
    }

    payload = https.getString();
    https.end();

    payloadLength_ = payload.length();
    DebugLog::printf("Payload length: %lu bytes\r\n", static_cast<unsigned long>(payloadLength_));
    return true;
}

const ApiAircraft *OpenSkyProvider::aircraft() const
{
    return aircraft_;
}

uint8_t OpenSkyProvider::aircraftCount() const
{
    return aircraftCount_;
}

uint16_t OpenSkyProvider::rawStateCount() const
{
    return rawStateCount_;
}

uint16_t OpenSkyProvider::validPositionCount() const
{
    return validPositionCount_;
}

int OpenSkyProvider::httpStatusCode() const
{
    return httpStatusCode_;
}

uint32_t OpenSkyProvider::payloadLength() const
{
    return payloadLength_;
}

uint32_t OpenSkyProvider::lastSuccessMs() const
{
    return lastSuccessMs_;
}

const char *OpenSkyProvider::lastError() const
{
    return lastError_;
}

String OpenSkyProvider::buildUrl(const AppConfig &config) const
{
    String url = "https://opensky-network.org/api/states/all?";
    url += "lamin=";
    url += String(config.openSkyLamin, 4);
    url += "&lomin=";
    url += String(config.openSkyLomin, 4);
    url += "&lamax=";
    url += String(config.openSkyLamax, 4);
    url += "&lomax=";
    url += String(config.openSkyLomax, 4);
    return url;
}

bool OpenSkyProvider::parsePayload(const String &payload)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        snprintf(lastError_, sizeof(lastError_), "JSON: %s", error.c_str());
        DebugLog::println(lastError_);
        return false;
    }

    JsonArray states = doc["states"].as<JsonArray>();
    if (states.isNull())
    {
        setError("states missing");
        DebugLog::println(lastError_);
        return false;
    }

    rawStateCount_ = states.size();

    for (JsonArray state : states)
    {
        if (state[5].isNull() || state[6].isNull())
        {
            continue;
        }

        ++validPositionCount_;

        if (aircraftCount_ >= kMaxAircraft)
        {
            continue;
        }

        ApiAircraft &target = aircraft_[aircraftCount_];
        memset(&target, 0, sizeof(target));

        const char *icao24 = state[0] | "";
        const char *callsign = state[1] | "";
        copyTrimmed(target.icao24, sizeof(target.icao24), icao24);
        copyTrimmed(target.callsign, sizeof(target.callsign), callsign);
        if (target.callsign[0] == '\0')
        {
            copyTrimmed(target.callsign, sizeof(target.callsign), target.icao24);
        }

        target.lon = state[5] | 0.0f;
        target.lat = state[6] | 0.0f;
        target.altitudeM = state[7].isNull() ? (state[13] | 0.0f) : (state[7] | 0.0f);
        target.onGround = state[8] | false;
        target.speedMs = state[9] | 0.0f;
        target.headingDeg = state[10] | 0.0f;
        target.valid = true;

        DebugLog::printf("  %s lat=%.4f lon=%.4f alt=%.0fm speed=%.1fm/s hdg=%.0f onGround=%u\r\n",
                         target.callsign,
                         target.lat,
                         target.lon,
                         target.altitudeM,
                         target.speedMs,
                         target.headingDeg,
                         target.onGround ? 1 : 0);

        ++aircraftCount_;
    }

    if (aircraftCount_ == 0)
    {
        setError("no aircraft");
        DebugLog::println(lastError_);
        return false;
    }

    return true;
}

void OpenSkyProvider::printSummary() const
{
    DebugLog::println("OpenSky summary:");
    DebugLog::printf("  HTTP=%d payload=%lu raw=%u valid_pos=%u aircraft=%u status=%s\r\n",
                     httpStatusCode_,
                     static_cast<unsigned long>(payloadLength_),
                     rawStateCount_,
                     validPositionCount_,
                     aircraftCount_,
                     lastError_);

    const uint8_t count = min<uint8_t>(aircraftCount_, 8);
    for (uint8_t i = 0; i < count; ++i)
    {
        const ApiAircraft &target = aircraft_[i];
        DebugLog::printf("  #%u %-11s icao=%s lat=%.5f lon=%.5f alt=%.0fm speed=%.1fm/s hdg=%.0f onGround=%u\r\n",
                         i + 1,
                         target.callsign,
                         target.icao24,
                         target.lat,
                         target.lon,
                         target.altitudeM,
                         target.speedMs,
                         target.headingDeg,
                         target.onGround ? 1 : 0);
    }
}

void OpenSkyProvider::clearAircraft()
{
    aircraftCount_ = 0;
    for (uint8_t i = 0; i < kMaxAircraft; ++i)
    {
        memset(&aircraft_[i], 0, sizeof(aircraft_[i]));
    }
}

void OpenSkyProvider::setError(const char *message)
{
    strncpy(lastError_, message, sizeof(lastError_) - 1);
    lastError_[sizeof(lastError_) - 1] = '\0';
}

void OpenSkyProvider::copyTrimmed(char *dest, size_t destSize, const char *source) const
{
    if (destSize == 0)
    {
        return;
    }

    while (*source == ' ')
    {
        ++source;
    }

    strncpy(dest, source, destSize - 1);
    dest[destSize - 1] = '\0';

    size_t length = strlen(dest);
    while (length > 0 && dest[length - 1] == ' ')
    {
        dest[length - 1] = '\0';
        --length;
    }
}
