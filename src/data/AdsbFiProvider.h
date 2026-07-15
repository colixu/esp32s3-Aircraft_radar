#pragma once

#include <Arduino.h>

#include "../app/UserSettings.h"
#include "OpenSkyProvider.h"

class AdsbFiProvider
{
public:
    static constexpr uint8_t kMaxAircraft = OpenSkyProvider::kMaxAircraft;

    bool requestAircraft(const UserSettings &settings);

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
    float aircraftScore_[kMaxAircraft];
    uint8_t aircraftCount_ = 0;
    uint16_t rawStateCount_ = 0;
    uint16_t validPositionCount_ = 0;
    int httpStatusCode_ = 0;
    uint32_t payloadLength_ = 0;
    uint32_t lastSuccessMs_ = 0;
    char lastError_[64] = "not requested";

    String buildUrl(const UserSettings &settings) const;
    bool performRequest(const String &url, String &payload);
    bool parsePayload(const String &payload, const UserSettings &settings);
    void insertAircraft(const ApiAircraft &candidate, float score);
    float scoreAircraft(float lat, float lon, float seenPosSec, const UserSettings &settings) const;
    void printSummary() const;
    void clearAircraft();
    void setError(const char *message);
    void copyTrimmed(char *dest, size_t destSize, const char *source) const;
};
