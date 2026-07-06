#pragma once

#include <Arduino.h>

namespace GeoPredict
{
    bool predictLatLon(float lat,
                       float lon,
                       float speedMs,
                       float headingDeg,
                       float deltaSeconds,
                       float &outLat,
                       float &outLon);
}
