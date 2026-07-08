#pragma once

#include <TFT_eSPI.h>

#include "../aircraft/AircraftModel.h"
#include "../app/AppConfig.h"
#include "../app/UserSettings.h"

enum class SetupDisplayMode
{
    QrCode,
    Details
};

enum class SettingsDisplayMode
{
    ApQr,
    ApDetails,
    StaQr,
    StaDetails
};

class RadarRenderer
{
public:
    explicit RadarRenderer(TFT_eSPI &display);

    void begin();
    bool isReady() const;
    void renderRadarFrame(const Aircraft *aircraft,
                          uint8_t aircraftCount,
                          uint8_t selectedAircraftIndex,
                          const AppConfig &config,
                          UiTheme theme = UiTheme::ClassicRadar,
                          const char *statusText = nullptr);
    void advanceSweep(float stepDeg);
    void renderSetupPortalFrame(const char *apSsid,
                                const char *apPassword,
                                const char *ipAddress,
                                const char *statusText,
                                SetupDisplayMode mode);
    void renderSettingsFrame(const char *apSsid,
                             const char *apPassword,
                             const char *apIpAddress,
                             const char *staIpAddress,
                             const char *statusText,
                             SettingsDisplayMode mode);
    void renderSystemStatusFrame(const char *line1,
                                 const char *line2,
                                 const char *line3);
    void renderClockFrame(const char *timeText,
                          const char *dateText,
                          const char *nextRunText,
                          const char *hintText);
    void renderBlankFrame();

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
    uint16_t modernBg_;
    uint16_t modernGrid_;
    uint16_t modernText_;
    uint16_t modernCenter_;
    uint16_t modernAircraft_;
    uint16_t modernVector_;
    uint16_t modernTagType_;
    uint16_t modernTagAlt_;
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
    void drawStatusText(TFT_eSprite &canvas, const char *statusText);
    void renderClassicRadarFrame(const Aircraft *aircraft,
                                 uint8_t aircraftCount,
                                 uint8_t selectedAircraftIndex,
                                 const AppConfig &config,
                                 const char *statusText);
    void renderModernRadarFrame(const Aircraft *aircraft,
                                uint8_t aircraftCount,
                                uint8_t selectedAircraftIndex,
                                const AppConfig &config,
                                const char *statusText);
    void drawModernReferenceRadarFrame(TFT_eSprite &canvas, const AppConfig &config);
    void drawModernReferenceGrid(TFT_eSprite &canvas, const AppConfig &config);
    void drawModernReferenceRings(TFT_eSprite &canvas);
    void drawModernReferenceCrosshairs(TFT_eSprite &canvas);
    void drawModernReferenceCardinals(TFT_eSprite &canvas);
    void drawModernReferenceScaleLabel(TFT_eSprite &canvas, const AppConfig &config);
    void drawModernReferenceCenterDot(TFT_eSprite &canvas);
    void drawModernReferenceAircraft(TFT_eSprite &canvas,
                                     const Aircraft *aircraft,
                                     uint8_t aircraftCount,
                                     const AppConfig &config);
    void drawModernReferenceAircraftSymbol(TFT_eSprite &canvas,
                                           const Aircraft &target,
                                           int16_t x,
                                           int16_t y);
    void drawModernReferenceSpeedVector(TFT_eSprite &canvas,
                                        const Aircraft &target,
                                        int16_t x,
                                        int16_t y);
    void drawModernReferenceAircraftTag(TFT_eSprite &canvas,
                                        const Aircraft &target,
                                        int16_t x,
                                        int16_t y);
    void drawModernReferenceBeyondDot(TFT_eSprite &canvas, const Aircraft &target);
    bool modernReferenceToScreen(const Aircraft &target,
                                 const AppConfig &config,
                                 int16_t &x,
                                 int16_t &y,
                                 bool &insideOuterRing) const;
    int modernReferenceSpeedLineLengthPx(float speedMs) const;
    void modernReferenceNoseTip(int16_t x,
                                int16_t y,
                                float headingDeg,
                                int16_t &tipX,
                                int16_t &tipY) const;
    void clipModernReferencePointToOuterRing(int16_t x0,
                                             int16_t y0,
                                             int16_t &x1,
                                             int16_t &y1) const;
    void renderCyberpunkPlaceholder(uint8_t aircraftCount, const char *statusText);
};
