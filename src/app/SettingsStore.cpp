#include "SettingsStore.h"

#include "DebugLog.h"

SettingsStore::SettingsStore(const AppConfig &config) :
    config_(config)
{
}

bool SettingsStore::begin()
{
    DebugLog::println("SettingsStore begin: volatile default settings only.");
    return true;
}

bool SettingsStore::load(UserSettings &settings)
{
    loadDefaultUserSettings(settings, config_);
    DebugLog::println("Loaded user settings:");
    DebugLog::printf("  uiTheme=%s\r\n", uiThemeName(settings.uiTheme));
    DebugLog::printf("  maxRangeKm=%.0f\r\n", settings.maxRangeKm);
    DebugLog::printf("  radarCenterLat=%.5f\r\n", settings.radarCenterLat);
    DebugLog::printf("  radarCenterLon=%.5f\r\n", settings.radarCenterLon);
    DebugLog::printf("  showGroundTraffic=%u\r\n", settings.showGroundTraffic ? 1 : 0);
    DebugLog::printf("  minAirborneAltitudeM=%.0f\r\n", settings.minAirborneAltitudeM);
    DebugLog::printf("  minAirborneSpeedMs=%.1f\r\n", settings.minAirborneSpeedMs);
    DebugLog::printf("  apiRequestIntervalMs=%lu\r\n", static_cast<unsigned long>(settings.apiRequestIntervalMs));
    DebugLog::printf("  predictionCorrectionAlpha=%.2f\r\n", settings.predictionCorrectionAlpha);
    DebugLog::printf("  staleTimeoutMs=%lu\r\n", static_cast<unsigned long>(settings.staleTimeoutMs));
    return true;
}

bool SettingsStore::save(const UserSettings &settings)
{
    // Future version: replace this stub with NVS / Preferences persistence.
    DebugLog::printf("SettingsStore save stub: uiTheme=%s\r\n", uiThemeName(settings.uiTheme));
    return true;
}

void SettingsStore::resetToDefault(UserSettings &settings)
{
    loadDefaultUserSettings(settings, config_);
}
