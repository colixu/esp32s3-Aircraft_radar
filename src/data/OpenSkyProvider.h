#pragma once

#include <Arduino.h>

#include "../app/AppConfig.h"
#include "../app/UserSettings.h"
#include "OpenSkyAuthClient.h"

struct ApiAircraft
{
    char icao24[8];
    char callsign[12];
    char type[8];
    float lat;
    float lon;
    float altitudeM;
    float speedMs;
    float headingDeg;
    bool onGround;
    bool valid;
};

class OpenSkyProvider
{
public:
    static constexpr uint8_t kMaxAircraft = 64;

    bool requestStates(const AppConfig &config);
    bool requestStates(const AppConfig &config, const UserSettings &settings, OpenSkyAuthClient *authClient);

    const ApiAircraft *aircraft() const;
    uint8_t aircraftCount() const;
    uint16_t rawStateCount() const;
    uint16_t validPositionCount() const;
    int httpStatusCode() const;
    uint32_t payloadLength() const;
    uint32_t lastSuccessMs() const;
    const char *lastError() const;

private:
    ApiAircraft aircraft_[kMaxAircraft];
    uint8_t aircraftCount_ = 0;
    uint16_t rawStateCount_ = 0;
    uint16_t validPositionCount_ = 0;
    int httpStatusCode_ = 0;
    uint32_t payloadLength_ = 0;
    uint32_t lastSuccessMs_ = 0;
    char lastError_[64] = "not requested";

    String buildUrl(const AppConfig &config) const;
    bool performStatesRequest(const String &url, const char *bearerToken, String &payload);
    bool parsePayload(const String &payload);
    void printSummary() const;
    void clearAircraft();
    void setError(const char *message);
    void copyTrimmed(char *dest, size_t destSize, const char *source) const;
};
