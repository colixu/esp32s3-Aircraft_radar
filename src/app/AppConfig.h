#pragma once

#include <Arduino.h>

enum class DisplayMode
{
    ClassicRadar,
    ModernRadar
};

enum class AppMode
{
    RadarDemo,
    ApiTest,
    RealRadar
};

struct AppConfig
{
    AppMode appMode = AppMode::RealRadar;
    DisplayMode displayMode = DisplayMode::ClassicRadar;
    float maxRangeKm = 60.0f;
    bool showGroundTraffic = false;
    float minAirborneAltitudeM = 100.0f;
    float minAirborneSpeedMs = 30.0f;
    uint32_t frameIntervalMs = 80;
    uint32_t aircraftUpdateIntervalMs = 700;
    uint32_t selectionIntervalMs = 3200;
    float sweepStepDeg = 3.0f;
    bool showLabels = true;

    uint32_t apiRequestIntervalMs = 60000;
    uint32_t apiScreenRefreshMs = 1000;
    uint32_t apiSerialStatusIntervalMs = 5000;
    uint32_t wifiReconnectIntervalMs = 5000;

    float openSkyLamin = 35.0f;
    float openSkyLomin = 139.0f;
    float openSkyLamax = 36.0f;
    float openSkyLomax = 140.5f;

    float radarCenterLat = 35.55f;
    float radarCenterLon = 139.75f;
};
