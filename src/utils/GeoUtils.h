#pragma once

#include <Arduino.h>

namespace GeoUtils
{
    bool geoToRadar(float centerLat,
                    float centerLon,
                    float targetLat,
                    float targetLon,
                    float &distanceKm,
                    float &bearingDeg);
}
