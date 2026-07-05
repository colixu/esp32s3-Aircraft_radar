#pragma once

#include <Arduino.h>

enum class DisplayMode
{
    ClassicRadar,
    ModernRadar
};

struct AppConfig
{
    DisplayMode displayMode = DisplayMode::ClassicRadar;
    float maxRangeKm = 100.0f;
    uint32_t frameIntervalMs = 80;
    uint32_t aircraftUpdateIntervalMs = 700;
    uint32_t selectionIntervalMs = 3200;
    float sweepStepDeg = 3.0f;
    bool showLabels = true;
};
