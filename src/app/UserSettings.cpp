#include "UserSettings.h"

#include <math.h>
#include <string.h>

#include "../aircraft/AircraftModel.h"
#include "DebugLog.h"

namespace
{
    constexpr uint32_t kAdsbFiDefaultIntervalMs = 10000;
    constexpr uint32_t kAdsbFiMinIntervalMs = 1000;
    constexpr uint32_t kMaxManualIntervalMs = 3600000;

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
               theme == UiTheme::CyberpunkRadar ||
               theme == UiTheme::PlaneRadar;
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
               mode == ApiAccountMode::CustomBudget ||
               mode == ApiAccountMode::OpenSkyClient;
    }

    bool isValidRefreshPolicy(RefreshPolicy policy)
    {
        return policy == RefreshPolicy::AutoByDailyBudget ||
               policy == RefreshPolicy::ManualInterval;
    }

    bool isValidScheduleIdleDisplayMode(ScheduleIdleDisplayMode mode)
    {
        return mode == ScheduleIdleDisplayMode::PausedStatus ||
               mode == ScheduleIdleDisplayMode::Clock ||
               mode == ScheduleIdleDisplayMode::DisplayOff;
    }

    uint8_t sanitizeWifiTxPowerQuarterDbm(uint8_t power)
    {
        const uint8_t allowed[] = {34, 44, 52, 60, 68, 76, 78};
        uint8_t best = 60;
        uint8_t bestDelta = 255;
        for (uint8_t i = 0; i < sizeof(allowed); ++i)
        {
            const uint8_t candidate = allowed[i];
            const uint8_t delta = power > candidate ? power - candidate : candidate - power;
            if (delta < bestDelta)
            {
                best = candidate;
                bestDelta = delta;
            }
        }
        return best;
    }

    uint32_t defaultBudgetFor(ApiAccountMode mode, uint32_t customBudget)
    {
        switch (mode)
        {
            case ApiAccountMode::Anonymous:
                return 400;
            case ApiAccountMode::StandardUser:
            case ApiAccountMode::OpenSkyClient:
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

    void sanitizeRangePresets(LocationSettings &location)
    {
        for (uint8_t i = 0; i < 3; ++i)
        {
            location.rangePresetsKm[i] = clampValue(location.rangePresetsKm[i], 5.0f, 300.0f);
        }

        for (uint8_t i = 0; i < 2; ++i)
        {
            for (uint8_t j = i + 1; j < 3; ++j)
            {
                if (location.rangePresetsKm[j] < location.rangePresetsKm[i])
                {
                    const float temp = location.rangePresetsKm[i];
                    location.rangePresetsKm[i] = location.rangePresetsKm[j];
                    location.rangePresetsKm[j] = temp;
                }
            }
        }
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
    settings.location.fetchRangeKm = settings.location.maxRangeKm + 15.0f;
    settings.location.rangePresetsKm[0] = 30.0f;
    settings.location.rangePresetsKm[1] = 60.0f;
    settings.location.rangePresetsKm[2] = 120.0f;
    settings.location.queryLatMin = config.openSkyLamin;
    settings.location.queryLonMin = config.openSkyLomin;
    settings.location.queryLatMax = config.openSkyLamax;
    settings.location.queryLonMax = config.openSkyLomax;

    settings.api.provider = ApiProvider::AdsbFi;
    settings.api.accountMode = ApiAccountMode::Anonymous;
    settings.api.refreshPolicy = RefreshPolicy::ManualInterval;
    settings.api.dailyCreditBudget = 400;
    settings.api.creditReserveRatio = 0.90f;
    settings.api.requestCostCredits = 1.0f;
    settings.api.manualRequestIntervalMs = kAdsbFiDefaultIntervalMs;
    settings.api.minUsefulIntervalMs = kAdsbFiMinIntervalMs;

    settings.schedule.enabled = false;
    settings.schedule.startMinutesOfDay = 480;
    settings.schedule.endMinutesOfDay = 1200;
    settings.schedule.timezoneOffsetMinutes = 480;
    settings.schedule.idleDisplayMode = ScheduleIdleDisplayMode::PausedStatus;

    settings.display.uiTheme = UiTheme::ClassicRadar;
    settings.display.maxAircraftToDisplay = AircraftModel::kAircraftCount;
    settings.display.showLabels = config.showLabels;
    settings.display.showEdgeDots = true;
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
    settings.system.wifiTxPowerQuarterDbm = 60;

    sanitizeUserSettings(settings);
    updateQueryBoxFromCenterRange(settings);
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
    settings.location.fetchRangeKm = clampValue(settings.location.fetchRangeKm,
                                                settings.location.maxRangeKm,
                                                maxAllowedFetchRangeKm(settings));
    sanitizeRangePresets(settings.location);

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
        settings.api.provider = ApiProvider::AdsbFi;
    }
    if (!isValidApiAccountMode(settings.api.accountMode))
    {
        settings.api.accountMode = ApiAccountMode::Anonymous;
    }
    if (!isValidRefreshPolicy(settings.api.refreshPolicy))
    {
        settings.api.refreshPolicy = RefreshPolicy::AutoByDailyBudget;
    }
    if (!isValidScheduleIdleDisplayMode(settings.schedule.idleDisplayMode))
    {
        settings.schedule.idleDisplayMode = ScheduleIdleDisplayMode::PausedStatus;
    }
    settings.system.wifiTxPowerQuarterDbm = sanitizeWifiTxPowerQuarterDbm(settings.system.wifiTxPowerQuarterDbm);

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

    if (settings.api.provider == ApiProvider::AdsbFi)
    {
        settings.api.accountMode = ApiAccountMode::Anonymous;
        settings.api.refreshPolicy = RefreshPolicy::ManualInterval;
        settings.api.dailyCreditBudget = max<uint32_t>(settings.api.dailyCreditBudget, 1);
        settings.api.creditReserveRatio = clampValue(settings.api.creditReserveRatio, 0.1f, 1.0f);
        settings.api.requestCostCredits = max(0.1f, settings.api.requestCostCredits);
        settings.api.minUsefulIntervalMs = max<uint32_t>(settings.api.minUsefulIntervalMs, kAdsbFiMinIntervalMs);
        settings.api.manualRequestIntervalMs = clampValue<uint32_t>(settings.api.manualRequestIntervalMs,
                                                                    settings.api.minUsefulIntervalMs,
                                                                    kMaxManualIntervalMs);
    }
    else
    {
        settings.api.dailyCreditBudget = max<uint32_t>(settings.api.dailyCreditBudget, 1);
        settings.api.dailyCreditBudget = defaultBudgetFor(settings.api.accountMode, settings.api.dailyCreditBudget);
        settings.api.creditReserveRatio = clampValue(settings.api.creditReserveRatio, 0.1f, 1.0f);
        settings.api.requestCostCredits = max(0.1f, settings.api.requestCostCredits);
        settings.api.minUsefulIntervalMs = max<uint32_t>(settings.api.minUsefulIntervalMs,
                                                         defaultMinIntervalFor(settings.api.accountMode));
        settings.api.manualRequestIntervalMs = clampValue<uint32_t>(settings.api.manualRequestIntervalMs,
                                                                    settings.api.minUsefulIntervalMs,
                                                                    kMaxManualIntervalMs);
    }

    settings.api.computedRequestIntervalMs = computeRecommendedRequestIntervalMs(settings);
}

void updateQueryBoxFromCenterRange(UserSettings &settings)
{
    settings.location.centerLat = clampValue(settings.location.centerLat, -90.0f, 90.0f);
    settings.location.centerLon = clampValue(settings.location.centerLon, -180.0f, 180.0f);
    settings.location.maxRangeKm = clampValue(settings.location.maxRangeKm, 5.0f, 300.0f);
    settings.location.fetchRangeKm = clampValue(settings.location.fetchRangeKm,
                                                settings.location.maxRangeKm,
                                                maxAllowedFetchRangeKm(settings));

    const float queryRangeKm = effectiveFetchRangeKm(settings);
    const float latDelta = queryRangeKm / 111.32f;
    const float latRadians = settings.location.centerLat * DEG_TO_RAD;
    const float cosLat = max(0.05f, fabsf(cosf(latRadians)));
    const float lonDelta = queryRangeKm / (111.32f * cosLat);

    settings.location.queryLatMin = clampValue(settings.location.centerLat - latDelta, -90.0f, 90.0f);
    settings.location.queryLatMax = clampValue(settings.location.centerLat + latDelta, -90.0f, 90.0f);
    settings.location.queryLonMin = clampValue(settings.location.centerLon - lonDelta, -180.0f, 180.0f);
    settings.location.queryLonMax = clampValue(settings.location.centerLon + lonDelta, -180.0f, 180.0f);
}

bool uiThemeSupportsEdgeDots(UiTheme theme)
{
    return theme == UiTheme::ModernRadar || theme == UiTheme::PlaneRadar;
}

float maxAllowedFetchRangeKm(const UserSettings &settings)
{
    const float displayRangeKm = max(settings.location.maxRangeKm, 1.0f);
    return min(displayRangeKm * 1.5f, 300.0f);
}

float effectiveFetchRangeKm(const UserSettings &settings)
{
    if (!settings.display.showEdgeDots)
    {
        return settings.location.maxRangeKm;
    }

    if (!uiThemeSupportsEdgeDots(settings.display.uiTheme))
    {
        return settings.location.maxRangeKm;
    }

    return settings.location.fetchRangeKm;
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

bool isWithinSchedule(const ScheduleSettings &schedule, int16_t localMinutesOfDay)
{
    if (!schedule.enabled || schedule.startMinutesOfDay == schedule.endMinutesOfDay)
    {
        return true;
    }

    if (localMinutesOfDay < 0)
    {
        return false;
    }

    const int16_t minute = localMinutesOfDay % (24 * 60);
    const int16_t start = schedule.startMinutesOfDay;
    const int16_t end = schedule.endMinutesOfDay;

    if (end > start)
    {
        return minute >= start && minute < end;
    }

    return minute >= start || minute < end;
}

int16_t computeNextScheduleStartMinutes(const ScheduleSettings &schedule, int16_t localMinutesOfDay)
{
    if (!schedule.enabled || schedule.startMinutesOfDay == schedule.endMinutesOfDay)
    {
        return -1;
    }

    if (localMinutesOfDay < 0)
    {
        return schedule.startMinutesOfDay;
    }

    const int16_t minute = localMinutesOfDay % (24 * 60);
    if (isWithinSchedule(schedule, minute))
    {
        return -1;
    }

    return schedule.startMinutesOfDay;
}

uint32_t computeRecommendedRequestIntervalMs(const UserSettings &settings)
{
    if (settings.api.provider == ApiProvider::AdsbFi)
    {
        return max<uint32_t>(settings.api.manualRequestIntervalMs, kAdsbFiMinIntervalMs);
    }

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
    if (settings.api.provider == ApiProvider::AdsbFi)
    {
        return max<uint32_t>(settings.api.manualRequestIntervalMs, kAdsbFiMinIntervalMs);
    }

    if (settings.api.refreshPolicy == RefreshPolicy::ManualInterval)
    {
        return settings.api.manualRequestIntervalMs;
    }

    return settings.api.computedRequestIntervalMs;
}

void printUserSettings(const UserSettings &settings)
{
    DebugLog::println("[User]");
    DebugLog::printf("  configured=%u ssid_set=%u\r\n",
                     settings.wifi.configured ? 1 : 0,
                     settings.wifi.ssid[0] != '\0' ? 1 : 0);
    DebugLog::printf("  center=%.5f,%.5f range=%.0fkm presets=%.0f/%.0f/%.0fkm\r\n",
                     settings.location.centerLat,
                     settings.location.centerLon,
                     settings.location.maxRangeKm,
                     settings.location.rangePresetsKm[0],
                     settings.location.rangePresetsKm[1],
                     settings.location.rangePresetsKm[2]);
    DebugLog::printf("[Range] display=%.0fkm fetch=%.0fkm effectiveFetch=%.0fkm edgeDots=%u supported=%u\r\n",
                     settings.location.maxRangeKm,
                     settings.location.fetchRangeKm,
                     effectiveFetchRangeKm(settings),
                     settings.display.showEdgeDots ? 1 : 0,
                     uiThemeSupportsEdgeDots(settings.display.uiTheme) ? 1 : 0);
    DebugLog::printf("  api=%s account=%s client_id_set=%u client_secret_set=%u\r\n",
                     apiProviderName(settings.api.provider),
                     apiAccountModeName(settings.api.accountMode),
                     settings.api.openSkyClientId[0] != '\0' ? 1 : 0,
                     settings.api.openSkyClientSecret[0] != '\0' ? 1 : 0);
    DebugLog::printf("  schedule=%u start=%d end=%d tz=%d idle=%s theme=%s brightness=%u\r\n",
                     settings.schedule.enabled ? 1 : 0,
                     settings.schedule.startMinutesOfDay,
                     settings.schedule.endMinutesOfDay,
                     settings.schedule.timezoneOffsetMinutes,
                     scheduleIdleDisplayModeName(settings.schedule.idleDisplayMode),
                     uiThemeName(settings.display.uiTheme),
                     settings.display.brightness);
    DebugLog::println("[Advanced]");
    DebugLog::printf("  bbox lat %.4f..%.4f lon %.4f..%.4f labels=%u maxAircraft=%u\r\n",
                     settings.location.queryLatMin,
                     settings.location.queryLatMax,
                     settings.location.queryLonMin,
                     settings.location.queryLonMax,
                     settings.display.showLabels ? 1 : 0,
                     settings.display.maxAircraftToDisplay);
    DebugLog::printf("  refresh policy=%s budget=%lu reserve=%.2f cost=%.1f min=%lums manual=%lums computed=%lums active=%lums\r\n",
                     refreshPolicyName(settings.api.refreshPolicy),
                     static_cast<unsigned long>(settings.api.dailyCreditBudget),
                     settings.api.creditReserveRatio,
                     settings.api.requestCostCredits,
                     static_cast<unsigned long>(settings.api.minUsefulIntervalMs),
                     static_cast<unsigned long>(settings.api.manualRequestIntervalMs),
                     static_cast<unsigned long>(settings.api.computedRequestIntervalMs),
                     static_cast<unsigned long>(activeRequestIntervalMs(settings)));
    DebugLog::printf("  ground=%u minAlt=%.0f minSpeed=%.1f\r\n",
                     settings.filter.showGroundTraffic ? 1 : 0,
                     settings.filter.minAirborneAltitudeM,
                     settings.filter.minAirborneSpeedMs);
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
    DebugLog::printf("  uiButtonPin=%d serialDebug=%u wifiTxPower=%.1fdBm raw=%u\r\n",
                     settings.system.uiButtonPin,
                     settings.system.serialDebug ? 1 : 0,
                     static_cast<float>(settings.system.wifiTxPowerQuarterDbm) / 4.0f,
                     settings.system.wifiTxPowerQuarterDbm);
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
        case UiTheme::PlaneRadar:
            return "PlaneRadar";
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
            return "adsb.fi";
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
        case ApiAccountMode::OpenSkyClient:
            return "OpenSkyClient";
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

const char *scheduleIdleDisplayModeName(ScheduleIdleDisplayMode mode)
{
    switch (mode)
    {
        case ScheduleIdleDisplayMode::PausedStatus:
            return "PausedStatus";
        case ScheduleIdleDisplayMode::Clock:
            return "Clock";
        case ScheduleIdleDisplayMode::DisplayOff:
            return "DisplayOff";
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
            return UiTheme::PlaneRadar;
        case UiTheme::PlaneRadar:
        default:
            return UiTheme::ClassicRadar;
    }
}

ScheduleIdleDisplayMode nextScheduleIdleDisplayMode(ScheduleIdleDisplayMode mode)
{
    switch (mode)
    {
        case ScheduleIdleDisplayMode::PausedStatus:
            return ScheduleIdleDisplayMode::Clock;
        case ScheduleIdleDisplayMode::Clock:
            return ScheduleIdleDisplayMode::DisplayOff;
        case ScheduleIdleDisplayMode::DisplayOff:
        default:
            return ScheduleIdleDisplayMode::PausedStatus;
    }
}
