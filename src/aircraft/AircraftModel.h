#pragma once

#include <Arduino.h>

struct Aircraft
{
    char callsign[12];
    float distanceKm;
    float bearingDeg;
    float altitudeM;
    float speedMs;
    float headingDeg;
    bool valid;
};

namespace AircraftModel
{
    constexpr uint8_t kAircraftCount = 6;

    float wrapDegrees(float degrees);
    float shortestAngleDelta(float a, float b);
    void setAircraft(Aircraft &aircraft,
                     const char *callsign,
                     float distanceKm,
                     float bearingDeg,
                     float altitudeM,
                     float speedMs,
                     float headingDeg,
                     bool valid);
}
