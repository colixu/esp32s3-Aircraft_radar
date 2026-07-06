#include "UserSettings.h"

#include <string.h>

void loadDefaultUserSettings(UserSettings &settings, const AppConfig &config)
{
    settings.uiTheme = UiTheme::ClassicRadar;
    settings.radarCenterLat = config.radarCenterLat;
    settings.radarCenterLon = config.radarCenterLon;
    settings.maxRangeKm = config.maxRangeKm;
    settings.showGroundTraffic = config.showGroundTraffic;
    settings.minAirborneAltitudeM = config.minAirborneAltitudeM;
    settings.minAirborneSpeedMs = config.minAirborneSpeedMs;
    settings.apiRequestIntervalMs = config.apiRequestIntervalMs;
    settings.predictionEnabled = true;
    settings.predictionFollowAlpha = 0.15f;
    settings.predictionMaxMs = 120000;
    settings.jumpResetDistanceKm = 20.0f;
    settings.lowSpeedPredictionThresholdMs = 20.0f;
    settings.staleGraceMs = 90000;
    settings.staleTimeoutMs = 120000;
    settings.uiButtonPin = -1;

    settings.wifiSsid[0] = '\0';
    settings.wifiPassword[0] = '\0';
    settings.openSkyUsername[0] = '\0';
    settings.openSkyPassword[0] = '\0';
}

const char *uiThemeName(UiTheme theme)
{
    switch (theme)
    {
        case UiTheme::ClassicRadar:
            return "ClassicRadar";
        case UiTheme::ModernRadar:
            return "ModernRadar";
        case UiTheme::CyberpunkRadar:
            return "CyberpunkRadar";
        default:
            return "Unknown";
    }
}

UiTheme nextUiTheme(UiTheme theme)
{
    switch (theme)
    {
        case UiTheme::ClassicRadar:
            return UiTheme::ModernRadar;
        case UiTheme::ModernRadar:
            return UiTheme::CyberpunkRadar;
        case UiTheme::CyberpunkRadar:
        default:
            return UiTheme::ClassicRadar;
    }
}
