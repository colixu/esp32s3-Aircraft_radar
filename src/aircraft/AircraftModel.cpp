#include "AircraftModel.h"

#include <math.h>
#include <string.h>

namespace AircraftModel
{
    float wrapDegrees(float degrees)
    {
        while (degrees >= 360.0f)
        {
            degrees -= 360.0f;
        }
        while (degrees < 0.0f)
        {
            degrees += 360.0f;
        }
        return degrees;
    }

    float shortestAngleDelta(float a, float b)
    {
        float delta = fabsf(wrapDegrees(a) - wrapDegrees(b));
        return delta > 180.0f ? 360.0f - delta : delta;
    }

    void clearAircraft(Aircraft *aircraft, uint8_t aircraftCount)
    {
        for (uint8_t i = 0; i < aircraftCount; ++i)
        {
            memset(&aircraft[i], 0, sizeof(aircraft[i]));
        }
    }

    uint8_t countValid(const Aircraft *aircraft, uint8_t aircraftCount)
    {
        uint8_t validCount = 0;
        for (uint8_t i = 0; i < aircraftCount; ++i)
        {
            if (aircraft[i].valid)
            {
                ++validCount;
            }
        }
        return validCount;
    }

    int8_t firstValidIndex(const Aircraft *aircraft, uint8_t aircraftCount)
    {
        for (uint8_t i = 0; i < aircraftCount; ++i)
        {
            if (aircraft[i].valid)
            {
                return i;
            }
        }
        return -1;
    }

    void setAircraft(Aircraft &aircraft,
                     const char *callsign,
                     float distanceKm,
                     float bearingDeg,
                     float altitudeM,
                     float speedMs,
                     float headingDeg,
                     bool valid)
    {
        strncpy(aircraft.callsign, callsign, sizeof(aircraft.callsign) - 1);
        aircraft.callsign[sizeof(aircraft.callsign) - 1] = '\0';
        aircraft.distanceKm = distanceKm;
        aircraft.bearingDeg = bearingDeg;
        aircraft.altitudeM = altitudeM;
        aircraft.speedMs = speedMs;
        aircraft.headingDeg = headingDeg;
        aircraft.valid = valid;
    }
}
