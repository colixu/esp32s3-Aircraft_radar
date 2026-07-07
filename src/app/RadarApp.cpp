#include "RadarApp.h"

#include <WiFi.h>
#include <string.h>

#include "DebugLog.h"
#include "../utils/GeoUtils.h"

namespace
{
    constexpr uint32_t kScheduleCheckIntervalMs = 5000;

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
    settingsStore_.begin();
    settingsStore_.load(settings_);
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
    DebugLog::printf("RealRadar filters: ground=%s minAlt=%.0fm minSpeed=%.1fm/s\r\n",
                     settings_.filter.showGroundTraffic ? "show" : "hide",
                     settings_.filter.minAirborneAltitudeM,
                     settings_.filter.minAirborneSpeedMs);
    DebugLog::printf("OpenSky bbox: lat %.4f..%.4f lon %.4f..%.4f\r\n",
                     config_.openSkyLamin,
                     config_.openSkyLamax,
                     config_.openSkyLomin,
                     config_.openSkyLomax);
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
}

void RadarApp::handleInputEvent(InputEvent event)
{
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
            switchUiTheme();
            break;

        case InputEvent::SwitchRange:
            switchRange();
            break;

        case InputEvent::ToggleGroundTraffic:
            toggleGroundTraffic();
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
                renderRealRadarSystemStatus();
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

        case InputEvent::None:
        default:
            break;
    }
}

void RadarApp::printSerialHelp()
{
    DebugLog::println("Serial commands:");
    DebugLog::println("  h: help");
    DebugLog::println("  p: print UserSettings");
    DebugLog::println("  c: enter setup portal");
    DebugLog::println("  x: exit setup portal");
    DebugLog::println("  q: toggle setup QR/details");
    DebugLog::println("  w: show STA settings URL QR");
    DebugLog::println("  u: switch UI theme");
    DebugLog::println("  r: switch radar range");
    DebugLog::println("  g: toggle ground traffic");
    DebugLog::println("  s: save settings");
    DebugLog::println("  l: load settings");
    DebugLog::println("  t: print time/schedule status");
    DebugLog::println("  m: print device mode/state");
    DebugLog::println("  a: print API/auth status");
    DebugLog::println("  A: clear OpenSky token");
    DebugLog::println("  d: reset settings to default");
    DebugLog::println("  b: reboot device");
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
    if (settings_.location.maxRangeKm < 45.0f)
    {
        settings_.location.maxRangeKm = 60.0f;
    }
    else if (settings_.location.maxRangeKm < 90.0f)
    {
        settings_.location.maxRangeKm = 120.0f;
    }
    else
    {
        settings_.location.maxRangeKm = 30.0f;
    }

    sanitizeUserSettings(settings_);
    DebugLog::printf("Range switched: %.0fkm\r\n", settings_.location.maxRangeKm);
    settingsStore_.save(settings_);
}

void RadarApp::toggleGroundTraffic()
{
    settings_.filter.showGroundTraffic = !settings_.filter.showGroundTraffic;
    DebugLog::printf("showGroundTraffic=%u\r\n", settings_.filter.showGroundTraffic ? 1 : 0);
    settingsStore_.save(settings_);
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
        renderRealRadarFrame();
    }
    else if (config_.appMode == AppMode::RadarDemo)
    {
        renderFrame();
    }
}

void RadarApp::renderSettingsDisplay(const char *statusText)
{
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
}

void RadarApp::printDeviceStateStatus()
{
    DebugLog::println("Device mode/status:");
    DebugLog::printf("  appMode=%d state=%s wifi=%s\r\n",
                     static_cast<int>(config_.appMode),
                     deviceStateName(deviceState_),
                     wifi_.statusText());
    DebugLog::printf("  updater running=%u updating=%u interval=%lums\r\n",
                     realApiUpdater_.isRunning() ? 1 : 0,
                     realApiUpdater_.isUpdating() ? 1 : 0,
                     static_cast<unsigned long>(currentRealApiIntervalMs_));
}

void RadarApp::printApiAuthStatus()
{
    DebugLog::println("API/auth status:");
    DebugLog::printf("  accountMode=%s\r\n", apiAccountModeName(settings_.api.accountMode));
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
    configPortal_.stop();
    wifiLostSinceMs_ = 0;
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
        stopRealRadarUpdater();
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
        stopRealRadarUpdater();
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

    if (!wifi_.isConnected())
    {
        renderer_.renderSystemStatusFrame("WIFI LOST", "Reconnecting", "");
        return;
    }

    if (settings_.schedule.enabled && !timeManager_.isTimeSynced())
    {
        renderer_.renderSystemStatusFrame("TIME SYNC", "Waiting for NTP", "");
        return;
    }

    if (deviceState_ == DeviceState::PausedBySchedule)
    {
        char nextStart[8];
        char line3[24];
        formatMinutesOfDay(computeNextScheduleStartMinutes(settings_.schedule,
                                                           timeManager_.getLocalMinutesOfDay()),
                           nextStart,
                           sizeof(nextStart));
        snprintf(line3, sizeof(line3), "Next: %s", nextStart);
        renderer_.renderSystemStatusFrame("PAUSED", "Outside schedule", line3);
        return;
    }

    renderer_.renderSystemStatusFrame("STATUS", realRadarStatus_, "");
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
        if (now - lastFrameMs_ >= config_.frameIntervalMs)
        {
            lastFrameMs_ = now;
            renderRealRadarSystemStatus();
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
    const AppConfig renderConfig = runtimeRenderConfig();
    renderer_.renderRadarFrame(dataProvider_.aircraft(),
                               dataProvider_.count(),
                               selectedAircraftIndex_,
                               renderConfig,
                               settings_.display.uiTheme);
}

void RadarApp::renderRealRadarFrame()
{
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
    apiTestView_.render(wifi_, openSky_);
}

void RadarApp::updateRealRadarStatus()
{
    if (!wifi_.isConnected())
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "WIFI LOST");
        return;
    }

    if (settings_.api.accountMode == ApiAccountMode::OpenSkyClient &&
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
    if ((apiError != nullptr && strncmp(apiError, "AUTH CONFIG", 11) == 0) ||
        (authError != nullptr && strncmp(authError, "AUTH CONFIG", 11) == 0))
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "AUTH CONFIG");
        return;
    }

    if ((apiError != nullptr && strncmp(apiError, "AUTH", 4) == 0) ||
        (authError != nullptr && strncmp(authError, "AUTH", 4) == 0 && strcmp(authError, "OK") != 0))
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
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "NO AIRBORNE");
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
