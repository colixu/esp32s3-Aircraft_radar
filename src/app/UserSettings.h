#pragma once

#include <Arduino.h>

#include "AppConfig.h"

enum class UiTheme
{
    ClassicRadar,
    ModernRadar,
    CyberpunkRadar
};

struct UserSettings
{
    UiTheme uiTheme;
    float radarCenterLat;
    float radarCenterLon;
    float maxRangeKm;
    bool showGroundTraffic;
    float minAirborneAltitudeM;
    float minAirborneSpeedMs;
    uint32_t apiRequestIntervalMs;
    float predictionCorrectionAlpha;
    float predictionResetDistanceKm;
    uint32_t predictionMaxDeltaMs;
    uint32_t staleGraceMs;
    uint32_t staleTimeoutMs;
    int uiButtonPin;

    char wifiSsid[32];
    char wifiPassword[64];
    char openSkyUsername[64];
    char openSkyPassword[64];
};

void loadDefaultUserSettings(UserSettings &settings, const AppConfig &config);
const char *uiThemeName(UiTheme theme);
UiTheme nextUiTheme(UiTheme theme);
