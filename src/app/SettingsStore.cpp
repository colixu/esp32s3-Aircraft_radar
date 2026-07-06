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
    sanitizeUserSettings(settings);
    DebugLog::println("Loaded user settings:");
    printUserSettings(settings);
    return true;
}

bool SettingsStore::save(const UserSettings &settings)
{
    // Future version: replace this stub with NVS / Preferences persistence.
    DebugLog::printf("SettingsStore save stub: uiTheme=%s range=%.0fkm ground=%u\r\n",
                     uiThemeName(settings.display.uiTheme),
                     settings.location.maxRangeKm,
                     settings.filter.showGroundTraffic ? 1 : 0);
    return true;
}

void SettingsStore::resetToDefault(UserSettings &settings)
{
    loadDefaultUserSettings(settings, config_);
    sanitizeUserSettings(settings);
}
