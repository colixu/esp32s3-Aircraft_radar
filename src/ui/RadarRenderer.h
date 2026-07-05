#pragma once

#include <TFT_eSPI.h>

#include "../aircraft/AircraftModel.h"
#include "../app/AppConfig.h"

class RadarRenderer
{
public:
    explicit RadarRenderer(TFT_eSPI &display);

    void begin();
    bool isReady() const;
    void renderRadarFrame(const Aircraft *aircraft,
                          uint8_t aircraftCount,
                          uint8_t selectedAircraftIndex,
                          const AppConfig &config);
    void advanceSweep(float stepDeg);

private:
    static constexpr int16_t kCenterX = 120;
    static constexpr int16_t kCenterY = 120;
    static constexpr int16_t kRadarRadius = 118;
    static constexpr uint8_t kScanTrailCount = 24;
    static constexpr uint8_t kScanTrailProfileCount = 32;

    TFT_eSPI &tft_;
    TFT_eSprite frame_;
    bool frameBufferReady_ = false;

    uint16_t radarGreen_;
    uint16_t dimGreen_;
    uint16_t sweepGreen_;
    uint16_t sweepTrail_[kScanTrailCount];
    float sweepTrailOffsetDeg_[kScanTrailCount];
    uint16_t aircraftGreen_;
    uint16_t selectedGreen_;
    uint16_t labelGreen_;
    float sweepAngleDeg_ = 0.0f;

    void printDisplaySetup();
    void initDisplay();
    void initFrameBuffer();
    void initColors();

    void radarToScreen(float bearingDeg,
                       float distanceKm,
                       float maxRangeKm,
                       int16_t &x,
                       int16_t &y) const;
    void drawRangeRings(TFT_eSprite &canvas);
    void drawReferenceLines(TFT_eSprite &canvas);
    void drawRadarBackground(TFT_eSprite &canvas);
    void drawScanSweep(TFT_eSprite &canvas);
    void drawAircraftTriangle(TFT_eSprite &canvas,
                              int16_t x,
                              int16_t y,
                              float headingDeg,
                              uint16_t color);
    void drawMinimalAircraftLabel(TFT_eSprite &canvas,
                                  const Aircraft &target,
                                  int16_t x,
                                  int16_t y);
    void drawAircraftSymbol(TFT_eSprite &canvas,
                            const Aircraft &target,
                            bool selected,
                            float maxRangeKm,
                            bool showLabels);
    void drawAircraftTargets(TFT_eSprite &canvas,
                             const Aircraft *aircraft,
                             uint8_t aircraftCount,
                             uint8_t selectedAircraftIndex,
                             const AppConfig &config);
};
