#pragma once

#include <Arduino.h>

struct Aircraft
{
    char callsign[12];
    char type[8];
    float distanceKm;
    float bearingDeg;
    float altitudeM;
    float speedMs;
    float headingDeg;
    bool valid;
};

namespace AircraftModel
{
    constexpr uint8_t kAircraftCount = 12;

    float wrapDegrees(float degrees);
    float shortestAngleDelta(float a, float b);
    void clearAircraft(Aircraft *aircraft, uint8_t aircraftCount);
    uint8_t countValid(const Aircraft *aircraft, uint8_t aircraftCount);
    int8_t firstValidIndex(const Aircraft *aircraft, uint8_t aircraftCount);
    void setAircraft(Aircraft &aircraft,
                     const char *callsign,
                     float distanceKm,
                     float bearingDeg,
                     float altitudeM,
                     float speedMs,
                     float headingDeg,
                     bool valid);
}
