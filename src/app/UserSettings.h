#pragma once

#include <Arduino.h>

#include "AppConfig.h"

enum class UiTheme
{
    ClassicRadar,
    ModernRadar,
    CyberpunkRadar
};

enum class ApiProvider
{
    OpenSky,
    AdsbFi,
    AirplanesLive,
    AdsbLol,
    Custom
};

enum class ApiAccountMode
{
    Anonymous,
    StandardUser,
    ActiveFeeder,
    CustomBudget,
    OpenSkyClient
};

enum class RefreshPolicy
{
    AutoByDailyBudget,
    ManualInterval
};

enum class ScheduleIdleDisplayMode
{
    PausedStatus,
    Clock,
    DisplayOff
};

enum class DeviceState
{
    Boot,
    ConnectWiFi,
    SetupPortal,
    Running,
    PausedBySchedule,
    WiFiLost,
    ApiError
};

struct WiFiSettings
{
    char ssid[32];
    char password[64];
    bool configured;
};

struct LocationSettings
{
    float centerLat;
    float centerLon;
    float maxRangeKm;
    float rangePresetsKm[3];
    float queryLatMin;
    float queryLonMin;
    float queryLatMax;
    float queryLonMax;
};

struct ApiSettings
{
    ApiProvider provider;
    ApiAccountMode accountMode;
    RefreshPolicy refreshPolicy;

    char openSkyUsername[64];
    char openSkyPassword[64];
    char openSkyClientId[96];
    char openSkyClientSecret[128];

    uint32_t dailyCreditBudget;
    float creditReserveRatio;
    float requestCostCredits;

    uint32_t manualRequestIntervalMs;
    uint32_t computedRequestIntervalMs;
    uint32_t minUsefulIntervalMs;
};

struct ScheduleSettings
{
    bool enabled;
    int16_t startMinutesOfDay;
    int16_t endMinutesOfDay;
    int16_t timezoneOffsetMinutes;
    ScheduleIdleDisplayMode idleDisplayMode;
};

struct DisplaySettings
{
    UiTheme uiTheme;
    uint8_t maxAircraftToDisplay;
    bool showLabels;
    uint8_t brightness;
};

struct FilterSettings
{
    bool showGroundTraffic;
    float minAirborneAltitudeM;
    float minAirborneSpeedMs;
};

struct PredictionSettings
{
    bool enabled;
    float followAlpha;
    uint32_t predictionMaxMs;
    float jumpResetDistanceKm;
    float lowSpeedThresholdMs;
    uint32_t staleTimeoutMs;
    bool correctionEnabled;
    uint32_t correctionMinApiIntervalMs;
    uint32_t correctionDurationMs;
    float correctionStartDistanceKm;
};

struct SystemSettings
{
    int16_t uiButtonPin;
    bool serialDebug;
};

struct UserSettings
{
    WiFiSettings wifi;
    LocationSettings location;
    ApiSettings api;
    ScheduleSettings schedule;
    DisplaySettings display;
    FilterSettings filter;
    PredictionSettings prediction;
    SystemSettings system;
};

void loadDefaultUserSettings(UserSettings &settings);
void loadDefaultUserSettings(UserSettings &settings, const AppConfig &config);
bool validateUserSettings(const UserSettings &settings);
void sanitizeUserSettings(UserSettings &settings);
void updateQueryBoxFromCenterRange(UserSettings &settings);
uint32_t computeActiveSecondsPerDay(const ScheduleSettings &schedule);
bool isWithinSchedule(const ScheduleSettings &schedule, int16_t localMinutesOfDay);
int16_t computeNextScheduleStartMinutes(const ScheduleSettings &schedule, int16_t localMinutesOfDay);
uint32_t computeRecommendedRequestIntervalMs(const UserSettings &settings);
uint32_t activeRequestIntervalMs(const UserSettings &settings);
void printUserSettings(const UserSettings &settings);

const char *uiThemeName(UiTheme theme);
const char *apiProviderName(ApiProvider provider);
const char *apiAccountModeName(ApiAccountMode mode);
const char *refreshPolicyName(RefreshPolicy policy);
const char *scheduleIdleDisplayModeName(ScheduleIdleDisplayMode mode);
UiTheme nextUiTheme(UiTheme theme);
ScheduleIdleDisplayMode nextScheduleIdleDisplayMode(ScheduleIdleDisplayMode mode);
