#include "RadarApp.h"

RadarApp::RadarApp() :
    tft_(),
    renderer_(tft_)
{
}

void RadarApp::begin()
{
    Serial.begin(115200);
    delay(800);
    Serial.println();
    Serial.println("ESP32-S3 GC9A01 aircraft radar UI demo");

    dataProvider_.begin();
    renderer_.begin();
    renderFrame();

    Serial.println("Fake aircraft radar UI is running.");
}

void RadarApp::update()
{
    const uint32_t now = millis();

    updateAircraftData(now);
    updateSelectedAircraft(now);

    if (now - lastFrameMs_ < config_.frameIntervalMs)
    {
        return;
    }
    lastFrameMs_ = now;

    renderFrame();
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
    if (now - lastSelectionMs_ < config_.selectionIntervalMs)
    {
        return;
    }
    lastSelectionMs_ = now;

    const Aircraft *aircraft = dataProvider_.aircraft();
    do
    {
        selectedAircraftIndex_ = (selectedAircraftIndex_ + 1) % dataProvider_.count();
    } while (!aircraft[selectedAircraftIndex_].valid);
}

void RadarApp::renderFrame()
{
    renderer_.renderRadarFrame(dataProvider_.aircraft(),
                               dataProvider_.count(),
                               selectedAircraftIndex_,
                               config_);
}
