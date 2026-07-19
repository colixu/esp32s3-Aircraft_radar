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
#include "../ui/RadarUiTuning.h"
#include "ConfigPortal.h"
#include "InputManager.h"
#include "SettingsStore.h"
#include "SystemStatus.h"
#include "TimeManager.h"
#include "UserSettings.h"
#include "WifiManagerSimple.h"

enum class DebugMode
{
    None,
    UiLab
};

enum class LocalMenuPage
{
    Closed,
    MainMenu,
    BrightnessAdjust,
    IdleDisplayAdjust,
    ConfirmReboot,
    ConfirmApSetup
};

enum class LocalMenuItem
{
    SettingsQr,
    Reboot,
    ScreenSleep,
    Brightness,
    IdleDisplay,
    ApSetup,
    Exit
};

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
    Aircraft uiLabAircraft_[AircraftModel::kAircraftCount];
    uint8_t realAircraftCount_ = 0;
    uint8_t uiLabAircraftCount_ = 0;
    uint16_t lastRealRadarFilteredCount_ = 0;
    char realRadarStatus_[32] = "LIVE WAIT";
    DeviceState deviceState_ = DeviceState::Boot;
    DeviceState uiLabPreviousDeviceState_ = DeviceState::Boot;
    DebugMode debugMode_ = DebugMode::None;
    UiTheme uiLabTheme_ = UiTheme::ModernRadar;
    uint8_t uiLabSceneIndex_ = 0;
    RadarUiTuning uiTuning_;
    SetupDisplayMode setupDisplayMode_ = SetupDisplayMode::QrCode;
    SettingsDisplayMode settingsDisplayMode_ = SettingsDisplayMode::ApQr;
    bool staSettingsOverlayVisible_ = false;
    bool wifiManagerStarted_ = false;
    bool screenSleeping_ = false;
    bool setupPortalFromLocalMenu_ = false;
    LocalMenuPage localMenuPage_ = LocalMenuPage::Closed;
    uint8_t localMenuIndex_ = 0;
    uint8_t localMenuBrightnessIndex_ = 0;
    ScheduleIdleDisplayMode localMenuIdleDisplayMode_ = ScheduleIdleDisplayMode::PausedStatus;
    uint32_t localMenuOpenedMs_ = 0;
    uint32_t localMenuLastInputMs_ = 0;
    uint32_t localMenuLastRenderMs_ = 0;

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
    uint32_t lastSystemStatusLogMs_ = 0;
    uint32_t wifiLostSinceMs_ = 0;
    uint32_t currentRealApiIntervalMs_ = 0;
    uint32_t apiRequestCount_ = 0;
    uint32_t apiErrorCount_ = 0;
    uint32_t lastApiErrorMs_ = 0;
    uint32_t idleUiPreviewUntilMs_ = 0;
    uint32_t idleUiPreviewRefreshMs_ = 0;
    uint32_t idleUiPreviewApiIntervalMs_ = 0;

    void beginConfiguredMode();
    void beginRadarDemo();
    void beginApiTest();
    void beginRealRadar();
    void updateInput();
    void handleInputEvent(InputEvent event);
    void handleButtonInputEvent(InputEvent event);
    void wakeScreenFromSleep();
    void openLocalMenu();
    void closeLocalMenu(bool restoreDisplay = true);
    void updateLocalMenu(uint32_t now);
    void handleLocalMenuButtonEvent(InputEvent event);
    void executeLocalMenuItem();
    void renderLocalMenu();
    void renderLocalMenuPage();
    void renderCurrentDisplay();
    bool isIdleUiPreviewActive(uint32_t now) const;
    void startIdleUiPreview(uint32_t now, uint32_t durationMs, uint32_t refreshMs, uint32_t apiIntervalMs = 0);
    void updateIdleRealRadarPreview(uint32_t now, bool previewActive);
    void renderIdleUiPreviewFrame();
    uint8_t brightnessIndexFromValue(uint8_t brightness) const;
    uint8_t brightnessValueFromIndex(uint8_t index) const;
    const char *idleDisplayMenuName(ScheduleIdleDisplayMode mode) const;
    void handleUiTuningCommand(const UiTuningCommand &command);
    void printSerialHelp();
    void toggleUiLab();
    void enterUiLab();
    void exitUiLab();
    void updateUiLab(uint32_t now);
    void renderUiLabFrame();
    void loadUiLabScene(uint8_t sceneIndex);
    void nextUiLabTheme();
    void nextUiLabScene();
    void resetUiTuning();
    void saveUiTuning();
    void printUiLabStatus();
    bool applyUiTuningColor(const char *key, const UiTuningCommand &command);
    bool applyUiTuningValue(const char *key, const UiTuningCommand &command);
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
    SystemStatus getSystemStatus() const;
    void updateLongRunStatusLog(uint32_t now);
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
