#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "UserSettings.h"

struct SystemStatus
{
    DeviceState deviceState = DeviceState::Boot;
    AppMode appMode = AppMode::RadarDemo;
    UiTheme uiTheme = UiTheme::ClassicRadar;

    bool wifiConnected = false;
    bool staSettingsServerRunning = false;
    bool setupPortalRunning = false;
    bool apiUpdaterRunning = false;
    bool apiUpdaterUpdating = false;
    bool ntpSynced = false;
    bool withinSchedule = false;
    bool uiLabRunning = false;

    uint32_t uptimeMs = 0;
    uint32_t freeHeap = 0;
    uint32_t minFreeHeap = 0;
    uint32_t maxAllocHeap = 0;

    uint32_t lastApiSuccessMs = 0;
    uint32_t lastApiErrorMs = 0;
    int lastHttpCode = 0;
    uint32_t apiRequestCount = 0;
    uint32_t apiErrorCount = 0;

    uint8_t aircraftCount = 0;
    uint32_t lastFrameMs = 0;
    uint16_t fpsX10 = 0;
};
