#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "AppConfig.h"
#include "../aircraft/AircraftModel.h"
#include "../data/FakeDataProvider.h"
#include "../data/OpenSkyAsyncUpdater.h"
#include "../data/OpenSkyProvider.h"
#include "../data/RealRadarTrackManager.h"
#include "../ui/ApiTestView.h"
#include "../ui/RadarRenderer.h"
#include "ConfigPortal.h"
#include "InputManager.h"
#include "SettingsStore.h"
#include "TimeManager.h"
#include "UserSettings.h"
#include "WifiManagerSimple.h"

class RadarApp
{
public:
    RadarApp();

    void begin();
    void update();

private:
    AppConfig config_;
    SettingsStore settingsStore_;
    UserSettings settings_;
    InputManager inputManager_;
    ConfigPortal configPortal_;
    TFT_eSPI tft_;
    FakeDataProvider dataProvider_;
    RadarRenderer renderer_;
    TimeManager timeManager_;
    WifiManagerSimple wifi_;
    OpenSkyProvider openSky_;
    OpenSkyAsyncUpdater realApiUpdater_;
    RealRadarTrackManager realTrackManager_;
    ApiTestView apiTestView_;
    Aircraft realAircraft_[AircraftModel::kAircraftCount];
    uint8_t realAircraftCount_ = 0;
    char realRadarStatus_[32] = "LIVE WAIT";
    DeviceState deviceState_ = DeviceState::Boot;
    SetupDisplayMode setupDisplayMode_ = SetupDisplayMode::QrCode;
    SettingsDisplayMode settingsDisplayMode_ = SettingsDisplayMode::ApQr;
    bool staSettingsOverlayVisible_ = false;
    bool wifiManagerStarted_ = false;

    uint8_t selectedAircraftIndex_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t lastAircraftUpdateMs_ = 0;
    uint32_t lastSelectionMs_ = 0;
    uint32_t lastApiRequestMs_ = 0;
    uint32_t lastApiScreenMs_ = 0;
    uint32_t lastApiSerialMs_ = 0;
    uint32_t lastPredictionSummaryMs_ = 0;
    uint32_t lastScheduleCheckMs_ = 0;
    uint32_t lastTimeSyncLogMs_ = 0;
    uint32_t lastIdleDisplayRenderMs_ = 0;
    uint32_t wifiLostSinceMs_ = 0;
    uint32_t currentRealApiIntervalMs_ = 0;

    void beginConfiguredMode();
    void beginRadarDemo();
    void beginApiTest();
    void beginRealRadar();
    void updateInput();
    void handleInputEvent(InputEvent event);
    void printSerialHelp();
    void switchUiTheme();
    void switchRange();
    void toggleGroundTraffic();
    void cycleScheduleIdleDisplayMode();
    void toggleSetupDisplayMode();
    void showStaSettingsOverlay();
    void hideStaSettingsOverlay();
    bool hasActiveOverlay() const;
    void renderSettingsDisplay(const char *statusText);
    void beginStaSettingsServer();
    void resetSettingsToDefault();
    void printTimeStatus();
    void printDeviceStateStatus();
    void printApiAuthStatus();
    void clearAuthToken();
    void enterSetupPortal(const char *reason);
    void exitSetupPortal();
    void updateSetupPortal(uint32_t now);
    void renderSetupPortalFrame(const char *statusText);
    void setDeviceState(DeviceState state, const char *reason = nullptr);
    bool connectToConfiguredWiFi();
    void startWifiManagerFromSettings();
    bool updateRealRadarRunGate(uint32_t now, bool forceCheck);
    void ensureRealRadarUpdaterRunning();
    void stopRealRadarUpdater();
    void renderRealRadarSystemStatus();
    void renderPausedIdleFrame(bool force);
    void formatMinutesOfDay(int16_t minutes, char *buffer, size_t bufferSize) const;
    void updateRadarDemo(uint32_t now);
    void updateApiTest(uint32_t now);
    void updateRealRadar(uint32_t now);
    void updateAircraftData(uint32_t now);
    void updateSelectedAircraft(uint32_t now);
    void updateSelectedAircraftForList(const Aircraft *aircraft, uint8_t aircraftCount, uint32_t now);
    void renderFrame();
    void renderRealRadarFrame();
    void renderApiTestScreen();
    void printApiTestSerialStatus();
    void handleRealRadarSnapshot(const OpenSkySnapshot &snapshot, uint32_t now);
    void rebuildRealRadarAircraft(RealRadarTrackStats &stats);
    void printRealRadarTrackSummary(const OpenSkySnapshot &snapshot, const RealRadarTrackStats &stats);
    void updateRealRadarStatus();
    AppConfig runtimeRenderConfig() const;
};
