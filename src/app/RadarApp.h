#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "AppConfig.h"
#include "../data/FakeDataProvider.h"
#include "../ui/RadarRenderer.h"

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

    uint8_t selectedAircraftIndex_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t lastAircraftUpdateMs_ = 0;
    uint32_t lastSelectionMs_ = 0;

    void updateAircraftData(uint32_t now);
    void updateSelectedAircraft(uint32_t now);
    void renderFrame();
};
