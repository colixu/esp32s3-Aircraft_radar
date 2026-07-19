#include "AdsbFiProvider.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <string.h>

#include "../app/DebugLog.h"

namespace
{
    constexpr float kFeetToMeter = 0.3048f;
    constexpr float kKnotsToMeterPerSecond = 0.514444f;
    constexpr float kKmToNauticalMiles = 1.0f / 1.852f;
    constexpr float kMaxSeenPositionSec = 30.0f;

    uint16_t clampSearchDistanceNm(float rangeKm)
    {
        int distanceNm = static_cast<int>(roundf(rangeKm * kKmToNauticalMiles));
        if (distanceNm < 1)
        {
            distanceNm = 1;
        }
        if (distanceNm > 250)
        {
            distanceNm = 250;
        }
        return static_cast<uint16_t>(distanceNm);
    }
}

bool AdsbFiProvider::requestAircraft(const UserSettings &settings)
{
    clearAircraft();
    payloadLength_ = 0;
    httpStatusCode_ = 0;
    rawStateCount_ = 0;
    validPositionCount_ = 0;

    const String url = buildUrl(settings);
    DebugLog::println("adsb.fi request:");
    DebugLog::println(url.c_str());

    String payload;
    if (!performRequest(url, payload))
    {
        return false;
    }

    if (httpStatusCode_ == 429)
    {
        setError("rate limited 429");
        DebugLog::println("adsb.fi rate limit reached. Backing off before retry.");
        return false;
    }

    if (httpStatusCode_ != 200)
    {
        snprintf(lastError_, sizeof(lastError_), "HTTP %d", httpStatusCode_);
        DebugLog::println(lastError_);
        return false;
    }

    if (!parsePayload(payload, settings))
    {
        return false;
    }

    lastSuccessMs_ = millis();
    if (aircraftCount_ == 0 && strcmp(lastError_, "OK") == 0)
    {
        setError("no aircraft");
    }
    DebugLog::printf("Parsed adsb.fi aircraft: %u\r\n", aircraftCount_);
    printSummary();
    return true;
}

const ApiAircraft *AdsbFiProvider::aircraft() const
{
    return aircraft_;
}

uint8_t AdsbFiProvider::aircraftCount() const
{
    return aircraftCount_;
}

uint16_t AdsbFiProvider::rawStateCount() const
{
    return rawStateCount_;
}

uint16_t AdsbFiProvider::validPositionCount() const
{
    return validPositionCount_;
}

int AdsbFiProvider::httpStatusCode() const
{
    return httpStatusCode_;
}

uint32_t AdsbFiProvider::payloadLength() const
{
    return payloadLength_;
}

uint32_t AdsbFiProvider::lastSuccessMs() const
{
    return lastSuccessMs_;
}

const char *AdsbFiProvider::lastError() const
{
    return lastError_;
}

String AdsbFiProvider::buildUrl(const UserSettings &settings) const
{
    const uint16_t distanceNm = clampSearchDistanceNm(settings.location.maxRangeKm);
    String url = "https://opendata.adsb.fi/api/v3/lat/";
    url += String(settings.location.centerLat, 5);
    url += "/lon/";
    url += String(settings.location.centerLon, 5);
    url += "/dist/";
    url += String(distanceNm);
    return url;
}

bool AdsbFiProvider::performRequest(const String &url, String &payload)
{
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(8000);
    if (!https.begin(client, url))
    {
        setError("HTTPS begin failed");
        DebugLog::println(lastError_);
        return false;
    }

    https.addHeader("User-Agent", "ESP32-Aircraft-Radar/0.1");
    https.addHeader("Accept", "application/json");

    httpStatusCode_ = https.GET();
    DebugLog::printf("adsb.fi HTTP status: %d\r\n", httpStatusCode_);

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
    DebugLog::printf("adsb.fi payload length: %lu bytes\r\n",
                     static_cast<unsigned long>(payloadLength_));
    return true;
}

bool AdsbFiProvider::parsePayload(const String &payload, const UserSettings &settings)
{
    setError("OK");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        snprintf(lastError_, sizeof(lastError_), "JSON: %s", error.c_str());
        DebugLog::println(lastError_);
        return false;
    }

    JsonArray aircraftArray = doc["ac"].as<JsonArray>();
    if (aircraftArray.isNull())
    {
        aircraftArray = doc["aircraft"].as<JsonArray>();
    }

    if (aircraftArray.isNull())
    {
        rawStateCount_ = 0;
        validPositionCount_ = 0;
        setError("no aircraft array");
        DebugLog::println(lastError_);
        return true;
    }

    rawStateCount_ = aircraftArray.size();

    for (JsonObject item : aircraftArray)
    {
        const char *hex = item["hex"] | "";
        if (hex == nullptr || hex[0] == '\0')
        {
            continue;
        }

        if (item["lat"].isNull() || item["lon"].isNull())
        {
            continue;
        }

        const float lat = item["lat"] | NAN;
        const float lon = item["lon"] | NAN;
        if (!isfinite(lat) || !isfinite(lon))
        {
            continue;
        }

        const bool hasSeenPos = !item["seen_pos"].isNull();
        const float seenPosSec = hasSeenPos ? (item["seen_pos"] | 0.0f) : 0.0f;
        if (hasSeenPos && seenPosSec > kMaxSeenPositionSec)
        {
            continue;
        }

        ++validPositionCount_;

        ApiAircraft candidate;
        memset(&candidate, 0, sizeof(candidate));
        copyTrimmed(candidate.icao24, sizeof(candidate.icao24), hex);

        const char *flight = item["flight"] | "";
        copyTrimmed(candidate.callsign, sizeof(candidate.callsign), flight);
        if (candidate.callsign[0] == '\0')
        {
            copyTrimmed(candidate.callsign, sizeof(candidate.callsign), candidate.icao24);
        }

        const char *type = item["t"] | "";
        copyTrimmed(candidate.type, sizeof(candidate.type), type);

        candidate.lat = lat;
        candidate.lon = lon;
        candidate.onGround = false;

        if (!item["alt_baro"].isNull() && item["alt_baro"].is<const char *>())
        {
            const char *altText = item["alt_baro"] | "";
            if (strcmp(altText, "ground") == 0)
            {
                candidate.onGround = true;
                candidate.altitudeM = 0.0f;
            }
        }
        else if (!item["alt_baro"].isNull())
        {
            candidate.altitudeM = (item["alt_baro"] | 0.0f) * kFeetToMeter;
        }
        else if (!item["alt_geom"].isNull())
        {
            candidate.altitudeM = (item["alt_geom"] | 0.0f) * kFeetToMeter;
        }

        candidate.speedMs = (item["gs"] | 0.0f) * kKnotsToMeterPerSecond;
        candidate.headingDeg = item["track"] | 0.0f;
        candidate.valid = true;

        const float score = scoreAircraft(candidate.lat, candidate.lon, seenPosSec, settings);
        insertAircraft(candidate, score);
    }

    return true;
}

void AdsbFiProvider::insertAircraft(const ApiAircraft &candidate, float score)
{
    if (aircraftCount_ < kMaxAircraft)
    {
        aircraft_[aircraftCount_] = candidate;
        aircraftScore_[aircraftCount_] = score;
        ++aircraftCount_;
        return;
    }

    uint8_t worstIndex = 0;
    float worstScore = aircraftScore_[0];
    for (uint8_t i = 1; i < kMaxAircraft; ++i)
    {
        if (aircraftScore_[i] > worstScore)
        {
            worstScore = aircraftScore_[i];
            worstIndex = i;
        }
    }

    if (score < worstScore)
    {
        aircraft_[worstIndex] = candidate;
        aircraftScore_[worstIndex] = score;
    }
}

float AdsbFiProvider::scoreAircraft(float lat, float lon, float seenPosSec, const UserSettings &settings) const
{
    const float latKm = (lat - settings.location.centerLat) * 111.32f;
    const float centerLatRad = settings.location.centerLat * DEG_TO_RAD;
    const float lonKm = (lon - settings.location.centerLon) * 111.32f * max(0.05f, fabsf(cosf(centerLatRad)));
    const float distanceKm = sqrtf(latKm * latKm + lonKm * lonKm);
    return distanceKm + seenPosSec * 0.1f;
}

void AdsbFiProvider::printSummary() const
{
    DebugLog::println("adsb.fi summary:");
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
        DebugLog::printf("  #%u %-11s type=%s icao=%s lat=%.5f lon=%.5f alt=%.0fm speed=%.1fm/s hdg=%.0f onGround=%u\r\n",
                         i + 1,
                         target.callsign,
                         target.type[0] != '\0' ? target.type : "--",
                         target.icao24,
                         target.lat,
                         target.lon,
                         target.altitudeM,
                         target.speedMs,
                         target.headingDeg,
                         target.onGround ? 1 : 0);
    }
}

void AdsbFiProvider::clearAircraft()
{
    aircraftCount_ = 0;
    for (uint8_t i = 0; i < kMaxAircraft; ++i)
    {
        memset(&aircraft_[i], 0, sizeof(aircraft_[i]));
        aircraftScore_[i] = 0.0f;
    }
}

void AdsbFiProvider::setError(const char *message)
{
    strncpy(lastError_, message, sizeof(lastError_) - 1);
    lastError_[sizeof(lastError_) - 1] = '\0';
}

void AdsbFiProvider::copyTrimmed(char *dest, size_t destSize, const char *source) const
{
    if (destSize == 0)
    {
        return;
    }

    if (source == nullptr)
    {
        dest[0] = '\0';
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
