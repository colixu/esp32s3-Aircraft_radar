#include "SettingsStore.h"

#include <string.h>

#include "DebugLog.h"

namespace
{
    constexpr const char *kPreferencesNamespace = "radar_cfg";
    constexpr uint32_t kSettingsVersion = 1;

#if ENABLE_NVS_SETTINGS
    void getString(Preferences &preferences, const char *key, char *value, size_t valueSize)
    {
        if (valueSize == 0)
        {
            return;
        }

        value[0] = '\0';
        preferences.getString(key, value, valueSize);
        value[valueSize - 1] = '\0';
    }
#endif
}

SettingsStore::SettingsStore(const AppConfig &config) :
    config_(config)
{
}

bool SettingsStore::begin()
{
#if ENABLE_NVS_SETTINGS
    nvsReady_ = preferences_.begin(kPreferencesNamespace, false);
    DebugLog::printf("SettingsStore begin: NVS %s namespace=%s\r\n",
                     nvsReady_ ? "enabled" : "failed",
                     kPreferencesNamespace);
    return nvsReady_;
#else
    nvsReady_ = false;
    DebugLog::println("SettingsStore begin: NVS disabled, volatile settings mode.");
    return true;
#endif
}

bool SettingsStore::load(UserSettings &settings)
{
    loadDefaultUserSettings(settings, config_);

#if ENABLE_NVS_SETTINGS
    if (!nvsReady_)
    {
        sanitizeUserSettings(settings);
        DebugLog::println("NVS not ready, loaded default user settings.");
        printUserSettings(settings);
        return false;
    }

    const uint32_t version = preferences_.getUInt("ver", 0);
    if (version == 0)
    {
        sanitizeUserSettings(settings);
        DebugLog::println("No NVS settings found, using defaults.");
        printUserSettings(settings);
        return true;
    }

    settings.display.uiTheme = static_cast<UiTheme>(preferences_.getUChar("ui",
                                                                           static_cast<uint8_t>(settings.display.uiTheme)));
    settings.display.maxAircraftToDisplay = preferences_.getUChar("maxac", settings.display.maxAircraftToDisplay);
    settings.display.showLabels = preferences_.getBool("labels", settings.display.showLabels);
    settings.display.brightness = preferences_.getUChar("bright", settings.display.brightness);

    settings.location.centerLat = preferences_.getFloat("clat", settings.location.centerLat);
    settings.location.centerLon = preferences_.getFloat("clon", settings.location.centerLon);
    settings.location.maxRangeKm = preferences_.getFloat("range", settings.location.maxRangeKm);
    settings.location.rangePresetsKm[0] = preferences_.getFloat("rng0", settings.location.rangePresetsKm[0]);
    settings.location.rangePresetsKm[1] = preferences_.getFloat("rng1", settings.location.rangePresetsKm[1]);
    settings.location.rangePresetsKm[2] = preferences_.getFloat("rng2", settings.location.rangePresetsKm[2]);
    settings.location.queryLatMin = preferences_.getFloat("qlat0", settings.location.queryLatMin);
    settings.location.queryLonMin = preferences_.getFloat("qlon0", settings.location.queryLonMin);
    settings.location.queryLatMax = preferences_.getFloat("qlat1", settings.location.queryLatMax);
    settings.location.queryLonMax = preferences_.getFloat("qlon1", settings.location.queryLonMax);

    settings.api.provider = static_cast<ApiProvider>(preferences_.getUChar("prov",
                                                                            static_cast<uint8_t>(settings.api.provider)));
    settings.api.accountMode = static_cast<ApiAccountMode>(preferences_.getUChar("acct",
                                                                                  static_cast<uint8_t>(settings.api.accountMode)));
    settings.api.refreshPolicy = static_cast<RefreshPolicy>(preferences_.getUChar("rpol",
                                                                                   static_cast<uint8_t>(settings.api.refreshPolicy)));
    settings.api.dailyCreditBudget = preferences_.getUInt("budget", settings.api.dailyCreditBudget);
    settings.api.creditReserveRatio = preferences_.getFloat("reserve", settings.api.creditReserveRatio);
    settings.api.requestCostCredits = preferences_.getFloat("cost", settings.api.requestCostCredits);
    settings.api.manualRequestIntervalMs = preferences_.getUInt("manint", settings.api.manualRequestIntervalMs);
    settings.api.minUsefulIntervalMs = preferences_.getUInt("minint", settings.api.minUsefulIntervalMs);
    getString(preferences_, "osuser", settings.api.openSkyUsername, sizeof(settings.api.openSkyUsername));
    getString(preferences_, "ospass", settings.api.openSkyPassword, sizeof(settings.api.openSkyPassword));
    getString(preferences_, "oscid", settings.api.openSkyClientId, sizeof(settings.api.openSkyClientId));
    getString(preferences_, "oscsec", settings.api.openSkyClientSecret, sizeof(settings.api.openSkyClientSecret));

    settings.schedule.enabled = preferences_.getBool("sch", settings.schedule.enabled);
    settings.schedule.startMinutesOfDay = preferences_.getShort("start", settings.schedule.startMinutesOfDay);
    settings.schedule.endMinutesOfDay = preferences_.getShort("end", settings.schedule.endMinutesOfDay);
    settings.schedule.timezoneOffsetMinutes = preferences_.getShort("tz", settings.schedule.timezoneOffsetMinutes);
    settings.schedule.idleDisplayMode = static_cast<ScheduleIdleDisplayMode>(
        preferences_.getUChar("idle", static_cast<uint8_t>(settings.schedule.idleDisplayMode)));

    settings.filter.showGroundTraffic = preferences_.getBool("ground", settings.filter.showGroundTraffic);
    settings.filter.minAirborneAltitudeM = preferences_.getFloat("minalt", settings.filter.minAirborneAltitudeM);
    settings.filter.minAirborneSpeedMs = preferences_.getFloat("minspd", settings.filter.minAirborneSpeedMs);

    settings.prediction.enabled = preferences_.getBool("pred", settings.prediction.enabled);
    settings.prediction.followAlpha = preferences_.getFloat("alpha", settings.prediction.followAlpha);
    settings.prediction.predictionMaxMs = preferences_.getUInt("pmax", settings.prediction.predictionMaxMs);
    settings.prediction.jumpResetDistanceKm = preferences_.getFloat("jump", settings.prediction.jumpResetDistanceKm);
    settings.prediction.lowSpeedThresholdMs = preferences_.getFloat("lowspd", settings.prediction.lowSpeedThresholdMs);
    settings.prediction.staleTimeoutMs = preferences_.getUInt("stale", settings.prediction.staleTimeoutMs);
    settings.prediction.correctionEnabled = preferences_.getBool("corr", settings.prediction.correctionEnabled);
    settings.prediction.correctionMinApiIntervalMs = preferences_.getUInt("cmin", settings.prediction.correctionMinApiIntervalMs);
    settings.prediction.correctionDurationMs = preferences_.getUInt("cdur", settings.prediction.correctionDurationMs);
    settings.prediction.correctionStartDistanceKm = preferences_.getFloat("cdist", settings.prediction.correctionStartDistanceKm);

    settings.system.uiButtonPin = preferences_.getShort("btn", settings.system.uiButtonPin);
    settings.system.serialDebug = preferences_.getBool("dbg", settings.system.serialDebug);

    settings.wifi.configured = preferences_.getBool("wcfg", settings.wifi.configured);
    getString(preferences_, "ssid", settings.wifi.ssid, sizeof(settings.wifi.ssid));
    getString(preferences_, "wpass", settings.wifi.password, sizeof(settings.wifi.password));

    sanitizeUserSettings(settings);
    DebugLog::printf("Loaded user settings from NVS version=%lu:\r\n", static_cast<unsigned long>(version));
    printUserSettings(settings);
    return true;
#else
    sanitizeUserSettings(settings);
    DebugLog::println("Loaded default user settings: NVS disabled.");
    printUserSettings(settings);
    return true;
#endif
}

bool SettingsStore::save(const UserSettings &settings)
{
#if ENABLE_NVS_SETTINGS
    if (!nvsReady_)
    {
        DebugLog::println("SettingsStore save skipped: NVS not ready.");
        return false;
    }

    preferences_.putUInt("ver", kSettingsVersion);

    preferences_.putUChar("ui", static_cast<uint8_t>(settings.display.uiTheme));
    preferences_.putUChar("maxac", settings.display.maxAircraftToDisplay);
    preferences_.putBool("labels", settings.display.showLabels);
    preferences_.putUChar("bright", settings.display.brightness);

    preferences_.putFloat("clat", settings.location.centerLat);
    preferences_.putFloat("clon", settings.location.centerLon);
    preferences_.putFloat("range", settings.location.maxRangeKm);
    preferences_.putFloat("rng0", settings.location.rangePresetsKm[0]);
    preferences_.putFloat("rng1", settings.location.rangePresetsKm[1]);
    preferences_.putFloat("rng2", settings.location.rangePresetsKm[2]);
    preferences_.putFloat("qlat0", settings.location.queryLatMin);
    preferences_.putFloat("qlon0", settings.location.queryLonMin);
    preferences_.putFloat("qlat1", settings.location.queryLatMax);
    preferences_.putFloat("qlon1", settings.location.queryLonMax);

    preferences_.putUChar("prov", static_cast<uint8_t>(settings.api.provider));
    preferences_.putUChar("acct", static_cast<uint8_t>(settings.api.accountMode));
    preferences_.putUChar("rpol", static_cast<uint8_t>(settings.api.refreshPolicy));
    preferences_.putUInt("budget", settings.api.dailyCreditBudget);
    preferences_.putFloat("reserve", settings.api.creditReserveRatio);
    preferences_.putFloat("cost", settings.api.requestCostCredits);
    preferences_.putUInt("manint", settings.api.manualRequestIntervalMs);
    preferences_.putUInt("minint", settings.api.minUsefulIntervalMs);
    preferences_.putString("osuser", settings.api.openSkyUsername);
    preferences_.putString("ospass", settings.api.openSkyPassword);
    preferences_.putString("oscid", settings.api.openSkyClientId);
    preferences_.putString("oscsec", settings.api.openSkyClientSecret);

    preferences_.putBool("sch", settings.schedule.enabled);
    preferences_.putShort("start", settings.schedule.startMinutesOfDay);
    preferences_.putShort("end", settings.schedule.endMinutesOfDay);
    preferences_.putShort("tz", settings.schedule.timezoneOffsetMinutes);
    preferences_.putUChar("idle", static_cast<uint8_t>(settings.schedule.idleDisplayMode));

    preferences_.putBool("ground", settings.filter.showGroundTraffic);
    preferences_.putFloat("minalt", settings.filter.minAirborneAltitudeM);
    preferences_.putFloat("minspd", settings.filter.minAirborneSpeedMs);

    preferences_.putBool("pred", settings.prediction.enabled);
    preferences_.putFloat("alpha", settings.prediction.followAlpha);
    preferences_.putUInt("pmax", settings.prediction.predictionMaxMs);
    preferences_.putFloat("jump", settings.prediction.jumpResetDistanceKm);
    preferences_.putFloat("lowspd", settings.prediction.lowSpeedThresholdMs);
    preferences_.putUInt("stale", settings.prediction.staleTimeoutMs);
    preferences_.putBool("corr", settings.prediction.correctionEnabled);
    preferences_.putUInt("cmin", settings.prediction.correctionMinApiIntervalMs);
    preferences_.putUInt("cdur", settings.prediction.correctionDurationMs);
    preferences_.putFloat("cdist", settings.prediction.correctionStartDistanceKm);

    preferences_.putShort("btn", settings.system.uiButtonPin);
    preferences_.putBool("dbg", settings.system.serialDebug);

    preferences_.putBool("wcfg", settings.wifi.configured);
    preferences_.putString("ssid", settings.wifi.ssid);
    preferences_.putString("wpass", settings.wifi.password);

    DebugLog::printf("Settings saved to NVS: ui=%s range=%.0fkm ground=%u idle=%s\r\n",
                     uiThemeName(settings.display.uiTheme),
                     settings.location.maxRangeKm,
                     settings.filter.showGroundTraffic ? 1 : 0,
                     scheduleIdleDisplayModeName(settings.schedule.idleDisplayMode));
    return true;
#else
    DebugLog::printf("SettingsStore save skipped: NVS disabled, volatile mode. ui=%s range=%.0fkm ground=%u idle=%s\r\n",
                     uiThemeName(settings.display.uiTheme),
                     settings.location.maxRangeKm,
                     settings.filter.showGroundTraffic ? 1 : 0,
                     scheduleIdleDisplayModeName(settings.schedule.idleDisplayMode));
    return true;
#endif
}

void SettingsStore::resetToDefault(UserSettings &settings)
{
    loadDefaultUserSettings(settings, config_);
    sanitizeUserSettings(settings);

#if ENABLE_NVS_SETTINGS
    if (nvsReady_)
    {
        save(settings);
        DebugLog::println("Settings reset to defaults and saved to NVS.");
        return;
    }
#endif

    DebugLog::println("Settings reset to defaults in volatile mode.");
}
