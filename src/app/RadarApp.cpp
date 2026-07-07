#include "RadarApp.h"

#include <WiFi.h>

#include "DebugLog.h"
#include "../utils/GeoUtils.h"

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
    inputManager_.begin(settings_.system.uiButtonPin);
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
        beginRealRadar();
        return;
    }

    beginRadarDemo();
}

void RadarApp::update()
{
    const uint32_t now = millis();
    updateInput();

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
    setDeviceState(DeviceState::Running);
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

    AircraftModel::clearAircraft(realAircraft_, AircraftModel::kAircraftCount);
    realAircraftCount_ = 0;
    realTrackManager_.begin();
    updateRealRadarStatus();

    if (!renderer_.isReady())
    {
        renderer_.begin();
    }
    startWifiManagerFromSettings();
    realApiUpdater_.begin(config_, settings_, activeRequestIntervalMs(settings_));
    renderRealRadarFrame();
}

void RadarApp::updateInput()
{
    inputManager_.update();

    if (inputManager_.wasHelpPressed())
    {
        printSerialHelp();
    }
    if (inputManager_.wasRebootPressed())
    {
        DebugLog::println("Reboot requested from serial.");
        delay(100);
        ESP.restart();
    }
    if (inputManager_.wasConfigPortalPressed())
    {
        enterSetupPortal("Serial command");
        return;
    }
    if (inputManager_.wasExitConfigPortalPressed())
    {
        if (deviceState_ == DeviceState::SetupPortal)
        {
            exitSetupPortal();
        }
        return;
    }
    if (inputManager_.wasUiSwitchPressed())
    {
        switchUiTheme();
    }
    if (inputManager_.wasRangeSwitchPressed())
    {
        switchRange();
    }
    if (inputManager_.wasGroundTogglePressed())
    {
        toggleGroundTraffic();
    }
    if (inputManager_.wasPrintSettingsPressed())
    {
        printUserSettings(settings_);
    }
    if (inputManager_.wasResetDefaultsPressed())
    {
        resetSettingsToDefault();
    }
    if (inputManager_.wasSaveSettingsPressed())
    {
        DebugLog::println("Manual settings save requested.");
        settingsStore_.save(settings_);
    }
    if (inputManager_.wasLoadSettingsPressed())
    {
        DebugLog::println("Manual settings load requested.");
        settingsStore_.load(settings_);
        inputManager_.begin(settings_.system.uiButtonPin);
        if (config_.appMode == AppMode::RealRadar)
        {
            renderRealRadarFrame();
        }
    }
}

void RadarApp::printSerialHelp()
{
    DebugLog::println("Serial commands:");
    DebugLog::println("  h: help");
    DebugLog::println("  p: print UserSettings");
    DebugLog::println("  c: enter setup portal");
    DebugLog::println("  x: exit setup portal");
    DebugLog::println("  u: switch UI theme");
    DebugLog::println("  r: switch radar range");
    DebugLog::println("  g: toggle ground traffic");
    DebugLog::println("  s: save settings");
    DebugLog::println("  l: load settings");
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

void RadarApp::resetSettingsToDefault()
{
    settingsStore_.resetToDefault(settings_);
    DebugLog::println("User settings reset to default.");
    printUserSettings(settings_);
    settingsStore_.save(settings_);
}

void RadarApp::enterSetupPortal(const char *reason)
{
    if (configPortal_.isRunning())
    {
        renderSetupPortalFrame(reason);
        return;
    }

    DebugLog::printf("Entering setup portal: %s\r\n", reason != nullptr ? reason : "requested");
    realApiUpdater_.stop();
    wifiManagerStarted_ = false;
    setDeviceState(DeviceState::SetupPortal);

    if (!renderer_.isReady())
    {
        renderer_.begin();
    }

    configPortal_.begin(&settings_, &settingsStore_);
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
    renderer_.renderSetupPortalFrame(configPortal_.apSsid(),
                                     configPortal_.apPassword(),
                                     configPortal_.ipAddress(),
                                     statusText);
}

void RadarApp::setDeviceState(DeviceState state)
{
    if (deviceState_ == state)
    {
        return;
    }

    deviceState_ = state;
    DebugLog::printf("Device state: %d\r\n", static_cast<int>(deviceState_));
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
        wifi_.update(millis(), 1000);
        delay(100);
    }

    if (!wifi_.isConnected())
    {
        setDeviceState(DeviceState::WiFiLost);
        DebugLog::println("Configured WiFi connection failed.");
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

void RadarApp::updateRadarDemo(uint32_t now)
{
    updateAircraftData(now);
    updateSelectedAircraftForList(dataProvider_.aircraft(), dataProvider_.count(), now);

    if (now - lastFrameMs_ < config_.frameIntervalMs)
    {
        return;
    }
    lastFrameMs_ = now;

    renderFrame();
    renderer_.advanceSweep(config_.sweepStepDeg);
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
    renderRealRadarFrame();
    renderer_.advanceSweep(config_.sweepStepDeg);
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

    if (realApiUpdater_.isUpdating())
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "UPDATING");
        return;
    }

    if (realApiUpdater_.lastHttpStatus() == 429)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "HTTP 429");
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

    snprintf(realRadarStatus_, sizeof(realRadarStatus_), "LIVE N=%u %lus", realAircraftCount_, static_cast<unsigned long>(ageSec));
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
