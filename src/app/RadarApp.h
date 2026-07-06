#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "AppConfig.h"
#include "../aircraft/AircraftModel.h"
#include "../data/FakeDataProvider.h"
#include "../data/OpenSkyProvider.h"
#include "../ui/ApiTestView.h"
#include "../ui/RadarRenderer.h"
#include "WifiManagerSimple.h"

class RadarApp
{
public:
    RadarApp();

    void begin();
    void update();

private:
    AppConfig config_;
    TFT_eSPI tft_;
    FakeDataProvider dataProvider_;
    RadarRenderer renderer_;
    WifiManagerSimple wifi_;
    OpenSkyProvider openSky_;
    ApiTestView apiTestView_;
    Aircraft realAircraft_[AircraftModel::kAircraftCount];
    float realAircraftLat_[AircraftModel::kAircraftCount];
    float realAircraftLon_[AircraftModel::kAircraftCount];
    bool realAircraftOnGround_[AircraftModel::kAircraftCount];
    uint8_t realAircraftCount_ = 0;
    char realRadarStatus_[32] = "LIVE WAIT";

    struct RealRadarFilterStats
    {
        uint16_t filteredGround = 0;
        uint16_t filteredAltitude = 0;
        uint16_t filteredSpeed = 0;
        uint16_t filteredRange = 0;
    };

    uint8_t selectedAircraftIndex_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t lastAircraftUpdateMs_ = 0;
    uint32_t lastSelectionMs_ = 0;
    uint32_t lastApiRequestMs_ = 0;
    uint32_t lastApiScreenMs_ = 0;
    uint32_t lastApiSerialMs_ = 0;

    void beginRadarDemo();
    void beginApiTest();
    void beginRealRadar();
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
    void requestRealTraffic();
    void convertApiAircraftToRadar();
    void addRealAircraftSorted(const ApiAircraft &source,
                               const char *displayName,
                               float distanceKm,
                               float bearingDeg);
    void printRealRadarFilterSummary(const RealRadarFilterStats &stats);
    void updateRealRadarStatus();
};
