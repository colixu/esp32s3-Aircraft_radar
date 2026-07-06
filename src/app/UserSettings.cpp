#include "UserSettings.h"

#include <string.h>

#include "../aircraft/AircraftModel.h"
#include "DebugLog.h"

namespace
{
    template <typename T>
    T clampValue(T value, T minValue, T maxValue)
    {
        if (value < minValue)
        {
            return minValue;
        }
        if (value > maxValue)
        {
            return maxValue;
        }
        return value;
    }

    bool isValidUiTheme(UiTheme theme)
    {
        return theme == UiTheme::ClassicRadar ||
               theme == UiTheme::ModernRadar ||
               theme == UiTheme::CyberpunkRadar;
    }

    bool isValidApiProvider(ApiProvider provider)
    {
        return provider == ApiProvider::OpenSky ||
               provider == ApiProvider::AdsbFi ||
               provider == ApiProvider::AirplanesLive ||
               provider == ApiProvider::AdsbLol ||
               provider == ApiProvider::Custom;
    }

    bool isValidApiAccountMode(ApiAccountMode mode)
    {
        return mode == ApiAccountMode::Anonymous ||
               mode == ApiAccountMode::StandardUser ||
               mode == ApiAccountMode::ActiveFeeder ||
               mode == ApiAccountMode::CustomBudget;
    }

    bool isValidRefreshPolicy(RefreshPolicy policy)
    {
        return policy == RefreshPolicy::AutoByDailyBudget ||
               policy == RefreshPolicy::ManualInterval;
    }

    uint32_t defaultBudgetFor(ApiAccountMode mode, uint32_t customBudget)
    {
        switch (mode)
        {
            case ApiAccountMode::Anonymous:
                return 400;
            case ApiAccountMode::StandardUser:
                return 4000;
            case ApiAccountMode::ActiveFeeder:
                return 8000;
            case ApiAccountMode::CustomBudget:
                return customBudget > 0 ? customBudget : 400;
            default:
                return 400;
        }
    }

    uint32_t defaultMinIntervalFor(ApiAccountMode mode)
    {
        return mode == ApiAccountMode::Anonymous ? 10000 : 5000;
    }
}

void loadDefaultUserSettings(UserSettings &settings)
{
    AppConfig config;
    loadDefaultUserSettings(settings, config);
}

void loadDefaultUserSettings(UserSettings &settings, const AppConfig &config)
{
    memset(&settings, 0, sizeof(settings));

    settings.wifi.configured = false;

    settings.location.centerLat = config.radarCenterLat;
    settings.location.centerLon = config.radarCenterLon;
    settings.location.maxRangeKm = config.maxRangeKm;
    settings.location.queryLatMin = config.openSkyLamin;
    settings.location.queryLonMin = config.openSkyLomin;
    settings.location.queryLatMax = config.openSkyLamax;
    settings.location.queryLonMax = config.openSkyLomax;

    settings.api.provider = ApiProvider::OpenSky;
    settings.api.accountMode = ApiAccountMode::Anonymous;
    settings.api.refreshPolicy = RefreshPolicy::ManualInterval;
    settings.api.dailyCreditBudget = 400;
    settings.api.creditReserveRatio = 0.90f;
    settings.api.requestCostCredits = 1.0f;
    settings.api.manualRequestIntervalMs = config.apiRequestIntervalMs;
    settings.api.minUsefulIntervalMs = 10000;

    settings.schedule.enabled = false;
    settings.schedule.startMinutesOfDay = 480;
    settings.schedule.endMinutesOfDay = 1200;
    settings.schedule.timezoneOffsetMinutes = 480;

    settings.display.uiTheme = UiTheme::ClassicRadar;
    settings.display.maxAircraftToDisplay = AircraftModel::kAircraftCount;
    settings.display.showLabels = config.showLabels;
    settings.display.brightness = 255;

    settings.filter.showGroundTraffic = config.showGroundTraffic;
    settings.filter.minAirborneAltitudeM = config.minAirborneAltitudeM;
    settings.filter.minAirborneSpeedMs = config.minAirborneSpeedMs;

    settings.prediction.enabled = true;
    settings.prediction.followAlpha = 0.15f;
    settings.prediction.predictionMaxMs = 120000;
    settings.prediction.jumpResetDistanceKm = 20.0f;
    settings.prediction.lowSpeedThresholdMs = 20.0f;
    settings.prediction.staleTimeoutMs = 120000;
    settings.prediction.correctionEnabled = true;
    settings.prediction.correctionMinApiIntervalMs = 30000;
    settings.prediction.correctionDurationMs = 6000;
    settings.prediction.correctionStartDistanceKm = 0.5f;

    settings.system.uiButtonPin = -1;
    settings.system.serialDebug = true;

    sanitizeUserSettings(settings);
}

bool validateUserSettings(const UserSettings &settings)
{
    UserSettings copy = settings;
    sanitizeUserSettings(copy);
    return memcmp(&copy, &settings, sizeof(UserSettings)) == 0;
}

void sanitizeUserSettings(UserSettings &settings)
{
    settings.location.centerLat = clampValue(settings.location.centerLat, -90.0f, 90.0f);
    settings.location.centerLon = clampValue(settings.location.centerLon, -180.0f, 180.0f);
    settings.location.maxRangeKm = clampValue(settings.location.maxRangeKm, 5.0f, 300.0f);

    settings.location.queryLatMin = clampValue(settings.location.queryLatMin, -90.0f, 90.0f);
    settings.location.queryLatMax = clampValue(settings.location.queryLatMax, -90.0f, 90.0f);
    settings.location.queryLonMin = clampValue(settings.location.queryLonMin, -180.0f, 180.0f);
    settings.location.queryLonMax = clampValue(settings.location.queryLonMax, -180.0f, 180.0f);

    if (settings.location.queryLatMin > settings.location.queryLatMax)
    {
        const float temp = settings.location.queryLatMin;
        settings.location.queryLatMin = settings.location.queryLatMax;
        settings.location.queryLatMax = temp;
    }
    if (settings.location.queryLonMin > settings.location.queryLonMax)
    {
        const float temp = settings.location.queryLonMin;
        settings.location.queryLonMin = settings.location.queryLonMax;
        settings.location.queryLonMax = temp;
    }

    if (!isValidUiTheme(settings.display.uiTheme))
    {
        settings.display.uiTheme = UiTheme::ClassicRadar;
    }
    if (!isValidApiProvider(settings.api.provider))
    {
        settings.api.provider = ApiProvider::OpenSky;
    }
    if (!isValidApiAccountMode(settings.api.accountMode))
    {
        settings.api.accountMode = ApiAccountMode::Anonymous;
    }
    if (!isValidRefreshPolicy(settings.api.refreshPolicy))
    {
        settings.api.refreshPolicy = RefreshPolicy::AutoByDailyBudget;
    }

    settings.display.maxAircraftToDisplay = clampValue<uint8_t>(settings.display.maxAircraftToDisplay,
                                                                1,
                                                                AircraftModel::kAircraftCount);
    settings.display.brightness = clampValue<uint8_t>(settings.display.brightness, 0, 255);

    settings.filter.minAirborneAltitudeM = max(0.0f, settings.filter.minAirborneAltitudeM);
    settings.filter.minAirborneSpeedMs = max(0.0f, settings.filter.minAirborneSpeedMs);

    settings.prediction.followAlpha = clampValue(settings.prediction.followAlpha, 0.01f, 1.0f);
    settings.prediction.predictionMaxMs = max<uint32_t>(settings.prediction.predictionMaxMs, 10000);
    settings.prediction.jumpResetDistanceKm = max(0.1f, settings.prediction.jumpResetDistanceKm);
    settings.prediction.lowSpeedThresholdMs = max(0.0f, settings.prediction.lowSpeedThresholdMs);
    settings.prediction.staleTimeoutMs = max<uint32_t>(settings.prediction.staleTimeoutMs, 10000);
    settings.prediction.correctionMinApiIntervalMs = max<uint32_t>(settings.prediction.correctionMinApiIntervalMs, 0);
    settings.prediction.correctionDurationMs = max<uint32_t>(settings.prediction.correctionDurationMs, 1000);
    settings.prediction.correctionStartDistanceKm = max(0.1f, settings.prediction.correctionStartDistanceKm);

    settings.schedule.startMinutesOfDay = clampValue<int16_t>(settings.schedule.startMinutesOfDay, 0, 1439);
    settings.schedule.endMinutesOfDay = clampValue<int16_t>(settings.schedule.endMinutesOfDay, 0, 1439);

    settings.api.dailyCreditBudget = max<uint32_t>(settings.api.dailyCreditBudget, 1);
    settings.api.dailyCreditBudget = defaultBudgetFor(settings.api.accountMode, settings.api.dailyCreditBudget);
    settings.api.creditReserveRatio = clampValue(settings.api.creditReserveRatio, 0.1f, 1.0f);
    settings.api.requestCostCredits = max(0.1f, settings.api.requestCostCredits);
    settings.api.minUsefulIntervalMs = max<uint32_t>(settings.api.minUsefulIntervalMs,
                                                     defaultMinIntervalFor(settings.api.accountMode));
    settings.api.manualRequestIntervalMs = max<uint32_t>(settings.api.manualRequestIntervalMs,
                                                         settings.api.minUsefulIntervalMs);
    settings.api.computedRequestIntervalMs = computeRecommendedRequestIntervalMs(settings);
}

uint32_t computeActiveSecondsPerDay(const ScheduleSettings &schedule)
{
    if (!schedule.enabled || schedule.startMinutesOfDay == schedule.endMinutesOfDay)
    {
        return 24UL * 3600UL;
    }

    int16_t minutes = schedule.endMinutesOfDay - schedule.startMinutesOfDay;
    if (minutes < 0)
    {
        minutes += 24 * 60;
    }

    return static_cast<uint32_t>(minutes) * 60UL;
}

uint32_t computeRecommendedRequestIntervalMs(const UserSettings &settings)
{
    const uint32_t activeSeconds = computeActiveSecondsPerDay(settings.schedule);
    const float usableCredits = static_cast<float>(settings.api.dailyCreditBudget) *
                                settings.api.creditReserveRatio;
    const float effectiveRequests = usableCredits / settings.api.requestCostCredits;
    if (effectiveRequests <= 0.0f)
    {
        return settings.api.minUsefulIntervalMs;
    }

    const uint32_t intervalMs = static_cast<uint32_t>((static_cast<float>(activeSeconds) / effectiveRequests) * 1000.0f);
    return max<uint32_t>(intervalMs, settings.api.minUsefulIntervalMs);
}

uint32_t activeRequestIntervalMs(const UserSettings &settings)
{
    if (settings.api.refreshPolicy == RefreshPolicy::ManualInterval)
    {
        return settings.api.manualRequestIntervalMs;
    }

    return settings.api.computedRequestIntervalMs;
}

void printUserSettings(const UserSettings &settings)
{
    DebugLog::println("[WiFi]");
    DebugLog::printf("  configured=%u ssid_set=%u\r\n",
                     settings.wifi.configured ? 1 : 0,
                     settings.wifi.ssid[0] != '\0' ? 1 : 0);
    DebugLog::println("[Display]");
    DebugLog::printf("  uiTheme=%s maxAircraft=%u labels=%u brightness=%u\r\n",
                     uiThemeName(settings.display.uiTheme),
                     settings.display.maxAircraftToDisplay,
                     settings.display.showLabels ? 1 : 0,
                     settings.display.brightness);
    DebugLog::println("[Location]");
    DebugLog::printf("  center=%.5f,%.5f range=%.0fkm\r\n",
                     settings.location.centerLat,
                     settings.location.centerLon,
                     settings.location.maxRangeKm);
    DebugLog::printf("  bbox lat %.4f..%.4f lon %.4f..%.4f\r\n",
                     settings.location.queryLatMin,
                     settings.location.queryLatMax,
                     settings.location.queryLonMin,
                     settings.location.queryLonMax);
    DebugLog::println("[API]");
    DebugLog::printf("  provider=%s account=%s policy=%s\r\n",
                     apiProviderName(settings.api.provider),
                     apiAccountModeName(settings.api.accountMode),
                     refreshPolicyName(settings.api.refreshPolicy));
    DebugLog::printf("  budget=%lu reserve=%.2f cost=%.1f min=%lums manual=%lums computed=%lums active=%lums\r\n",
                     static_cast<unsigned long>(settings.api.dailyCreditBudget),
                     settings.api.creditReserveRatio,
                     settings.api.requestCostCredits,
                     static_cast<unsigned long>(settings.api.minUsefulIntervalMs),
                     static_cast<unsigned long>(settings.api.manualRequestIntervalMs),
                     static_cast<unsigned long>(settings.api.computedRequestIntervalMs),
                     static_cast<unsigned long>(activeRequestIntervalMs(settings)));
    DebugLog::println("[Schedule]");
    DebugLog::printf("  enabled=%u start=%d end=%d tz=%d activeSeconds=%lu\r\n",
                     settings.schedule.enabled ? 1 : 0,
                     settings.schedule.startMinutesOfDay,
                     settings.schedule.endMinutesOfDay,
                     settings.schedule.timezoneOffsetMinutes,
                     static_cast<unsigned long>(computeActiveSecondsPerDay(settings.schedule)));
    DebugLog::println("[Filter]");
    DebugLog::printf("  ground=%u minAlt=%.0f minSpeed=%.1f\r\n",
                     settings.filter.showGroundTraffic ? 1 : 0,
                     settings.filter.minAirborneAltitudeM,
                     settings.filter.minAirborneSpeedMs);
    DebugLog::println("[Prediction]");
    DebugLog::printf("  enabled=%u alpha=%.2f max=%lums jump=%.1fkm lowSpeed=%.1f stale=%lums\r\n",
                     settings.prediction.enabled ? 1 : 0,
                     settings.prediction.followAlpha,
                     static_cast<unsigned long>(settings.prediction.predictionMaxMs),
                     settings.prediction.jumpResetDistanceKm,
                     settings.prediction.lowSpeedThresholdMs,
                     static_cast<unsigned long>(settings.prediction.staleTimeoutMs));
    DebugLog::printf("  correction=%u minApi=%lums duration=%lums start=%.1fkm\r\n",
                     settings.prediction.correctionEnabled ? 1 : 0,
                     static_cast<unsigned long>(settings.prediction.correctionMinApiIntervalMs),
                     static_cast<unsigned long>(settings.prediction.correctionDurationMs),
                     settings.prediction.correctionStartDistanceKm);
    DebugLog::println("[System]");
    DebugLog::printf("  uiButtonPin=%d serialDebug=%u\r\n",
                     settings.system.uiButtonPin,
                     settings.system.serialDebug ? 1 : 0);
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

const char *apiProviderName(ApiProvider provider)
{
    switch (provider)
    {
        case ApiProvider::OpenSky:
            return "OpenSky";
        case ApiProvider::AdsbFi:
            return "AdsbFi";
        case ApiProvider::AirplanesLive:
            return "AirplanesLive";
        case ApiProvider::AdsbLol:
            return "AdsbLol";
        case ApiProvider::Custom:
            return "Custom";
        default:
            return "Unknown";
    }
}

const char *apiAccountModeName(ApiAccountMode mode)
{
    switch (mode)
    {
        case ApiAccountMode::Anonymous:
            return "Anonymous";
        case ApiAccountMode::StandardUser:
            return "StandardUser";
        case ApiAccountMode::ActiveFeeder:
            return "ActiveFeeder";
        case ApiAccountMode::CustomBudget:
            return "CustomBudget";
        default:
            return "Unknown";
    }
}

const char *refreshPolicyName(RefreshPolicy policy)
{
    switch (policy)
    {
        case RefreshPolicy::AutoByDailyBudget:
            return "AutoByDailyBudget";
        case RefreshPolicy::ManualInterval:
            return "ManualInterval";
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
