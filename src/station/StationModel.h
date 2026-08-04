#pragma once

#include <Arduino.h>

struct RadarStation
{
    char code[8];
    float distanceKm;
    float bearingDeg;
    bool valid;
};

namespace StationModel
{
    constexpr uint8_t kMaxVisibleStations = 8;

    uint8_t buildVisibleStations(float centerLat,
                                 float centerLon,
                                 float maxRangeKm,
                                 RadarStation *stations,
                                 uint8_t stationCapacity);
}
