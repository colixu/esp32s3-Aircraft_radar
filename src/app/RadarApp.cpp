#include "RadarApp.h"

#include <WiFi.h>
#include <driver/temp_sensor.h>
#include <math.h>
#include <string.h>

#include "DebugLog.h"
#include "FeatureFlags.h"
#include "../utils/GeoUtils.h"

namespace
{
    constexpr uint32_t kScheduleCheckIntervalMs = 5000;
    constexpr uint32_t kSystemStatusLogIntervalMs = 60000;
    constexpr uint32_t kTemperatureLogIntervalMs = 1000;
    constexpr uint32_t kLocalMenuTimeoutMs = 30000;
    constexpr uint32_t kLocalMenuOpenLongGuardMs = 8000;
    constexpr uint32_t kLocalMenuRefreshMs = 500;
    constexpr uint32_t kIdleThemePreviewMs = 30000;
    constexpr uint32_t kIdleRangePreviewMs = 10UL * 60UL * 1000UL;
    constexpr uint32_t kIdleRangePreviewRefreshMs = 10000;
    constexpr uint32_t kIdleRangePreviewApiIntervalMs = 5UL * 60UL * 1000UL;
    constexpr uint8_t kLocalMenuItemCount = 7;
    constexpr uint8_t kBrightnessLevelCount = 4;

    const char *const kLocalMenuItems[kLocalMenuItemCount] = {
        "Settings QR",
        "Reboot",
        "Screen Sleep",
        "Brightness",
        "Idle Display",
        "AP Setup",
        "Exit"
    };

    const char *const kBrightnessLabels[kBrightnessLevelCount] = {
        "25%",
        "50%",
        "75%",
        "100%"
    };

    const uint8_t kBrightnessValues[kBrightnessLevelCount] = {
        64,
        128,
        192,
        255
    };

    bool keyMatches(const char *key, const char *a, const char *b = nullptr, const char *c = nullptr)
    {
        return (a != nullptr && strcmp(key, a) == 0) ||
               (b != nullptr && strcmp(key, b) == 0) ||
               (c != nullptr && strcmp(key, c) == 0);
    }

    const char *deviceStateName(DeviceState state)
    {
        switch (state)
        {
            case DeviceState::Boot:
                return "Boot";
            case DeviceState::ConnectWiFi:
                return "ConnectWiFi";
            case DeviceState::SetupPortal:
                return "SetupPortal";
            case DeviceState::Running:
                return "Running";
            case DeviceState::PausedBySchedule:
                return "PausedBySchedule";
            case DeviceState::WiFiLost:
                return "WiFiLost";
            case DeviceState::ApiError:
                return "ApiError";
            default:
                return "Unknown";
        }
    }

    bool isButtonInputEvent(InputEvent event)
    {
        return event == InputEvent::KeyUpShort ||
               event == InputEvent::KeyUpLong ||
               event == InputEvent::KeyUpDouble ||
               event == InputEvent::KeyDownShort ||
               event == InputEvent::KeyDownLong ||
               event == InputEvent::KeyDownDouble;
    }

    bool isBootButtonInputEvent(InputEvent event)
    {
        return event == InputEvent::BootButtonShort ||
               event == InputEvent::BootButtonDouble ||
               event == InputEvent::BootButtonLong;
    }

    ScheduleIdleDisplayMode previousIdleDisplayMode(ScheduleIdleDisplayMode mode)
    {
        switch (mode)
        {
            case ScheduleIdleDisplayMode::PausedStatus:
                return ScheduleIdleDisplayMode::DisplayOff;
            case ScheduleIdleDisplayMode::Clock:
                return ScheduleIdleDisplayMode::PausedStatus;
            case ScheduleIdleDisplayMode::DisplayOff:
            default:
                return ScheduleIdleDisplayMode::Clock;
        }
    }

    bool startInternalTemperatureSensor()
    {
        temp_sensor_config_t config = TSENS_CONFIG_DEFAULT();

        esp_err_t result = temp_sensor_set_config(config);
        if (result != ESP_OK)
        {
            DebugLog::printf("ESP32-S3 internal temperature sensor config failed: %d\r\n", result);
            return false;
        }

        result = temp_sensor_start();
        if (result != ESP_OK)
        {
            DebugLog::printf("ESP32-S3 internal temperature sensor start failed: %d\r\n", result);
            return false;
        }

        temp_sensor_config_t appliedConfig = TSENS_CONFIG_DEFAULT();
        temp_sensor_get_config(&appliedConfig);
        DebugLog::printf("ESP32-S3 internal temperature sensor started: default dac=%d clk_div=%u\r\n",
                         static_cast<int>(appliedConfig.dac_offset),
                         appliedConfig.clk_div);
        return true;
    }

    float readInternalTemperatureC()
    {
        static bool sensorStarted = false;
        static bool sensorFailed = false;
        static uint8_t discardedSampleCount = 0;
        static uint32_t sensorStartedMs = 0;
        float temperatureC = NAN;

        if (sensorFailed)
        {
            return NAN;
        }

        if (!sensorStarted)
        {
            if (!startInternalTemperatureSensor())
            {
                sensorFailed = true;
                return NAN;
            }

            sensorStarted = true;
            sensorStartedMs = millis();
        }

        if (millis() - sensorStartedMs < 1000)
        {
            return NAN;
        }

        if (discardedSampleCount < 5)
        {
            temp_sensor_read_celsius(&temperatureC);
            ++discardedSampleCount;
            return NAN;
        }

        const esp_err_t result = temp_sensor_read_celsius(&temperatureC);
        if (result != ESP_OK)
        {
            DebugLog::printf("ESP32-S3 internal temperature default read failed: %d\r\n", result);
            return NAN;
        }

        return temperatureC;
    }
}

RadarApp::RadarApp() :
    settingsStore_(config_),
    tft_(),
    renderer_(tft_),
    apiTestView_(tft_)
{
}

void RadarApp::begin()
{
    DebugLog::begin(115200);
    delay(800);
    DebugLog::println();
    DebugLog::println("ESP32-S3 GC9A01 aircraft radar");
    loadDefaultRadarUiTuning(uiTuning_);
    renderer_.setUiTuning(&uiTuning_);
    settingsStore_.begin();
    const bool settingsLoaded = settingsStore_.load(settings_);
    if (!settingsLoaded)
    {
        DebugLog::println("Settings load fell back to sanitized defaults.");
    }
    inputManager_.begin(settings_);
    printSerialHelp();
    beginConfiguredMode();
}

void RadarApp::beginConfiguredMode()
{
    if (config_.appMode == AppMode::ApiTest)
    {
        if (!connectToConfiguredWiFi())
        {
            enterSetupPortal("WiFi setup required");
            return;
        }
        beginStaSettingsServer();
        beginApiTest();
        return;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        if (!connectToConfiguredWiFi())
        {
            enterSetupPortal("WiFi setup required");
            return;
        }
        beginStaSettingsServer();
        beginRealRadar();
        return;
    }

    beginRadarDemo();
}

void RadarApp::update()
{
    const uint32_t now = millis();
    updateInput();
    updateTemperatureLog(now);
    updateLongRunStatusLog(now);

    if (debugMode_ == DebugMode::UiLab)
    {
        updateUiLab(now);
        return;
    }

    if (screenSleeping_)
    {
        return;
    }

    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        updateLocalMenu(now);
        return;
    }

    if (configPortal_.isRunning() && configPortal_.mode() == ConfigPortalMode::StaSettings)
    {
        configPortal_.update();
        if (configPortal_.shouldRestart())
        {
            delay(300);
            ESP.restart();
        }
    }

    if (deviceState_ == DeviceState::SetupPortal)
    {
        updateSetupPortal(now);
        return;
    }

    if (config_.appMode == AppMode::ApiTest)
    {
        updateApiTest(now);
        return;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        updateRealRadar(now);
        return;
    }

    updateRadarDemo(now);
}

void RadarApp::idle()
{
    const uint32_t delayMs = computeIdleDelayMs();

    if (settings_.system.serialDebug && delayMs != lastIdleDelayMs_)
    {
        lastIdleDelayMs_ = delayMs;
        DebugLog::printf("[Idle] delay=%lums state=%s screenSleep=%u menu=%u\r\n",
                         static_cast<unsigned long>(delayMs),
                         deviceStateName(deviceState_),
                         screenSleeping_ ? 1 : 0,
                         localMenuPage_ != LocalMenuPage::Closed ? 1 : 0);
    }

    if (delayMs > 0)
    {
        delay(delayMs);
    }
}

uint32_t RadarApp::computeIdleDelayMs() const
{
    if (screenSleeping_)
    {
        return 50;
    }

    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        return 5;
    }

    if (deviceState_ == DeviceState::SetupPortal)
    {
        return 5;
    }

    if (configPortal_.isRunning() && configPortal_.mode() == ConfigPortalMode::StaSettings)
    {
        return 2;
    }

    if (deviceState_ == DeviceState::PausedBySchedule)
    {
        return 25;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        return 2;
    }

    if (config_.appMode == AppMode::ApiTest)
    {
        return 2;
    }

    if (config_.appMode == AppMode::RadarDemo)
    {
        return 5;
    }

    return 1;
}

void RadarApp::beginRadarDemo()
{
    setDeviceState(DeviceState::Running);
    DebugLog::println("Starting RadarDemo mode.");
    dataProvider_.begin();
    if (!renderer_.isReady())
    {
        renderer_.begin();
    }
    renderFrame();

    DebugLog::println("Fake aircraft radar UI is running.");
}

void RadarApp::beginApiTest()
{
    setDeviceState(DeviceState::Running);
    DebugLog::println("Starting ApiTest mode.");
    DebugLog::println("Logs use the upload USB/UART port at 115200 baud.");
    apiTestView_.begin();
    startWifiManagerFromSettings();
    renderApiTestScreen();
    printApiTestSerialStatus();
}

void RadarApp::beginRealRadar()
{
    DebugLog::println("Starting RealRadar mode.");
    DebugLog::printf("Radar center: lat=%.5f lon=%.5f range=%.0fkm\r\n",
                     settings_.location.centerLat,
                     settings_.location.centerLon,
                     settings_.location.maxRangeKm);
    DebugLog::printf("Provider: %s\r\n", apiProviderName(settings_.api.provider));
    DebugLog::printf("RealRadar filters: ground=%s minAlt=%.0fm minSpeed=%.1fm/s\r\n",
                     settings_.filter.showGroundTraffic ? "show" : "hide",
                     settings_.filter.minAirborneAltitudeM,
                     settings_.filter.minAirborneSpeedMs);
    if (settings_.api.provider == ApiProvider::OpenSky)
    {
        DebugLog::printf("OpenSky bbox: lat %.4f..%.4f lon %.4f..%.4f\r\n",
                         config_.openSkyLamin,
                         config_.openSkyLamax,
                         config_.openSkyLomin,
                         config_.openSkyLomax);
    }
    else if (settings_.api.provider == ApiProvider::AdsbFi)
    {
        DebugLog::printf("adsb.fi query center: lat=%.5f lon=%.5f range=%.0fkm\r\n",
                         settings_.location.centerLat,
                         settings_.location.centerLon,
                         settings_.location.maxRangeKm);
    }
    currentRealApiIntervalMs_ = activeRequestIntervalMs(settings_);
    DebugLog::printf("RealRadar API interval: %lu ms (%s)\r\n",
                     static_cast<unsigned long>(currentRealApiIntervalMs_),
                     refreshPolicyName(settings_.api.refreshPolicy));

    AircraftModel::clearAircraft(realAircraft_, AircraftModel::kAircraftCount);
    realAircraftCount_ = 0;
    realTrackManager_.begin();
    updateRealRadarStatus();

    if (!renderer_.isReady())
    {
        renderer_.begin();
    }
    startWifiManagerFromSettings();
    timeManager_.begin(settings_);
    lastScheduleCheckMs_ = 0;
    if (updateRealRadarRunGate(millis(), true))
    {
        renderRealRadarFrame();
    }
    else
    {
        renderRealRadarSystemStatus();
    }
}

void RadarApp::updateInput()
{
    inputManager_.update();

    InputEvent event = InputEvent::None;
    while (inputManager_.popEvent(event))
    {
        handleInputEvent(event);
    }

    UiTuningCommand command;
    while (inputManager_.popUiTuningCommand(command))
    {
        handleUiTuningCommand(command);
    }
}

void RadarApp::handleInputEvent(InputEvent event)
{
    if (isButtonInputEvent(event))
    {
        handleButtonInputEvent(event);
        return;
    }

    if (isBootButtonInputEvent(event))
    {
        handleBootButtonInputEvent(event);
        return;
    }

    switch (event)
    {
        case InputEvent::ShowHelp:
            printSerialHelp();
            break;

        case InputEvent::PrintSettings:
            printUserSettings(settings_);
            break;

        case InputEvent::EnterApSetup:
            enterSetupPortal("Serial command");
            break;

        case InputEvent::ShowStaSettings:
            showStaSettingsOverlay();
            break;

        case InputEvent::ToggleSettingsDisplay:
            toggleSetupDisplayMode();
            break;

        case InputEvent::ToggleUiLab:
#if ENABLE_UI_LAB
            toggleUiLab();
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case InputEvent::NextUiLabScene:
#if ENABLE_UI_LAB
            nextUiLabScene();
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case InputEvent::PrintUiTuning:
#if ENABLE_UI_LAB
#if ENABLE_UI_LAB_ADVANCED_TUNING
            printRadarUiTuning(uiTuning_);
#else
            printUiLabStatus();
#endif
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case InputEvent::SaveUiTuning:
#if ENABLE_UI_LAB
            saveUiTuning();
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case InputEvent::ResetUiTuning:
#if ENABLE_UI_LAB
            resetUiTuning();
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case InputEvent::ExitCurrentView:
            if (deviceState_ == DeviceState::SetupPortal)
            {
                exitSetupPortal();
            }
            else if (staSettingsOverlayVisible_)
            {
                hideStaSettingsOverlay();
            }
            break;

        case InputEvent::NextUiTheme:
            if (debugMode_ == DebugMode::UiLab)
            {
                nextUiLabTheme();
            }
            else
            {
                switchUiTheme();
                if (deviceState_ == DeviceState::PausedBySchedule)
                {
                    startIdleUiPreview(millis(), kIdleThemePreviewMs, config_.frameIntervalMs);
                }
            }
            break;

        case InputEvent::SwitchRange:
            switchRange();
            break;

        case InputEvent::ToggleGroundTraffic:
            toggleGroundTraffic();
            break;

        case InputEvent::CycleScheduleIdleDisplayMode:
            cycleScheduleIdleDisplayMode();
            break;

        case InputEvent::SaveSettings:
            DebugLog::println("Manual settings save requested.");
            settingsStore_.save(settings_);
            break;

        case InputEvent::LoadSettings:
            DebugLog::println("Manual settings load requested.");
            settingsStore_.load(settings_);
            inputManager_.begin(settings_);
            if (config_.appMode == AppMode::RealRadar)
            {
                sanitizeUserSettings(settings_);
                timeManager_.begin(settings_);
                stopRealRadarUpdater();
                updateRealRadarRunGate(millis(), true);
                if (deviceState_ == DeviceState::PausedBySchedule)
                {
                    renderPausedIdleFrame(true);
                }
                else
                {
                    renderRealRadarSystemStatus();
                }
            }
            break;

        case InputEvent::ResetDefaults:
            resetSettingsToDefault();
            break;

        case InputEvent::PrintTimeStatus:
            printTimeStatus();
            break;

        case InputEvent::PrintDeviceStatus:
            printDeviceStateStatus();
            break;

        case InputEvent::PrintApiAuthStatus:
            printApiAuthStatus();
            break;

        case InputEvent::ClearAuthToken:
            clearAuthToken();
            break;

        case InputEvent::Reboot:
            DebugLog::println("Reboot requested from serial.");
            delay(100);
            ESP.restart();
            break;

        case InputEvent::KeyUpShort:
        case InputEvent::KeyUpLong:
        case InputEvent::KeyUpDouble:
        case InputEvent::KeyDownShort:
        case InputEvent::KeyDownLong:
        case InputEvent::KeyDownDouble:
            break;

        case InputEvent::None:
        default:
            break;
    }
}

void RadarApp::handleButtonInputEvent(InputEvent event)
{
    if (screenSleeping_)
    {
        wakeScreenFromSleep();
        return;
    }

    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        handleLocalMenuButtonEvent(event);
        return;
    }

    switch (event)
    {
        case InputEvent::KeyUpShort:
            if (staSettingsOverlayVisible_ || deviceState_ == DeviceState::SetupPortal)
            {
                toggleSetupDisplayMode();
            }
            else if (debugMode_ == DebugMode::UiLab)
            {
                nextUiLabTheme();
            }
            else
            {
                switchUiTheme();
                if (deviceState_ == DeviceState::PausedBySchedule)
                {
                    startIdleUiPreview(millis(), kIdleThemePreviewMs, config_.frameIntervalMs);
                }
            }
            break;

        case InputEvent::KeyDownShort:
            if (staSettingsOverlayVisible_ || deviceState_ == DeviceState::SetupPortal)
            {
                toggleSetupDisplayMode();
            }
            else
            {
                switchRange();
                if (deviceState_ == DeviceState::PausedBySchedule)
                {
                    startIdleUiPreview(millis(),
                                       kIdleRangePreviewMs,
                                       kIdleRangePreviewRefreshMs,
                                       kIdleRangePreviewApiIntervalMs);
                }
            }
            break;

        case InputEvent::KeyUpLong:
        case InputEvent::KeyDownLong:
            if (deviceState_ == DeviceState::SetupPortal)
            {
                toggleSetupDisplayMode();
            }
            else
            {
                openLocalMenu();
            }
            break;

        case InputEvent::KeyUpDouble:
            if (deviceState_ == DeviceState::SetupPortal || staSettingsOverlayVisible_)
            {
                toggleSetupDisplayMode();
            }
            else
            {
                DebugLog::println("KEY_UP double press: no action in current view.");
            }
            break;

        case InputEvent::KeyDownDouble:
            if (deviceState_ == DeviceState::SetupPortal)
            {
                exitSetupPortal();
            }
            else if (staSettingsOverlayVisible_)
            {
                hideStaSettingsOverlay();
            }
            else
            {
                DebugLog::println("KEY_DOWN double press: no action in current view.");
            }
            break;

        case InputEvent::None:
        default:
            break;
    }
}

void RadarApp::handleBootButtonInputEvent(InputEvent event)
{
    if (screenSleeping_)
    {
        wakeScreenFromSleep();
        return;
    }

    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        DebugLog::println("[Input] BOOT ignored: local menu is open.");
        return;
    }

    switch (event)
    {
        case InputEvent::BootButtonShort:
            if (hasActiveOverlay())
            {
                DebugLog::println("[Input] BOOT short ignored: settings view is active.");
                return;
            }

            DebugLog::println("[Input] BOOT short: next UI theme");
            switchUiTheme();
            if (deviceState_ == DeviceState::PausedBySchedule)
            {
                startIdleUiPreview(millis(), kIdleThemePreviewMs, config_.frameIntervalMs);
            }
            break;

        case InputEvent::BootButtonDouble:
            if (hasActiveOverlay())
            {
                DebugLog::println("[Input] BOOT double ignored: settings view is active.");
                return;
            }

            DebugLog::println("[Input] BOOT double: next range");
            switchRange();
            if (deviceState_ == DeviceState::PausedBySchedule)
            {
                startIdleUiPreview(millis(),
                                   kIdleRangePreviewMs,
                                   kIdleRangePreviewRefreshMs,
                                   kIdleRangePreviewApiIntervalMs);
            }
            break;

        case InputEvent::BootButtonLong:
            DebugLog::println("[Input] BOOT long: open settings QR");
            openSettingsEntryFromBootButton();
            break;

        case InputEvent::None:
        default:
            break;
    }
}

void RadarApp::wakeScreenFromSleep()
{
    screenSleeping_ = false;
    lastIdleDisplayRenderMs_ = millis();
    DebugLog::println("Screen sleep wake by button input.");

    if (!renderer_.isReady())
    {
        return;
    }

    if (deviceState_ == DeviceState::PausedBySchedule &&
        settings_.schedule.idleDisplayMode == ScheduleIdleDisplayMode::DisplayOff)
    {
        renderer_.renderSystemStatusFrame("AWAKE", "Button input", "");
        return;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        renderRealRadarFrame();
    }
    else if (config_.appMode == AppMode::RadarDemo)
    {
        renderFrame();
    }
    else
    {
        renderApiTestScreen();
    }
}

void RadarApp::openSettingsEntryFromBootButton()
{
    if (wifi_.isConnected())
    {
        showStaSettingsOverlay();
        return;
    }

    screenSleeping_ = false;
    setupPortalFromLocalMenu_ = true;
    enterSetupPortal("BOOT button");
}

void RadarApp::openLocalMenu()
{
    if (staSettingsOverlayVisible_)
    {
        hideStaSettingsOverlay();
    }

    localMenuPage_ = LocalMenuPage::MainMenu;
    localMenuIndex_ = 0;
    localMenuBrightnessIndex_ = brightnessIndexFromValue(settings_.display.brightness);
    localMenuIdleDisplayMode_ = settings_.schedule.idleDisplayMode;
    localMenuOpenedMs_ = millis();
    localMenuLastInputMs_ = millis();
    localMenuLastRenderMs_ = 0;
    DebugLog::println("Local menu opened.");
    renderLocalMenu();
}

void RadarApp::closeLocalMenu(bool restoreDisplay)
{
    if (localMenuPage_ == LocalMenuPage::Closed)
    {
        return;
    }

    localMenuPage_ = LocalMenuPage::Closed;
    localMenuOpenedMs_ = 0;
    localMenuLastInputMs_ = 0;
    localMenuLastRenderMs_ = 0;
    DebugLog::println("Local menu closed.");

    if (restoreDisplay)
    {
        renderCurrentDisplay();
    }
}

void RadarApp::updateLocalMenu(uint32_t now)
{
    const uint32_t menuNow = millis();
    (void)now;

    if (localMenuLastInputMs_ != 0 && menuNow - localMenuLastInputMs_ >= kLocalMenuTimeoutMs)
    {
        DebugLog::println("Local menu timeout.");
        closeLocalMenu(true);
        return;
    }

    if (localMenuLastRenderMs_ == 0 || menuNow - localMenuLastRenderMs_ >= kLocalMenuRefreshMs)
    {
        renderLocalMenu();
    }
}

void RadarApp::handleLocalMenuButtonEvent(InputEvent event)
{
    const uint32_t now = millis();
    localMenuLastInputMs_ = now;

    if (event == InputEvent::KeyUpLong || event == InputEvent::KeyDownLong)
    {
        if (localMenuOpenedMs_ != 0 && now - localMenuOpenedMs_ < kLocalMenuOpenLongGuardMs)
        {
            DebugLog::println("Local menu: ignored repeated opening long press.");
            renderLocalMenu();
            return;
        }

        if (localMenuPage_ == LocalMenuPage::ConfirmReboot ||
            localMenuPage_ == LocalMenuPage::ConfirmApSetup ||
            localMenuPage_ == LocalMenuPage::BrightnessAdjust ||
            localMenuPage_ == LocalMenuPage::IdleDisplayAdjust)
        {
            localMenuPage_ = LocalMenuPage::MainMenu;
            renderLocalMenu();
            return;
        }

        closeLocalMenu(true);
        return;
    }

    switch (localMenuPage_)
    {
        case LocalMenuPage::MainMenu:
            if (event == InputEvent::KeyUpShort)
            {
                localMenuIndex_ = localMenuIndex_ == 0 ? kLocalMenuItemCount - 1 : localMenuIndex_ - 1;
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyDownShort)
            {
                localMenuIndex_ = (localMenuIndex_ + 1) % kLocalMenuItemCount;
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyUpDouble)
            {
                executeLocalMenuItem();
            }
            else if (event == InputEvent::KeyDownDouble)
            {
                closeLocalMenu(true);
            }
            break;

        case LocalMenuPage::BrightnessAdjust:
            if (event == InputEvent::KeyUpShort)
            {
                if (localMenuBrightnessIndex_ < kBrightnessLevelCount - 1)
                {
                    ++localMenuBrightnessIndex_;
                }
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyDownShort)
            {
                if (localMenuBrightnessIndex_ > 0)
                {
                    --localMenuBrightnessIndex_;
                }
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyUpDouble)
            {
                settings_.display.brightness = brightnessValueFromIndex(localMenuBrightnessIndex_);
                sanitizeUserSettings(settings_);
                settingsStore_.save(settings_);
                DebugLog::printf("Brightness saved: %u\r\n", settings_.display.brightness);
                localMenuPage_ = LocalMenuPage::MainMenu;
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyDownDouble)
            {
                localMenuBrightnessIndex_ = brightnessIndexFromValue(settings_.display.brightness);
                localMenuPage_ = LocalMenuPage::MainMenu;
                renderLocalMenu();
            }
            break;

        case LocalMenuPage::IdleDisplayAdjust:
            if (event == InputEvent::KeyUpShort)
            {
                localMenuIdleDisplayMode_ = previousIdleDisplayMode(localMenuIdleDisplayMode_);
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyDownShort)
            {
                localMenuIdleDisplayMode_ = nextScheduleIdleDisplayMode(localMenuIdleDisplayMode_);
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyUpDouble)
            {
                settings_.schedule.idleDisplayMode = localMenuIdleDisplayMode_;
                sanitizeUserSettings(settings_);
                settingsStore_.save(settings_);
                DebugLog::printf("Idle display saved: %s\r\n",
                                 scheduleIdleDisplayModeName(settings_.schedule.idleDisplayMode));
                localMenuPage_ = LocalMenuPage::MainMenu;
                renderLocalMenu();
            }
            else if (event == InputEvent::KeyDownDouble)
            {
                localMenuIdleDisplayMode_ = settings_.schedule.idleDisplayMode;
                localMenuPage_ = LocalMenuPage::MainMenu;
                renderLocalMenu();
            }
            break;

        case LocalMenuPage::ConfirmReboot:
            if (event == InputEvent::KeyUpDouble)
            {
                DebugLog::println("Reboot confirmed from local menu.");
                delay(100);
                ESP.restart();
            }
            else if (event == InputEvent::KeyDownDouble)
            {
                localMenuPage_ = LocalMenuPage::MainMenu;
                renderLocalMenu();
            }
            break;

        case LocalMenuPage::ConfirmApSetup:
            if (event == InputEvent::KeyUpDouble)
            {
                closeLocalMenu(false);
                setupPortalFromLocalMenu_ = true;
                enterSetupPortal("Local menu");
            }
            else if (event == InputEvent::KeyDownDouble)
            {
                localMenuPage_ = LocalMenuPage::MainMenu;
                renderLocalMenu();
            }
            break;

        case LocalMenuPage::Closed:
        default:
            break;
    }
}

void RadarApp::executeLocalMenuItem()
{
    switch (static_cast<LocalMenuItem>(localMenuIndex_))
    {
        case LocalMenuItem::SettingsQr:
            closeLocalMenu(false);
            if (wifi_.isConnected())
            {
                showStaSettingsOverlay();
            }
            else
            {
                screenSleeping_ = false;
                renderer_.renderSystemStatusFrame("NO WIFI", "WiFi not connected", "Use AP Setup");
            }
            break;

        case LocalMenuItem::Reboot:
            localMenuPage_ = LocalMenuPage::ConfirmReboot;
            renderLocalMenu();
            break;

        case LocalMenuItem::ScreenSleep:
            closeLocalMenu(false);
            screenSleeping_ = true;
            renderer_.renderBlankFrame();
            DebugLog::println("Screen sleep entered from local menu.");
            break;

        case LocalMenuItem::Brightness:
            localMenuBrightnessIndex_ = brightnessIndexFromValue(settings_.display.brightness);
            localMenuPage_ = LocalMenuPage::BrightnessAdjust;
            renderLocalMenu();
            break;

        case LocalMenuItem::IdleDisplay:
            localMenuIdleDisplayMode_ = settings_.schedule.idleDisplayMode;
            localMenuPage_ = LocalMenuPage::IdleDisplayAdjust;
            renderLocalMenu();
            break;

        case LocalMenuItem::ApSetup:
            localMenuPage_ = LocalMenuPage::ConfirmApSetup;
            renderLocalMenu();
            break;

        case LocalMenuItem::Exit:
        default:
            closeLocalMenu(true);
            break;
    }
}

void RadarApp::renderLocalMenu()
{
    if (!renderer_.isReady())
    {
        return;
    }

    screenSleeping_ = false;
    localMenuLastRenderMs_ = millis();
    renderLocalMenuPage();
}

void RadarApp::renderLocalMenuPage()
{
    switch (localMenuPage_)
    {
        case LocalMenuPage::MainMenu:
            renderer_.renderLocalMenuFrame("LOCAL MENU",
                                           kLocalMenuItems,
                                           kLocalMenuItemCount,
                                           localMenuIndex_,
                                           "UP/DN Move");
            break;

        case LocalMenuPage::BrightnessAdjust:
        {
            char valueText[16];
            snprintf(valueText, sizeof(valueText), "%s", kBrightnessLabels[localMenuBrightnessIndex_]);
            renderer_.renderLocalAdjustFrame("BRIGHTNESS",
                                             valueText,
                                             "UP/DN Change",
                                             "UP2 OK DN2 Back");
            break;
        }

        case LocalMenuPage::IdleDisplayAdjust:
            renderer_.renderLocalAdjustFrame("IDLE DISPLAY",
                                             idleDisplayMenuName(localMenuIdleDisplayMode_),
                                             "UP/DN Change",
                                             "UP2 OK DN2 Back");
            break;

        case LocalMenuPage::ConfirmReboot:
            renderer_.renderLocalConfirmFrame("REBOOT",
                                              "Restart device?",
                                              "UP2 Reboot",
                                              "DN2 Back");
            break;

        case LocalMenuPage::ConfirmApSetup:
            renderer_.renderLocalConfirmFrame("AP SETUP",
                                              "Start setup AP?",
                                              "UP2 Start",
                                              "DN2 Back");
            break;

        case LocalMenuPage::Closed:
        default:
            break;
    }
}

void RadarApp::renderCurrentDisplay()
{
    if (!renderer_.isReady())
    {
        return;
    }

    if (staSettingsOverlayVisible_)
    {
        renderSettingsDisplay(nullptr);
        return;
    }

    if (deviceState_ == DeviceState::SetupPortal)
    {
        renderSetupPortalFrame(nullptr);
        return;
    }

    if (deviceState_ == DeviceState::PausedBySchedule)
    {
        renderPausedIdleFrame(true);
        return;
    }

    if (config_.appMode == AppMode::ApiTest)
    {
        renderApiTestScreen();
    }
    else if (config_.appMode == AppMode::RealRadar)
    {
        renderRealRadarFrame();
    }
    else
    {
        renderFrame();
    }
}

bool RadarApp::isIdleUiPreviewActive(uint32_t now) const
{
    return idleUiPreviewUntilMs_ != 0 &&
           static_cast<int32_t>(idleUiPreviewUntilMs_ - now) > 0;
}

void RadarApp::startIdleUiPreview(uint32_t now, uint32_t durationMs, uint32_t refreshMs, uint32_t apiIntervalMs)
{
    idleUiPreviewUntilMs_ = now + durationMs;
    idleUiPreviewRefreshMs_ = refreshMs > 0 ? refreshMs : config_.frameIntervalMs;
    idleUiPreviewApiIntervalMs_ = apiIntervalMs;
    lastFrameMs_ = 0;
    DebugLog::printf("Idle UI preview: duration=%lu ms refresh=%lu ms api=%lu ms\r\n",
                     static_cast<unsigned long>(durationMs),
                     static_cast<unsigned long>(idleUiPreviewRefreshMs_),
                     static_cast<unsigned long>(idleUiPreviewApiIntervalMs_));
    renderIdleUiPreviewFrame();
}

void RadarApp::updateIdleRealRadarPreview(uint32_t now, bool previewActive)
{
    if (!previewActive)
    {
        idleUiPreviewUntilMs_ = 0;
        idleUiPreviewRefreshMs_ = 0;
        idleUiPreviewApiIntervalMs_ = 0;
        stopRealRadarUpdater();
        return;
    }

    if (idleUiPreviewApiIntervalMs_ > 0)
    {
        bool intervalChanged = false;
        if (currentRealApiIntervalMs_ != idleUiPreviewApiIntervalMs_)
        {
            currentRealApiIntervalMs_ = idleUiPreviewApiIntervalMs_;
            intervalChanged = true;
            DebugLog::printf("Idle preview API interval: %lu ms\r\n",
                             static_cast<unsigned long>(currentRealApiIntervalMs_));
        }

        if (!realApiUpdater_.isRunning())
        {
            if (realApiUpdater_.begin(config_, settings_, currentRealApiIntervalMs_))
            {
                DebugLog::println("Idle preview API updater running.");
            }
        }
        else if (intervalChanged)
        {
            realApiUpdater_.begin(config_, settings_, currentRealApiIntervalMs_);
        }
    }

    OpenSkySnapshot snapshot;
    if (realApiUpdater_.copySnapshot(snapshot))
    {
        handleRealRadarSnapshot(snapshot, now);
    }

    realTrackManager_.updatePrediction(settings_, now);
    RealRadarTrackStats frameStats;
    rebuildRealRadarAircraft(frameStats);
    updateSelectedAircraftForList(realAircraft_, realAircraftCount_, now);
    updateRealRadarStatus();
}

void RadarApp::renderIdleUiPreviewFrame()
{
    if (!renderer_.isReady())
    {
        return;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        renderRealRadarFrame();
    }
    else if (config_.appMode == AppMode::RadarDemo)
    {
        renderFrame();
    }
    else
    {
        renderApiTestScreen();
    }
}

uint8_t RadarApp::brightnessIndexFromValue(uint8_t brightness) const
{
    uint8_t bestIndex = 0;
    uint8_t bestDelta = 255;
    for (uint8_t i = 0; i < kBrightnessLevelCount; ++i)
    {
        const uint8_t level = kBrightnessValues[i];
        const uint8_t delta = brightness > level ? brightness - level : level - brightness;
        if (delta < bestDelta)
        {
            bestDelta = delta;
            bestIndex = i;
        }
    }
    return bestIndex;
}

uint8_t RadarApp::brightnessValueFromIndex(uint8_t index) const
{
    if (index >= kBrightnessLevelCount)
    {
        index = kBrightnessLevelCount - 1;
    }
    return kBrightnessValues[index];
}

const char *RadarApp::idleDisplayMenuName(ScheduleIdleDisplayMode mode) const
{
    switch (mode)
    {
        case ScheduleIdleDisplayMode::PausedStatus:
            return "Paused";
        case ScheduleIdleDisplayMode::Clock:
            return "Clock";
        case ScheduleIdleDisplayMode::DisplayOff:
        default:
            return "Sleep / Black";
    }
}

void RadarApp::printSerialHelp()
{
    DebugLog::println("Serial input: virtual buttons only");
#if ENABLE_SERIAL_VIRTUAL_BUTTONS
    DebugLog::println("  btn up short      - simulate KEY_UP short press");
    DebugLog::println("  btn up long       - simulate KEY_UP long press");
    DebugLog::println("  btn up double     - simulate KEY_UP double click");
    DebugLog::println("  btn down short    - simulate KEY_DOWN short press");
    DebugLog::println("  btn down long     - simulate KEY_DOWN long press");
    DebugLog::println("  btn down double   - simulate KEY_DOWN double click");
    DebugLog::println("  bu short|long|double");
    DebugLog::println("  bd short|long|double");
#else
    DebugLog::println("  virtual button commands disabled (ENABLE_SERIAL_VIRTUAL_BUTTONS=0)");
#endif

#if !ENABLE_SERIAL_DEBUG_COMMANDS
    DebugLog::println("Debug serial commands are disabled in this build.");
    return;
#else
    DebugLog::println("Debug serial commands:");
    DebugLog::println("  h: help");
    DebugLog::println("  p: print UserSettings");
    DebugLog::println("  c: enter setup portal");
    DebugLog::println("  x: exit setup portal");
    DebugLog::println("  q: toggle setup QR/details");
    DebugLog::println("  w: show STA settings URL QR");
    DebugLog::println("  u: switch UI theme");
#if ENABLE_UI_LAB
    DebugLog::println("UI Lab:");
    DebugLog::println("  y: toggle UI Lab");
    DebugLog::println("  f: next UI Lab fake scene");
    DebugLog::println("  u: next UI theme while in UI Lab");
    DebugLog::println("  j: print UI Lab/display state");
    DebugLog::println("  n: reset UI Lab runtime state");
#if ENABLE_UI_LAB_ADVANCED_TUNING
    DebugLog::println("UI Lab advanced tuning:");
    DebugLog::println("  k: save UI tuning (not persisted yet)");
    DebugLog::println("  set modern.bg R G B");
    DebugLog::println("  set modern.grid R G B");
    DebugLog::println("  set modern.globalBrightness VALUE");
    DebugLog::println("  set modern.outerRadius VALUE");
    DebugLog::println("  set cyber.bg R G B");
    DebugLog::println("  set cyber.outer|ring|tick|magenta R G B");
    DebugLog::println("  set cyber.aircraft|aircraftGlow R G B");
    DebugLog::println("  set cyber.sweep R G B");
    DebugLog::println("  set cyber.globalBrightness VALUE");
    DebugLog::println("  set cyber.ring|text|aircraft|sweepBrightness VALUE");
    DebugLog::println("  set cyber.outerRadius VALUE");
    DebugLog::println("  set cyber.innerRadarRadius|ringCount|lineWidth VALUE");
    DebugLog::println("  set cyber.aircraftScale|vectorScale VALUE");
    DebugLog::println("  set cyber.sweepWidth|sweepTrailStrength VALUE");
    DebugLog::println("  set cyber.mapBrightness VALUE");
    DebugLog::println("  set cyber.mapDensity VALUE");
    DebugLog::println("  set cyber.radialGridBrightness VALUE");
    DebugLog::println("  set cyber.outerTickStepDeg VALUE");
    DebugLog::println("  set cyber.bearingLabelRadiusOffset VALUE");
    DebugLog::println("  set cyber.cardinalRadiusOffset VALUE");
    DebugLog::println("  set cyber.outerGlowBrightness VALUE");
    DebugLog::println("  set cyber.outerGlowWidth VALUE");
    DebugLog::println("  set modern|cyber.showStatusText 0/1");
    DebugLog::println("  set modern|cyber.showLeaderLines 0/1");
    DebugLog::println("  set modern|cyber.maxLabels VALUE");
#else
    DebugLog::println("  advanced set commands hidden (ENABLE_UI_LAB_ADVANCED_TUNING=0)");
#endif
#endif
    DebugLog::println("  r: switch radar range");
    DebugLog::println("  g: toggle ground traffic");
    DebugLog::println("  o: cycle outside-schedule display");
    DebugLog::println("  s: save settings");
    DebugLog::println("  l: load settings");
    DebugLog::println("  t: print time/schedule status");
    DebugLog::println("  m: print device mode/state");
    DebugLog::println("  a: print API/auth status");
    DebugLog::println("  A: clear OpenSky token");
    DebugLog::println("  d: reset settings to default");
    DebugLog::println("  b: reboot device");
#endif
}

void RadarApp::toggleUiLab()
{
    if (debugMode_ == DebugMode::UiLab)
    {
        exitUiLab();
        return;
    }

    enterUiLab();
}

void RadarApp::enterUiLab()
{
    uiLabPreviousDeviceState_ = deviceState_;
    stopRealRadarUpdater();
    staSettingsOverlayVisible_ = false;
    uiLabTheme_ = settings_.display.uiTheme;
    uiLabSceneIndex_ = 0;
    loadUiLabScene(uiLabSceneIndex_);
    selectedAircraftIndex_ = 0;
    debugMode_ = DebugMode::UiLab;
    lastFrameMs_ = 0;

    if (!renderer_.isReady())
    {
        renderer_.begin();
    }

    DebugLog::println("UI Lab entered. WiFi/API/schedule are bypassed.");
    DebugLog::printf("UI Lab theme: %s scene=%u\r\n", uiThemeName(uiLabTheme_), uiLabSceneIndex_);
    renderUiLabFrame();
}

void RadarApp::exitUiLab()
{
    debugMode_ = DebugMode::None;
    DebugLog::println("UI Lab exited.");

    if (deviceState_ == DeviceState::SetupPortal)
    {
        renderSetupPortalFrame(nullptr);
        return;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        lastScheduleCheckMs_ = 0;
        if (updateRealRadarRunGate(millis(), true))
        {
            renderRealRadarFrame();
        }
        else
        {
            renderRealRadarSystemStatus();
        }
        return;
    }

    if (config_.appMode == AppMode::ApiTest)
    {
        renderApiTestScreen();
        return;
    }

    renderFrame();
}

void RadarApp::updateUiLab(uint32_t now)
{
    if (now - lastFrameMs_ < config_.frameIntervalMs)
    {
        return;
    }

    lastFrameMs_ = now;
    renderUiLabFrame();
    renderer_.advanceSweep(config_.sweepStepDeg);
}

void RadarApp::renderUiLabFrame()
{
    AppConfig renderConfig = runtimeRenderConfig();
    renderConfig.maxRangeKm = 60.0f;
    renderConfig.showLabels = true;
    renderer_.renderRadarFrame(uiLabAircraft_,
                               uiLabAircraftCount_,
                               selectedAircraftIndex_,
                               renderConfig,
                               uiLabTheme_,
                               "UI LAB");
}

void RadarApp::loadUiLabScene(uint8_t sceneIndex)
{
    AircraftModel::clearAircraft(uiLabAircraft_, AircraftModel::kAircraftCount);
    uiLabAircraftCount_ = AircraftModel::kAircraftCount;

    if (sceneIndex % 2 == 0)
    {
        AircraftModel::setAircraft(uiLabAircraft_[0], "ANA123", 8.0f, 20.0f, 1200.0f, 130.0f, 80.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[1], "JAL456", 18.0f, 90.0f, 2500.0f, 160.0f, 140.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[2], "SKY729", 26.0f, 50.0f, 2476.0f, 150.0f, 110.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[3], "SFJ897", 14.0f, 130.0f, 1547.0f, 120.0f, 300.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[4], "CES320", 35.0f, 210.0f, 5200.0f, 180.0f, 260.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[5], "CCA998", 48.0f, 300.0f, 6900.0f, 210.0f, 20.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[6], "APJ501", 5.0f, 270.0f, 800.0f, 90.0f, 315.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[7], "FDA642", 58.0f, 350.0f, 8300.0f, 230.0f, 45.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[8], "SNA210", 42.0f, 160.0f, 4100.0f, 145.0f, 190.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[9], "ADO777", 62.0f, 75.0f, 9100.0f, 240.0f, 120.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[10], "IBX331", 22.0f, 240.0f, 3100.0f, 110.0f, 280.0f, true);
        AircraftModel::setAircraft(uiLabAircraft_[11], "JTA082", 12.0f, 330.0f, 2300.0f, 135.0f, 10.0f, true);
        return;
    }

    AircraftModel::setAircraft(uiLabAircraft_[0], "NEAR01", 3.0f, 5.0f, 600.0f, 60.0f, 45.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[1], "EAST22", 12.0f, 88.0f, 1400.0f, 80.0f, 90.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[2], "WEST33", 12.0f, 272.0f, 1500.0f, 80.0f, 270.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[3], "SOUTH4", 25.0f, 180.0f, 3300.0f, 130.0f, 220.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[4], "NORTH5", 25.0f, 0.0f, 3500.0f, 140.0f, 20.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[5], "EDGE66", 59.0f, 45.0f, 7800.0f, 220.0f, 90.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[6], "EDGE77", 59.0f, 135.0f, 7900.0f, 220.0f, 180.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[7], "EDGE88", 59.0f, 225.0f, 8000.0f, 220.0f, 270.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[8], "EDGE99", 59.0f, 315.0f, 8100.0f, 220.0f, 0.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[9], "OUT101", 72.0f, 60.0f, 9000.0f, 250.0f, 80.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[10], "OUT202", 75.0f, 250.0f, 9200.0f, 245.0f, 260.0f, true);
    AircraftModel::setAircraft(uiLabAircraft_[11], "LAB303", 34.0f, 295.0f, 4200.0f, 150.0f, 330.0f, true);
}

void RadarApp::nextUiLabTheme()
{
    if (debugMode_ != DebugMode::UiLab)
    {
        return;
    }

    uiLabTheme_ = nextUiTheme(uiLabTheme_);
    DebugLog::printf("UI Lab theme: %s\r\n", uiThemeName(uiLabTheme_));
    renderUiLabFrame();
}

void RadarApp::nextUiLabScene()
{
    if (debugMode_ != DebugMode::UiLab)
    {
        DebugLog::println("f ignored: enter UI Lab with y first.");
        return;
    }

    uiLabSceneIndex_ = (uiLabSceneIndex_ + 1) % 2;
    loadUiLabScene(uiLabSceneIndex_);
    DebugLog::printf("UI Lab scene: %u\r\n", uiLabSceneIndex_);
    renderUiLabFrame();
}

void RadarApp::resetUiTuning()
{
    loadDefaultRadarUiTuning(uiTuning_);
    renderer_.setUiTuning(&uiTuning_);
    DebugLog::println("UI tuning reset to defaults.");
#if ENABLE_UI_LAB_ADVANCED_TUNING
    printRadarUiTuning(uiTuning_);
#else
    printUiLabStatus();
#endif
    if (debugMode_ == DebugMode::UiLab)
    {
        renderUiLabFrame();
    }
}

void RadarApp::saveUiTuning()
{
#if ENABLE_UI_LAB_ADVANCED_TUNING
    DebugLog::println("UI tuning NVS save is not implemented in this first UI Lab version.");
    DebugLog::println("Use j to print values, then copy the chosen defaults into RadarUiTuning.cpp.");
    printRadarUiTuning(uiTuning_);
#else
    DebugLog::println("UI tuning save is hidden because advanced tuning is disabled.");
#endif
}

void RadarApp::printUiLabStatus()
{
    const SystemStatus status = getSystemStatus();
    DebugLog::println("UI Lab/display state:");
    DebugLog::printf("  uiLab=%u theme=%s scene=%u\r\n",
                     status.uiLabRunning ? 1 : 0,
                     status.uiLabRunning ? uiThemeName(uiLabTheme_) : uiThemeName(settings_.display.uiTheme),
                     uiLabSceneIndex_);
    DebugLog::printf("  display theme=%s labels=%u maxAircraft=%u brightness=%u\r\n",
                     uiThemeName(settings_.display.uiTheme),
                     settings_.display.showLabels ? 1 : 0,
                     settings_.display.maxAircraftToDisplay,
                     settings_.display.brightness);
    DebugLog::printf("  range=%.0fkm ground=%u aircraft=%u\r\n",
                     settings_.location.maxRangeKm,
                     settings_.filter.showGroundTraffic ? 1 : 0,
                     status.aircraftCount);
    DebugLog::printf("  state=%s wifi=%u updater=%u ntp=%u schedule=%u\r\n",
                     deviceStateName(status.deviceState),
                     status.wifiConnected ? 1 : 0,
                     status.apiUpdaterRunning ? 1 : 0,
                     status.ntpSynced ? 1 : 0,
                     status.withinSchedule ? 1 : 0);
}

void RadarApp::handleUiTuningCommand(const UiTuningCommand &command)
{
#if !(ENABLE_UI_LAB && ENABLE_UI_LAB_ADVANCED_TUNING)
    (void)command;
#if !ENABLE_UI_LAB
    DebugLog::println("UI Lab is disabled in this build.");
#else
    DebugLog::println("Advanced UI tuning is disabled in this build.");
    DebugLog::println("Set ENABLE_UI_LAB_ADVANCED_TUNING=1 to enable set commands.");
#endif
    return;
#else
    if (applyUiTuningColor(command.key, command) || applyUiTuningValue(command.key, command))
    {
        sanitizeRadarUiTuning(uiTuning_);
        DebugLog::printf("UI tuning updated: %s\r\n", command.key);
        if (debugMode_ == DebugMode::UiLab)
        {
            renderUiLabFrame();
        }
        else
        {
            DebugLog::println("Runtime tuning updated. Enter UI Lab with y or switch theme to preview it.");
        }
        return;
    }

    DebugLog::printf("Unknown UI tuning key: %s\r\n", command.key);
#endif
}

bool RadarApp::applyUiTuningColor(const char *key, const UiTuningCommand &command)
{
    if (command.valueCount != 3)
    {
        return false;
    }

    RgbColor *color = nullptr;
    if (keyMatches(key, "modern.bg", "modern.background", "bg"))
    {
        color = &uiTuning_.modern.background;
    }
    else if (keyMatches(key, "modern.grid", "grid"))
    {
        color = &uiTuning_.modern.grid;
    }
    else if (keyMatches(key, "modern.text", "text"))
    {
        color = &uiTuning_.modern.text;
    }
    else if (keyMatches(key, "modern.aircraft", "aircraft"))
    {
        color = &uiTuning_.modern.aircraft;
    }
    else if (keyMatches(key, "modern.vector", "vector"))
    {
        color = &uiTuning_.modern.vector;
    }
    else if (keyMatches(key, "modern.altitude", "modern.altitudeText", "altitude"))
    {
        color = &uiTuning_.modern.altitudeText;
    }
    else if (keyMatches(key, "modern.selected", "selected"))
    {
        color = &uiTuning_.modern.selected;
    }
    else if (keyMatches(key, "cyber.bg", "cyber.background"))
    {
        color = &uiTuning_.cyberpunk.background;
    }
    else if (keyMatches(key, "cyber.noise", "cyber.backgroundNoise"))
    {
        color = &uiTuning_.cyberpunk.backgroundNoise;
    }
    else if (keyMatches(key, "cyber.outer", "cyber.outerRing"))
    {
        color = &uiTuning_.cyberpunk.outerRing;
    }
    else if (keyMatches(key, "cyber.ring"))
    {
        color = &uiTuning_.cyberpunk.ring;
    }
    else if (keyMatches(key, "cyber.ringDim"))
    {
        color = &uiTuning_.cyberpunk.ringDim;
    }
    else if (keyMatches(key, "cyber.cross", "cyber.crosshair"))
    {
        color = &uiTuning_.cyberpunk.crosshair;
    }
    else if (keyMatches(key, "cyber.tick"))
    {
        color = &uiTuning_.cyberpunk.tick;
    }
    else if (keyMatches(key, "cyber.magenta"))
    {
        color = &uiTuning_.cyberpunk.magenta;
    }
    else if (keyMatches(key, "cyber.aircraft"))
    {
        color = &uiTuning_.cyberpunk.aircraft;
    }
    else if (keyMatches(key, "cyber.aircraftGlow"))
    {
        color = &uiTuning_.cyberpunk.aircraftGlow;
    }
    else if (keyMatches(key, "cyber.text"))
    {
        color = &uiTuning_.cyberpunk.text;
    }
    else if (keyMatches(key, "cyber.altitude", "cyber.altitudeText"))
    {
        color = &uiTuning_.cyberpunk.altitudeText;
    }
    else if (keyMatches(key, "cyber.selected"))
    {
        color = &uiTuning_.cyberpunk.selected;
    }
    else if (keyMatches(key, "cyber.sweep"))
    {
        color = &uiTuning_.cyberpunk.sweep;
    }
    else if (keyMatches(key, "cyber.map"))
    {
        color = &uiTuning_.cyberpunk.map;
    }

    if (color == nullptr)
    {
        return false;
    }

    color->r = static_cast<uint8_t>(constrain(static_cast<int>(command.values[0]), 0, 255));
    color->g = static_cast<uint8_t>(constrain(static_cast<int>(command.values[1]), 0, 255));
    color->b = static_cast<uint8_t>(constrain(static_cast<int>(command.values[2]), 0, 255));
    return true;
}

bool RadarApp::applyUiTuningValue(const char *key, const UiTuningCommand &command)
{
    if (command.valueCount != 1)
    {
        return false;
    }

    const float value = command.values[0];
    if (keyMatches(key, "modern.globalBrightness", "globalBrightness"))
    {
        uiTuning_.modern.globalBrightness = value;
        return true;
    }
    if (keyMatches(key, "modern.backgroundBrightness", "backgroundBrightness"))
    {
        uiTuning_.modern.backgroundBrightness = value;
        return true;
    }
    if (keyMatches(key, "modern.gridBrightness", "gridBrightness"))
    {
        uiTuning_.modern.gridBrightness = value;
        return true;
    }
    if (keyMatches(key, "modern.textBrightness", "textBrightness"))
    {
        uiTuning_.modern.textBrightness = value;
        return true;
    }
    if (keyMatches(key, "modern.outerRadius", "outerRadius"))
    {
        uiTuning_.modern.outerRadius = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "modern.ringCount", "ringCount"))
    {
        uiTuning_.modern.ringCount = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "modern.lineWidth", "lineWidth"))
    {
        uiTuning_.modern.lineWidth = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "modern.centerDotRadius", "centerDotRadius"))
    {
        uiTuning_.modern.centerDotRadius = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "modern.aircraftScale", "aircraftScale"))
    {
        uiTuning_.modern.aircraftScale = value;
        return true;
    }
    if (keyMatches(key, "modern.vectorScale", "vectorScale"))
    {
        uiTuning_.modern.vectorScale = value;
        return true;
    }
    if (keyMatches(key, "modern.labelGap", "labelGap"))
    {
        uiTuning_.modern.labelGap = static_cast<int8_t>(value);
        return true;
    }
    if (keyMatches(key, "modern.showStatusText"))
    {
        uiTuning_.modern.showStatusText = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "modern.showLeaderLines"))
    {
        uiTuning_.modern.showLeaderLines = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "modern.maxLabels"))
    {
        uiTuning_.modern.maxLabels = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.globalBrightness"))
    {
        uiTuning_.cyberpunk.globalBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.ringBrightness"))
    {
        uiTuning_.cyberpunk.ringBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.textBrightness"))
    {
        uiTuning_.cyberpunk.textBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.aircraftBrightness"))
    {
        uiTuning_.cyberpunk.aircraftBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.sweepBrightness"))
    {
        uiTuning_.cyberpunk.sweepBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.outerRadius"))
    {
        uiTuning_.cyberpunk.outerRadius = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.innerRadarRadius"))
    {
        uiTuning_.cyberpunk.innerRadarRadius = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.ringCount"))
    {
        uiTuning_.cyberpunk.ringCount = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.lineWidth"))
    {
        uiTuning_.cyberpunk.lineWidth = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.majorTickLength"))
    {
        uiTuning_.cyberpunk.majorTickLength = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.minorTickLength"))
    {
        uiTuning_.cyberpunk.minorTickLength = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.centerDotRadius"))
    {
        uiTuning_.cyberpunk.centerDotRadius = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.aircraftScale"))
    {
        uiTuning_.cyberpunk.aircraftScale = value;
        return true;
    }
    if (keyMatches(key, "cyber.vectorScale"))
    {
        uiTuning_.cyberpunk.vectorScale = value;
        return true;
    }
    if (keyMatches(key, "cyber.sweepWidth"))
    {
        uiTuning_.cyberpunk.sweepWidth = value;
        return true;
    }
    if (keyMatches(key, "cyber.sweepTrailStrength"))
    {
        uiTuning_.cyberpunk.sweepTrailStrength = value;
        return true;
    }
    if (keyMatches(key, "cyber.labelGap"))
    {
        uiTuning_.cyberpunk.labelGap = static_cast<int8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.mapBrightness"))
    {
        uiTuning_.cyberpunk.mapBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.mapDensity"))
    {
        uiTuning_.cyberpunk.mapDensity = value;
        return true;
    }
    if (keyMatches(key, "cyber.bearingLabelRadiusOffset"))
    {
        uiTuning_.cyberpunk.bearingLabelRadiusOffset = static_cast<int16_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.cardinalRadiusOffset"))
    {
        uiTuning_.cyberpunk.cardinalRadiusOffset = static_cast<int16_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.outerGlowBrightness"))
    {
        uiTuning_.cyberpunk.outerGlowBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.outerGlowWidth"))
    {
        uiTuning_.cyberpunk.outerGlowWidth = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.mapEnabled"))
    {
        uiTuning_.cyberpunk.mapEnabled = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "cyber.radialGridEnabled"))
    {
        uiTuning_.cyberpunk.radialGridEnabled = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "cyber.radialGridStepDeg"))
    {
        uiTuning_.cyberpunk.radialGridStepDeg = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.radialGridBrightness"))
    {
        uiTuning_.cyberpunk.radialGridBrightness = value;
        return true;
    }
    if (keyMatches(key, "cyber.bearingLabelsEnabled"))
    {
        uiTuning_.cyberpunk.bearingLabelsEnabled = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "cyber.bearingLabelStepDeg"))
    {
        uiTuning_.cyberpunk.bearingLabelStepDeg = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.rangeLabelsEnabled"))
    {
        uiTuning_.cyberpunk.rangeLabelsEnabled = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "cyber.outerTickStepDeg"))
    {
        uiTuning_.cyberpunk.outerTickStepDeg = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.mediumTickStepDeg"))
    {
        uiTuning_.cyberpunk.mediumTickStepDeg = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.majorTickStepDeg"))
    {
        uiTuning_.cyberpunk.majorTickStepDeg = static_cast<uint8_t>(value);
        return true;
    }
    if (keyMatches(key, "cyber.showStatusText"))
    {
        uiTuning_.cyberpunk.showStatusText = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "cyber.showLeaderLines"))
    {
        uiTuning_.cyberpunk.showLeaderLines = value >= 0.5f;
        return true;
    }
    if (keyMatches(key, "cyber.maxLabels"))
    {
        uiTuning_.cyberpunk.maxLabels = static_cast<uint8_t>(value);
        return true;
    }

    return false;
}

void RadarApp::switchUiTheme()
{
    settings_.display.uiTheme = nextUiTheme(settings_.display.uiTheme);
    DebugLog::printf("UI theme switched: %s\r\n", uiThemeName(settings_.display.uiTheme));
    settingsStore_.save(settings_);

    if (config_.appMode == AppMode::RealRadar)
    {
        renderRealRadarFrame();
    }
}

void RadarApp::switchRange()
{
    sanitizeUserSettings(settings_);

    uint8_t selectedPreset = 0;
    float selectedDelta = fabsf(settings_.location.maxRangeKm - settings_.location.rangePresetsKm[0]);
    for (uint8_t i = 1; i < 3; ++i)
    {
        const float delta = fabsf(settings_.location.maxRangeKm - settings_.location.rangePresetsKm[i]);
        if (delta < selectedDelta)
        {
            selectedPreset = i;
            selectedDelta = delta;
        }
    }

    const uint8_t nextPreset = (selectedPreset + 1) % 3;
    settings_.location.maxRangeKm = settings_.location.rangePresetsKm[nextPreset];
    updateQueryBoxFromCenterRange(settings_);
    DebugLog::printf("Range switched: %.0fkm\r\n", settings_.location.maxRangeKm);
    settingsStore_.save(settings_);
}

void RadarApp::toggleGroundTraffic()
{
    settings_.filter.showGroundTraffic = !settings_.filter.showGroundTraffic;
    DebugLog::printf("showGroundTraffic=%u\r\n", settings_.filter.showGroundTraffic ? 1 : 0);
    settingsStore_.save(settings_);
}

void RadarApp::cycleScheduleIdleDisplayMode()
{
    settings_.schedule.idleDisplayMode = nextScheduleIdleDisplayMode(settings_.schedule.idleDisplayMode);
    DebugLog::printf("Outside-schedule display: %s\r\n",
                     scheduleIdleDisplayModeName(settings_.schedule.idleDisplayMode));
    settingsStore_.save(settings_);

    if (config_.appMode == AppMode::RealRadar && deviceState_ == DeviceState::PausedBySchedule)
    {
        renderPausedIdleFrame(true);
    }
}

void RadarApp::toggleSetupDisplayMode()
{
    if (deviceState_ == DeviceState::SetupPortal)
    {
        setupDisplayMode_ = setupDisplayMode_ == SetupDisplayMode::QrCode ?
                            SetupDisplayMode::Details :
                            SetupDisplayMode::QrCode;
        settingsDisplayMode_ = setupDisplayMode_ == SetupDisplayMode::QrCode ?
                               SettingsDisplayMode::ApQr :
                               SettingsDisplayMode::ApDetails;
        DebugLog::printf("AP setup display mode: %s\r\n",
                         setupDisplayMode_ == SetupDisplayMode::QrCode ? "QR" : "Details");
        renderSetupPortalFrame(nullptr);
        return;
    }

    if (staSettingsOverlayVisible_)
    {
        settingsDisplayMode_ = settingsDisplayMode_ == SettingsDisplayMode::StaQr ?
                               SettingsDisplayMode::StaDetails :
                               SettingsDisplayMode::StaQr;
        DebugLog::printf("STA settings display mode: %s\r\n",
                         settingsDisplayMode_ == SettingsDisplayMode::StaQr ? "QR" : "Details");
        renderSettingsDisplay(nullptr);
        return;
    }

    DebugLog::println("q ignored: no settings display is active.");
}

void RadarApp::showStaSettingsOverlay()
{
    if (!wifi_.isConnected())
    {
        DebugLog::println("w ignored: WiFi is not connected.");
        return;
    }

    beginStaSettingsServer();
    staSettingsOverlayVisible_ = true;
    settingsDisplayMode_ = SettingsDisplayMode::StaQr;
    DebugLog::printf("STA settings URL: http://%s/\r\n", WiFi.localIP().toString().c_str());
    renderSettingsDisplay(nullptr);
}

void RadarApp::hideStaSettingsOverlay()
{
    if (!staSettingsOverlayVisible_)
    {
        return;
    }

    staSettingsOverlayVisible_ = false;
    DebugLog::println("STA settings overlay hidden.");
    if (config_.appMode == AppMode::RealRadar)
    {
        if (deviceState_ == DeviceState::PausedBySchedule)
        {
            renderPausedIdleFrame(true);
        }
        else
        {
            renderRealRadarFrame();
        }
    }
    else if (config_.appMode == AppMode::RadarDemo)
    {
        renderFrame();
    }
}

bool RadarApp::hasActiveOverlay() const
{
    return deviceState_ == DeviceState::SetupPortal || staSettingsOverlayVisible_;
}

void RadarApp::renderSettingsDisplay(const char *statusText)
{
    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        return;
    }

    screenSleeping_ = false;
    renderer_.renderSettingsFrame(configPortal_.apSsid(),
                                  configPortal_.apPassword(),
                                  "192.168.4.1",
                                  WiFi.localIP().toString().c_str(),
                                  statusText,
                                  settingsDisplayMode_);
}

void RadarApp::beginStaSettingsServer()
{
    if (!wifi_.isConnected())
    {
        return;
    }

    if (configPortal_.isRunning() && configPortal_.mode() == ConfigPortalMode::StaSettings)
    {
        return;
    }

    if (configPortal_.isRunning())
    {
        configPortal_.stop();
    }
    configPortal_.beginStaSettings(&settings_, &settingsStore_);
}

void RadarApp::resetSettingsToDefault()
{
    settingsStore_.resetToDefault(settings_);
    DebugLog::println("User settings reset to default.");
    printUserSettings(settings_);
    settingsStore_.save(settings_);
}

void RadarApp::printTimeStatus()
{
    timeManager_.update();
    char localTime[8];
    char nextStart[8];
    timeManager_.formatLocalTime(localTime, sizeof(localTime));
    formatMinutesOfDay(computeNextScheduleStartMinutes(settings_.schedule,
                                                       timeManager_.getLocalMinutesOfDay()),
                       nextStart,
                       sizeof(nextStart));

    const int16_t localMinutes = timeManager_.getLocalMinutesOfDay();
    DebugLog::println("Time status:");
    DebugLog::printf("  synced=%u utc=%lu\r\n",
                     timeManager_.isTimeSynced() ? 1 : 0,
                     static_cast<unsigned long>(timeManager_.getUnixTime()));
    DebugLog::printf("  local=%s minutes=%d timezoneOffset=%d\r\n",
                     localTime,
                     localMinutes,
                     settings_.schedule.timezoneOffsetMinutes);
    DebugLog::printf("  schedule enabled=%u active=%u start=%d end=%d next=%s\r\n",
                     settings_.schedule.enabled ? 1 : 0,
                     isWithinSchedule(settings_.schedule, localMinutes) ? 1 : 0,
                     settings_.schedule.startMinutesOfDay,
                     settings_.schedule.endMinutesOfDay,
                     nextStart);
    DebugLog::printf("  outside-schedule display=%s\r\n",
                     scheduleIdleDisplayModeName(settings_.schedule.idleDisplayMode));
}

void RadarApp::printDeviceStateStatus()
{
    const SystemStatus status = getSystemStatus();
    DebugLog::println("Device mode/status:");
    DebugLog::printf("  appMode=%d state=%s theme=%s wifi=%s staOverlay=%u setup=%u uiLab=%u\r\n",
                     static_cast<int>(status.appMode),
                     deviceStateName(status.deviceState),
                     uiThemeName(status.uiTheme),
                     wifi_.statusText(),
                     status.staSettingsServerRunning ? 1 : 0,
                     status.setupPortalRunning ? 1 : 0,
                     status.uiLabRunning ? 1 : 0);
    DebugLog::printf("  updater running=%u updating=%u interval=%lums http=%d\r\n",
                     status.apiUpdaterRunning ? 1 : 0,
                     status.apiUpdaterUpdating ? 1 : 0,
                     static_cast<unsigned long>(currentRealApiIntervalMs_),
                     status.lastHttpCode);
    DebugLog::printf("  uptime=%lus heap=%lu minHeap=%lu maxAlloc=%lu aircraft=%u api=%lu/%lu\r\n",
                     static_cast<unsigned long>(status.uptimeMs / 1000UL),
                     static_cast<unsigned long>(status.freeHeap),
                     static_cast<unsigned long>(status.minFreeHeap),
                     static_cast<unsigned long>(status.maxAllocHeap),
                     status.aircraftCount,
                     static_cast<unsigned long>(status.apiRequestCount),
                     static_cast<unsigned long>(status.apiErrorCount));
}

SystemStatus RadarApp::getSystemStatus() const
{
    SystemStatus status;
    status.deviceState = deviceState_;
    status.appMode = config_.appMode;
    status.uiTheme = debugMode_ == DebugMode::UiLab ? uiLabTheme_ : settings_.display.uiTheme;
    status.wifiConnected = wifi_.isConnected();
    status.staSettingsServerRunning = configPortal_.isRunning() &&
                                      configPortal_.mode() == ConfigPortalMode::StaSettings;
    status.setupPortalRunning = configPortal_.isRunning() &&
                                configPortal_.mode() == ConfigPortalMode::ApSetup;
    status.apiUpdaterRunning = realApiUpdater_.isRunning();
    status.apiUpdaterUpdating = realApiUpdater_.isUpdating();
    status.ntpSynced = timeManager_.isTimeSynced();
    status.withinSchedule = !settings_.schedule.enabled ||
                            (status.ntpSynced &&
                             isWithinSchedule(settings_.schedule, timeManager_.getLocalMinutesOfDay()));
    status.uiLabRunning = debugMode_ == DebugMode::UiLab;
    status.uptimeMs = millis();
    status.freeHeap = ESP.getFreeHeap();
    status.minFreeHeap = ESP.getMinFreeHeap();
    status.maxAllocHeap = ESP.getMaxAllocHeap();
    status.lastApiSuccessMs = realApiUpdater_.lastSuccessMs();
    status.lastApiErrorMs = lastApiErrorMs_;
    status.lastHttpCode = realApiUpdater_.lastHttpStatus();
    status.apiRequestCount = apiRequestCount_;
    status.apiErrorCount = apiErrorCount_;
    if (debugMode_ == DebugMode::UiLab)
    {
        status.aircraftCount = uiLabAircraftCount_;
    }
    else if (config_.appMode == AppMode::RealRadar)
    {
        status.aircraftCount = realAircraftCount_;
    }
    else
    {
        status.aircraftCount = dataProvider_.count();
    }
    status.lastFrameMs = lastFrameMs_;
    status.fpsX10 = config_.frameIntervalMs > 0 ?
                    static_cast<uint16_t>(10000UL / config_.frameIntervalMs) :
                    0;
    return status;
}

void RadarApp::updateLongRunStatusLog(uint32_t now)
{
    if (!settings_.system.serialDebug)
    {
        return;
    }

    if (lastSystemStatusLogMs_ != 0 && now - lastSystemStatusLogMs_ < kSystemStatusLogIntervalMs)
    {
        return;
    }
    lastSystemStatusLogMs_ = now;

    const SystemStatus status = getSystemStatus();
    DebugLog::printf("Status: up=%lus heap=%lu min=%lu wifi=%u state=%s provider=%s api=%lu/%lu aircraft=%u theme=%s\r\n",
                     static_cast<unsigned long>(status.uptimeMs / 1000UL),
                     static_cast<unsigned long>(status.freeHeap),
                     static_cast<unsigned long>(status.minFreeHeap),
                     status.wifiConnected ? 1 : 0,
                     deviceStateName(status.deviceState),
                     apiProviderName(settings_.api.provider),
                     static_cast<unsigned long>(status.apiRequestCount),
                     static_cast<unsigned long>(status.apiErrorCount),
                     status.aircraftCount,
                     uiThemeName(status.uiTheme));
}

void RadarApp::updateTemperatureLog(uint32_t now)
{
    if (lastTemperatureLogMs_ != 0 && now - lastTemperatureLogMs_ < kTemperatureLogIntervalMs)
    {
        return;
    }

    lastTemperatureLogMs_ = now;
    const float temperatureC = readInternalTemperatureC();
    if (isnan(temperatureC))
    {
        DebugLog::println("ESP32-S3 internal temperature default: NAN");
        return;
    }
    DebugLog::printf("ESP32-S3 internal temperature default: %.1f C\r\n", temperatureC);
}

void RadarApp::printApiAuthStatus()
{
    DebugLog::println("API/auth status:");
    DebugLog::printf("  provider=%s\r\n", apiProviderName(settings_.api.provider));
    DebugLog::printf("  accountMode=%s\r\n", apiAccountModeName(settings_.api.accountMode));
    if (settings_.api.provider == ApiProvider::AdsbFi)
    {
        DebugLog::println("  adsb.fi Open Data uses no OAuth token.");
        DebugLog::printf("  lastHttpStatus=%d lastApiError=%s\r\n",
                         realApiUpdater_.lastHttpStatus(),
                         realApiUpdater_.lastError());
        return;
    }
    DebugLog::printf("  hasClientId=%u hasClientSecret=%u\r\n",
                     settings_.api.openSkyClientId[0] != '\0' ? 1 : 0,
                     settings_.api.openSkyClientSecret[0] != '\0' ? 1 : 0);
    DebugLog::printf("  tokenValid=%u tokenExpiresIn=%lus\r\n",
                     realApiUpdater_.tokenValid() ? 1 : 0,
                     static_cast<unsigned long>(realApiUpdater_.tokenExpiresInMs() / 1000UL));
    DebugLog::printf("  lastAuthError=%s\r\n", realApiUpdater_.lastAuthError());
    DebugLog::printf("  lastHttpStatus=%d lastApiError=%s\r\n",
                     realApiUpdater_.lastHttpStatus(),
                     realApiUpdater_.lastError());
}

void RadarApp::clearAuthToken()
{
    realApiUpdater_.invalidateAuthToken();
}

void RadarApp::enterSetupPortal(const char *reason)
{
    if (configPortal_.isRunning() && configPortal_.mode() == ConfigPortalMode::ApSetup)
    {
        renderSetupPortalFrame(reason);
        return;
    }

    DebugLog::printf("Entering setup portal: %s\r\n", reason != nullptr ? reason : "requested");
    if (configPortal_.isRunning())
    {
        configPortal_.stop();
    }
    realApiUpdater_.stop();
    wifi_.stop();
    wifiManagerStarted_ = false;
    setupDisplayMode_ = SetupDisplayMode::QrCode;
    settingsDisplayMode_ = SettingsDisplayMode::ApQr;
    staSettingsOverlayVisible_ = false;
    setDeviceState(DeviceState::SetupPortal);

    if (!renderer_.isReady())
    {
        renderer_.begin();
    }

    configPortal_.beginApSetup(&settings_, &settingsStore_);
    renderSetupPortalFrame(reason);
}

void RadarApp::exitSetupPortal()
{
    DebugLog::println("Exiting setup portal.");
    const bool returnToApp = setupPortalFromLocalMenu_;
    setupPortalFromLocalMenu_ = false;
    configPortal_.stop();
    wifiLostSinceMs_ = 0;

    if (returnToApp)
    {
        DebugLog::println("Returning from local AP setup to app state.");
        if (config_.appMode == AppMode::RealRadar)
        {
            if (connectToConfiguredWiFi())
            {
                beginStaSettingsServer();
                beginRealRadar();
            }
            else
            {
                setDeviceState(DeviceState::WiFiLost, "AP setup exited");
                if (!renderer_.isReady())
                {
                    renderer_.begin();
                }
                renderRealRadarSystemStatus();
            }
            return;
        }

        if (config_.appMode == AppMode::ApiTest)
        {
            if (connectToConfiguredWiFi())
            {
                beginStaSettingsServer();
                beginApiTest();
            }
            else
            {
                if (!apiTestView_.isReady())
                {
                    apiTestView_.begin();
                }
                renderApiTestScreen();
            }
            return;
        }

        beginRadarDemo();
        return;
    }

    beginConfiguredMode();
}

void RadarApp::updateSetupPortal(uint32_t now)
{
    (void)now;
    configPortal_.update();

    if (configPortal_.shouldRestart())
    {
        renderSetupPortalFrame("Restarting...");
        delay(300);
        ESP.restart();
    }
}

void RadarApp::renderSetupPortalFrame(const char *statusText)
{
    settingsDisplayMode_ = setupDisplayMode_ == SetupDisplayMode::QrCode ?
                           SettingsDisplayMode::ApQr :
                           SettingsDisplayMode::ApDetails;
    renderSettingsDisplay(statusText);
}

void RadarApp::setDeviceState(DeviceState state, const char *reason)
{
    if (deviceState_ == state)
    {
        return;
    }

    const DeviceState previous = deviceState_;
    deviceState_ = state;
    if (state == DeviceState::PausedBySchedule && previous != DeviceState::PausedBySchedule)
    {
        lastIdleDisplayRenderMs_ = 0;
    }
    if (reason != nullptr && reason[0] != '\0')
    {
        DebugLog::printf("DeviceState: %s -> %s (%s)\r\n",
                         deviceStateName(previous),
                         deviceStateName(deviceState_),
                         reason);
    }
    else
    {
        DebugLog::printf("DeviceState: %s -> %s\r\n",
                         deviceStateName(previous),
                         deviceStateName(deviceState_));
    }
}

bool RadarApp::connectToConfiguredWiFi()
{
    if (!settings_.wifi.configured || settings_.wifi.ssid[0] == '\0')
    {
        DebugLog::println("WiFi is not configured in UserSettings.");
        return false;
    }

    setDeviceState(DeviceState::ConnectWiFi);
    startWifiManagerFromSettings();

    const uint32_t startMs = millis();
    while (!wifi_.isConnected() && millis() - startMs < 12000)
    {
        wifi_.update(millis(), config_.wifiReconnectIntervalMs);
        delay(100);
    }

    if (!wifi_.isConnected())
    {
        setDeviceState(DeviceState::WiFiLost);
        DebugLog::printf("Configured WiFi connection failed. status=%s ssid_len=%u password_set=%u\r\n",
                         wifi_.statusText(),
                         static_cast<unsigned int>(strlen(settings_.wifi.ssid)),
                         settings_.wifi.password[0] != '\0' ? 1 : 0);
        return false;
    }

    DebugLog::printf("Configured WiFi connected. IP=%s RSSI=%d\r\n",
                     WiFi.localIP().toString().c_str(),
                     WiFi.RSSI());
    return true;
}

void RadarApp::startWifiManagerFromSettings()
{
    if (wifiManagerStarted_)
    {
        return;
    }

    wifi_.begin(settings_.wifi.ssid, settings_.wifi.password);
    wifiManagerStarted_ = true;
}

bool RadarApp::updateRealRadarRunGate(uint32_t now, bool forceCheck)
{
    timeManager_.update();

    if (!forceCheck && lastScheduleCheckMs_ != 0 && now - lastScheduleCheckMs_ < kScheduleCheckIntervalMs)
    {
        return deviceState_ == DeviceState::Running;
    }
    lastScheduleCheckMs_ = now;

    if (!wifi_.isConnected())
    {
        stopRealRadarUpdater();
        setDeviceState(DeviceState::WiFiLost, "WiFi lost");
        return false;
    }

    if (settings_.schedule.enabled && !timeManager_.isTimeSynced())
    {
        if (!isIdleUiPreviewActive(now))
        {
            stopRealRadarUpdater();
        }
        setDeviceState(DeviceState::PausedBySchedule, "time not synced");
        if (lastTimeSyncLogMs_ == 0 || now - lastTimeSyncLogMs_ >= 10000)
        {
            lastTimeSyncLogMs_ = now;
            DebugLog::println("Waiting for NTP time sync before scheduled API requests.");
        }
        return false;
    }

    const int16_t localMinutes = timeManager_.getLocalMinutesOfDay();
    const bool active = isWithinSchedule(settings_.schedule, localMinutes);
    if (!active)
    {
        if (!isIdleUiPreviewActive(now))
        {
            stopRealRadarUpdater();
        }
        setDeviceState(DeviceState::PausedBySchedule, "schedule inactive");
        return false;
    }

    if (deviceState_ != DeviceState::Running)
    {
        setDeviceState(DeviceState::Running, "schedule active");
        DebugLog::println("Schedule active, resume API.");
    }

    ensureRealRadarUpdaterRunning();
    return true;
}

void RadarApp::ensureRealRadarUpdaterRunning()
{
    const uint32_t intervalMs = activeRequestIntervalMs(settings_);
    bool intervalChanged = false;
    if (intervalMs != currentRealApiIntervalMs_)
    {
        currentRealApiIntervalMs_ = intervalMs;
        intervalChanged = true;
        DebugLog::printf("RealRadar API interval updated: %lu ms (%s)\r\n",
                         static_cast<unsigned long>(currentRealApiIntervalMs_),
                         refreshPolicyName(settings_.api.refreshPolicy));
    }

    if (realApiUpdater_.isRunning())
    {
        if (intervalChanged)
        {
            realApiUpdater_.begin(config_, settings_, currentRealApiIntervalMs_);
        }
        return;
    }

    if (realApiUpdater_.begin(config_, settings_, currentRealApiIntervalMs_))
    {
        DebugLog::printf("RealRadar API updater running, interval=%lu ms\r\n",
                         static_cast<unsigned long>(currentRealApiIntervalMs_));
    }
}

void RadarApp::stopRealRadarUpdater()
{
    if (realApiUpdater_.isRunning() || realApiUpdater_.isUpdating())
    {
        realApiUpdater_.stop();
    }
}

void RadarApp::renderRealRadarSystemStatus()
{
    if (!renderer_.isReady())
    {
        return;
    }

    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        return;
    }

    screenSleeping_ = false;

    if (!wifi_.isConnected())
    {
        renderer_.renderSystemStatusFrame("WIFI LOST", "Reconnecting", "", settings_.display.uiTheme);
        return;
    }

    if (settings_.schedule.enabled && !timeManager_.isTimeSynced())
    {
        renderer_.renderSystemStatusFrame("TIME SYNC", "Waiting for NTP", "", settings_.display.uiTheme);
        return;
    }

    if (deviceState_ == DeviceState::PausedBySchedule)
    {
        renderPausedIdleFrame(true);
        return;
    }

    renderer_.renderSystemStatusFrame("STATUS", realRadarStatus_, "", settings_.display.uiTheme);
}

void RadarApp::renderPausedIdleFrame(bool force)
{
    if (!renderer_.isReady())
    {
        return;
    }

    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        return;
    }

    if (hasActiveOverlay())
    {
        if (staSettingsOverlayVisible_)
        {
            renderSettingsDisplay(nullptr);
        }
        return;
    }

    timeManager_.update();

    if (settings_.schedule.enabled && !timeManager_.isTimeSynced())
    {
        screenSleeping_ = false;
        renderer_.renderSystemStatusFrame("TIME SYNC", "Waiting for NTP", "", settings_.display.uiTheme);
        lastIdleDisplayRenderMs_ = millis();
        return;
    }

    char nextStart[8];
    formatMinutesOfDay(computeNextScheduleStartMinutes(settings_.schedule,
                                                       timeManager_.getLocalMinutesOfDay()),
                       nextStart,
                       sizeof(nextStart));

    switch (settings_.schedule.idleDisplayMode)
    {
        case ScheduleIdleDisplayMode::Clock:
        {
            screenSleeping_ = false;
            char localTime[8];
            char nextRun[24];
            timeManager_.formatLocalTime(localTime, sizeof(localTime));
            snprintf(nextRun, sizeof(nextRun), "Next run: %s", nextStart);
            renderer_.renderClockFrame(localTime, "", nextRun, "long: menu", settings_.display.uiTheme);
            lastIdleDisplayRenderMs_ = millis();
            break;
        }

        case ScheduleIdleDisplayMode::DisplayOff:
            if (force || lastIdleDisplayRenderMs_ == 0 || millis() - lastIdleDisplayRenderMs_ >= 60000)
            {
                renderer_.renderBlankFrame();
                lastIdleDisplayRenderMs_ = millis();
            }
            break;

        case ScheduleIdleDisplayMode::PausedStatus:
        default:
        {
            screenSleeping_ = false;
            char line3[24];
            snprintf(line3, sizeof(line3), "Next: %s", nextStart);
            renderer_.renderSystemStatusFrame("PAUSED", "Outside schedule", line3, settings_.display.uiTheme);
            lastIdleDisplayRenderMs_ = millis();
            break;
        }
    }
}

void RadarApp::formatMinutesOfDay(int16_t minutes, char *buffer, size_t bufferSize) const
{
    if (buffer == nullptr || bufferSize == 0)
    {
        return;
    }

    if (minutes < 0)
    {
        snprintf(buffer, bufferSize, "--:--");
        return;
    }

    minutes %= 24 * 60;
    if (minutes < 0)
    {
        minutes += 24 * 60;
    }
    snprintf(buffer, bufferSize, "%02d:%02d", minutes / 60, minutes % 60);
}

void RadarApp::updateRadarDemo(uint32_t now)
{
    updateAircraftData(now);
    updateSelectedAircraftForList(dataProvider_.aircraft(), dataProvider_.count(), now);

    if (now - lastFrameMs_ < config_.frameIntervalMs)
    {
        return;
    }
    lastFrameMs_ = now;

    if (!staSettingsOverlayVisible_)
    {
        renderFrame();
        renderer_.advanceSweep(config_.sweepStepDeg);
    }
}

void RadarApp::updateApiTest(uint32_t now)
{
    wifi_.update(now, config_.wifiReconnectIntervalMs);
    if (!wifi_.isConnected())
    {
        if (wifiLostSinceMs_ == 0)
        {
            wifiLostSinceMs_ = now;
        }
        else if (now - wifiLostSinceMs_ > 30000)
        {
            enterSetupPortal("WiFi lost");
            return;
        }
    }
    else
    {
        wifiLostSinceMs_ = 0;
    }

    if (wifi_.isConnected() &&
        (lastApiRequestMs_ == 0 || now - lastApiRequestMs_ >= activeRequestIntervalMs(settings_)))
    {
        lastApiRequestMs_ = now;
        openSky_.requestStates(config_);
        printApiTestSerialStatus();
    }

    if (now - lastApiSerialMs_ >= config_.apiSerialStatusIntervalMs)
    {
        lastApiSerialMs_ = now;
        printApiTestSerialStatus();
    }

    if (now - lastApiScreenMs_ < config_.apiScreenRefreshMs)
    {
        return;
    }

    lastApiScreenMs_ = now;
    renderApiTestScreen();
}

void RadarApp::updateRealRadar(uint32_t now)
{
    wifi_.update(now, config_.wifiReconnectIntervalMs);
    if (!wifi_.isConnected())
    {
        if (wifiLostSinceMs_ == 0)
        {
            wifiLostSinceMs_ = now;
            stopRealRadarUpdater();
            setDeviceState(DeviceState::WiFiLost, "WiFi lost");
            renderRealRadarSystemStatus();
        }
        else if (now - wifiLostSinceMs_ > 30000)
        {
            enterSetupPortal("WiFi lost");
        }
        return;
    }

    if (wifiLostSinceMs_ != 0 || deviceState_ == DeviceState::WiFiLost)
    {
        wifiLostSinceMs_ = 0;
        DebugLog::println("WiFi reconnected, restarting time sync and schedule check.");
        beginStaSettingsServer();
        timeManager_.begin(settings_);
        lastScheduleCheckMs_ = 0;
    }

    if (!updateRealRadarRunGate(now, false))
    {
        const bool previewActive = isIdleUiPreviewActive(now);
        if (deviceState_ == DeviceState::PausedBySchedule)
        {
            if (previewActive || idleUiPreviewUntilMs_ != 0)
            {
                updateIdleRealRadarPreview(now, previewActive);
            }
        }

        const uint32_t pausedFrameIntervalMs = previewActive && idleUiPreviewRefreshMs_ > 0 ?
                                               idleUiPreviewRefreshMs_ :
                                               config_.frameIntervalMs;
        if (now - lastFrameMs_ >= pausedFrameIntervalMs)
        {
            lastFrameMs_ = now;
            if (deviceState_ == DeviceState::PausedBySchedule)
            {
                if (previewActive)
                {
                    renderIdleUiPreviewFrame();
                    renderer_.advanceSweep(config_.sweepStepDeg);
                }
                else
                {
                    renderPausedIdleFrame(false);
                }
            }
            else
            {
                renderRealRadarSystemStatus();
            }
        }
        return;
    }

    OpenSkySnapshot snapshot;
    if (realApiUpdater_.copySnapshot(snapshot))
    {
        handleRealRadarSnapshot(snapshot, now);
    }

    realTrackManager_.updatePrediction(settings_, now);
    RealRadarTrackStats frameStats;
    rebuildRealRadarAircraft(frameStats);
    if (now - lastPredictionSummaryMs_ >= 10000)
    {
        lastPredictionSummaryMs_ = now;
        realTrackManager_.printPredictionSummary(settings_, now);
    }
    updateSelectedAircraftForList(realAircraft_, realAircraftCount_, now);

    if (now - lastFrameMs_ < config_.frameIntervalMs)
    {
        return;
    }

    lastFrameMs_ = now;
    updateRealRadarStatus();
    if (!staSettingsOverlayVisible_)
    {
        renderRealRadarFrame();
        renderer_.advanceSweep(config_.sweepStepDeg);
    }
}

void RadarApp::updateAircraftData(uint32_t now)
{
    if (now - lastAircraftUpdateMs_ < config_.aircraftUpdateIntervalMs)
    {
        return;
    }
    lastAircraftUpdateMs_ = now;
    dataProvider_.update();
}

void RadarApp::updateSelectedAircraft(uint32_t now)
{
    updateSelectedAircraftForList(dataProvider_.aircraft(), dataProvider_.count(), now);
}

void RadarApp::updateSelectedAircraftForList(const Aircraft *aircraft, uint8_t aircraftCount, uint32_t now)
{
    if (aircraftCount == 0 || AircraftModel::countValid(aircraft, aircraftCount) == 0)
    {
        selectedAircraftIndex_ = 0;
        return;
    }

    if (selectedAircraftIndex_ >= aircraftCount || !aircraft[selectedAircraftIndex_].valid)
    {
        const int8_t firstValid = AircraftModel::firstValidIndex(aircraft, aircraftCount);
        selectedAircraftIndex_ = firstValid >= 0 ? static_cast<uint8_t>(firstValid) : 0;
        lastSelectionMs_ = now;
        return;
    }

    if (now - lastSelectionMs_ < config_.selectionIntervalMs)
    {
        return;
    }
    lastSelectionMs_ = now;

    for (uint8_t attempts = 0; attempts < aircraftCount; ++attempts)
    {
        selectedAircraftIndex_ = (selectedAircraftIndex_ + 1) % aircraftCount;
        if (aircraft[selectedAircraftIndex_].valid)
        {
            return;
        }
    }
}

void RadarApp::renderFrame()
{
    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        return;
    }

    screenSleeping_ = false;
    const AppConfig renderConfig = runtimeRenderConfig();
    renderer_.renderRadarFrame(dataProvider_.aircraft(),
                               dataProvider_.count(),
                               selectedAircraftIndex_,
                               renderConfig,
                               settings_.display.uiTheme);
}

void RadarApp::renderRealRadarFrame()
{
    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        return;
    }

    screenSleeping_ = false;
    const AppConfig renderConfig = runtimeRenderConfig();
    renderer_.renderRadarFrame(realAircraft_,
                               realAircraftCount_,
                               selectedAircraftIndex_,
                               renderConfig,
                               settings_.display.uiTheme,
                               realRadarStatus_);
}

void RadarApp::renderApiTestScreen()
{
    if (localMenuPage_ != LocalMenuPage::Closed)
    {
        return;
    }

    screenSleeping_ = false;
    apiTestView_.render(wifi_, openSky_);
}

void RadarApp::updateRealRadarStatus()
{
    if (!wifi_.isConnected())
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "WIFI LOST");
        return;
    }

    if (settings_.api.provider == ApiProvider::OpenSky &&
        settings_.api.accountMode == ApiAccountMode::OpenSkyClient &&
        (settings_.api.openSkyClientId[0] == '\0' || settings_.api.openSkyClientSecret[0] == '\0'))
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "AUTH CONFIG");
        return;
    }

    if (realApiUpdater_.isUpdating())
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "UPDATING");
        return;
    }

    const char *apiError = realApiUpdater_.lastError();
    const char *authError = realApiUpdater_.lastAuthError();
    if (settings_.api.provider == ApiProvider::OpenSky &&
        ((apiError != nullptr && strncmp(apiError, "AUTH CONFIG", 11) == 0) ||
         (authError != nullptr && strncmp(authError, "AUTH CONFIG", 11) == 0)))
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "AUTH CONFIG");
        return;
    }

    if (settings_.api.provider == ApiProvider::OpenSky &&
        ((apiError != nullptr && strncmp(apiError, "AUTH", 4) == 0) ||
         (authError != nullptr && strncmp(authError, "AUTH", 4) == 0 && strcmp(authError, "OK") != 0)))
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "AUTH ERR");
        return;
    }

    if (realApiUpdater_.lastHttpStatus() == 429)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "HTTP 429");
        return;
    }

    if (realApiUpdater_.lastHttpStatus() == 401)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "HTTP 401");
        return;
    }

    if (realApiUpdater_.lastHttpStatus() != 0 && realApiUpdater_.lastHttpStatus() != 200)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "API ERROR");
        return;
    }

    if (realApiUpdater_.lastHttpStatus() == 0)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "NO DATA");
        return;
    }

    const uint32_t ageSec = realApiUpdater_.lastSuccessMs() > 0 ?
                            (millis() - realApiUpdater_.lastSuccessMs()) / 1000 :
                            0;
    if (ageSec > 300)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "DATA STALE");
        return;
    }

    if (realAircraftCount_ == 0)
    {
        if (lastRealRadarFilteredCount_ > 0)
        {
            snprintf(realRadarStatus_,
                     sizeof(realRadarStatus_),
                     "NO AIRBORNE\nFILTERED %u",
                     lastRealRadarFilteredCount_);
        }
        else
        {
            snprintf(realRadarStatus_, sizeof(realRadarStatus_), "NO AIRBORNE");
        }
        return;
    }

    const uint32_t intervalSec = currentRealApiIntervalMs_ / 1000UL;
    snprintf(realRadarStatus_,
             sizeof(realRadarStatus_),
             "LIVE N=%u %lu/%lus",
             realAircraftCount_,
             static_cast<unsigned long>(ageSec),
             static_cast<unsigned long>(intervalSec));
}

void RadarApp::printApiTestSerialStatus()
{
    DebugLog::println("API test status:");
    DebugLog::printf("  Query box: lat %.4f..%.4f lon %.4f..%.4f\r\n",
                     config_.openSkyLamin,
                     config_.openSkyLamax,
                     config_.openSkyLomin,
                     config_.openSkyLomax);
    DebugLog::printf("  WiFi=%s RSSI=%d\r\n", wifi_.statusText(), wifi_.rssi());
    DebugLog::printf("  HTTP=%d bytes=%lu planes=%u status=%s\r\n",
                     openSky_.httpStatusCode(),
                     static_cast<unsigned long>(openSky_.payloadLength()),
                     openSky_.aircraftCount(),
                     openSky_.lastError());

    if (openSky_.lastSuccessMs() > 0)
    {
        DebugLog::printf("  Last success: %lus ago\r\n",
                         static_cast<unsigned long>((millis() - openSky_.lastSuccessMs()) / 1000));
    }
    else
    {
        DebugLog::println("  Last success: never");
    }

    const ApiAircraft *aircraft = openSky_.aircraft();
    const uint8_t count = min<uint8_t>(openSky_.aircraftCount(), 3);
    if (count == 0)
    {
        DebugLog::println("  No aircraft parsed yet.");
        return;
    }

    for (uint8_t i = 0; i < count; ++i)
    {
        DebugLog::printf("  #%u %s lat=%.5f lon=%.5f alt=%.0fm speed=%.1fm/s hdg=%.0f\r\n",
                         i + 1,
                         aircraft[i].callsign,
                         aircraft[i].lat,
                         aircraft[i].lon,
                         aircraft[i].altitudeM,
                         aircraft[i].speedMs,
                         aircraft[i].headingDeg);
    }
}

void RadarApp::handleRealRadarSnapshot(const OpenSkySnapshot &snapshot, uint32_t now)
{
    ++apiRequestCount_;
    DebugLog::println("received new API snapshot");
    DebugLog::printf("  HTTP=%d duration=%lu ms payload=%lu aircraft=%u status=%s\r\n",
                     snapshot.httpStatusCode,
                     static_cast<unsigned long>(snapshot.durationMs),
                     static_cast<unsigned long>(snapshot.payloadLength),
                     snapshot.aircraftCount,
                     snapshot.lastError);

    if (snapshot.requestOk || snapshot.httpStatusCode == 200)
    {
        RealRadarTrackStats stats;
        realTrackManager_.mergeSnapshot(snapshot, settings_, now, stats);
        realTrackManager_.updatePrediction(settings_, now);
        rebuildRealRadarAircraft(stats);
        printRealRadarTrackSummary(snapshot, stats);
    }
    else
    {
        ++apiErrorCount_;
        lastApiErrorMs_ = now;
        DebugLog::println("  API snapshot failed, keeping current tracks.");
    }

    updateRealRadarStatus();
}

void RadarApp::rebuildRealRadarAircraft(RealRadarTrackStats &stats)
{
    realAircraftCount_ = realTrackManager_.buildAircraft(settings_,
                                                         realAircraft_,
                                                         AircraftModel::kAircraftCount,
                                                         stats);
    lastRealRadarFilteredCount_ = stats.filteredGround +
                                  stats.filteredAltitude +
                                  stats.filteredSpeed +
                                  stats.filteredRange;
}

void RadarApp::printRealRadarTrackSummary(const OpenSkySnapshot &snapshot, const RealRadarTrackStats &stats)
{
    DebugLog::println("RealRadar track summary:");
    DebugLog::printf("  raw=%u valid_pos=%u\r\n", snapshot.rawStateCount, snapshot.validPositionCount);
    DebugLog::printf("  matched tracks=%u\r\n", stats.matchedTracks);
    DebugLog::printf("  new tracks=%u\r\n", stats.newTracks);
    DebugLog::printf("  corrections started=%u\r\n", stats.correctionStartedCount);
    DebugLog::printf("  jump resets=%u\r\n", stats.jumpResetCount);
    DebugLog::printf("  stale tracks=%u\r\n", stats.staleTracks);
    DebugLog::printf("  active tracks=%u\r\n", stats.activeTracks);
    DebugLog::printf("  filtered_ground=%u filtered_altitude=%u filtered_speed=%u filtered_range=%u\r\n",
                     stats.filteredGround,
                     stats.filteredAltitude,
                     stats.filteredSpeed,
                     stats.filteredRange);
    DebugLog::printf("  rendered aircraft count=%u\r\n", stats.renderedAircraftCount);
}

AppConfig RadarApp::runtimeRenderConfig() const
{
    AppConfig renderConfig = config_;
    renderConfig.maxRangeKm = settings_.location.maxRangeKm;
    renderConfig.showLabels = settings_.display.showLabels;
    return renderConfig;
}
