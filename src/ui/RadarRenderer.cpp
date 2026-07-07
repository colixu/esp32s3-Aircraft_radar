#include "RadarRenderer.h"

#include <math.h>

#include "../app/DebugLog.h"

RadarRenderer::RadarRenderer(TFT_eSPI &display) :
    tft_(display),
    frame_(&display)
{
}

void RadarRenderer::begin()
{
    initColors();
    initDisplay();
    initFrameBuffer();
}

bool RadarRenderer::isReady() const
{
    return frameBufferReady_;
}

void RadarRenderer::renderRadarFrame(const Aircraft *aircraft,
                                     uint8_t aircraftCount,
                                     uint8_t selectedAircraftIndex,
                                     const AppConfig &config,
                                     UiTheme theme,
                                     const char *statusText)
{
    if (!frameBufferReady_)
    {
        return;
    }

    switch (theme)
    {
        case UiTheme::ModernRadar:
            renderModernPlaceholder(aircraftCount, statusText);
            return;
        case UiTheme::CyberpunkRadar:
            renderCyberpunkPlaceholder(aircraftCount, statusText);
            return;
        case UiTheme::ClassicRadar:
        default:
            renderClassicRadarFrame(aircraft, aircraftCount, selectedAircraftIndex, config, statusText);
            return;
    }
}

void RadarRenderer::renderClassicRadarFrame(const Aircraft *aircraft,
                                            uint8_t aircraftCount,
                                            uint8_t selectedAircraftIndex,
                                            const AppConfig &config,
                                            const char *statusText)
{
    drawRadarBackground(frame_);
    drawScanSweep(frame_);
    drawAircraftTargets(frame_, aircraft, aircraftCount, selectedAircraftIndex, config);
    drawStatusText(frame_, statusText);
    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderModernPlaceholder(uint8_t aircraftCount, const char *statusText)
{
    frame_.fillSprite(TFT_BLACK);

    const uint16_t lineColor = tft_.color565(60, 180, 180);
    const uint16_t textColor = tft_.color565(180, 255, 240);
    const uint16_t dimColor = tft_.color565(20, 80, 90);

    frame_.drawCircle(kCenterX, kCenterY, 116, lineColor);
    frame_.drawLine(38, 72, 202, 72, dimColor);
    frame_.drawLine(38, 168, 202, 168, dimColor);
    frame_.drawRect(58, 92, 124, 56, dimColor);

    char line[32];
    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(textColor, TFT_BLACK);
    frame_.drawString("MODERN MODE", kCenterX, 106, 2);
    snprintf(line, sizeof(line), "AIRCRAFT %u", aircraftCount);
    frame_.drawString(line, kCenterX, 128, 1);

    drawStatusText(frame_, statusText);
    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderCyberpunkPlaceholder(uint8_t aircraftCount, const char *statusText)
{
    frame_.fillSprite(TFT_BLACK);

    const uint16_t magenta = tft_.color565(220, 0, 160);
    const uint16_t cyan = tft_.color565(0, 220, 220);
    const uint16_t dimMagenta = tft_.color565(70, 0, 52);

    frame_.drawCircle(kCenterX, kCenterY, 116, dimMagenta);
    frame_.drawLine(48, 58, 190, 86, magenta);
    frame_.drawLine(50, 178, 192, 150, cyan);
    frame_.drawRect(64, 86, 112, 68, dimMagenta);
    frame_.drawFastHLine(72, 98, 96, cyan);
    frame_.drawFastHLine(72, 142, 96, magenta);

    char line[32];
    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(cyan, TFT_BLACK);
    frame_.drawString("CYBERPUNK MODE", kCenterX, 110, 2);
    snprintf(line, sizeof(line), "AIRCRAFT %u", aircraftCount);
    frame_.setTextColor(magenta, TFT_BLACK);
    frame_.drawString(line, kCenterX, 132, 1);

    drawStatusText(frame_, statusText);
    frame_.pushSprite(0, 0);
}

void RadarRenderer::advanceSweep(float stepDeg)
{
    sweepAngleDeg_ = AircraftModel::wrapDegrees(sweepAngleDeg_ + stepDeg);
}

void RadarRenderer::renderSetupPortalFrame(const char *apSsid,
                                           const char *apPassword,
                                           const char *ipAddress,
                                           const char *statusText)
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
    frame_.drawCircle(kCenterX, kCenterY, 116, radarGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 78, dimGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 40, dimGreen_);

    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(sweepGreen_, TFT_BLACK);
    frame_.drawString("SETUP MODE", kCenterX, 68, 2);

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString("AP", kCenterX, 94, 1);
    frame_.setTextColor(selectedGreen_, TFT_BLACK);
    frame_.drawString(apSsid != nullptr ? apSsid : "", kCenterX, 108, 1);

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString("PASS", kCenterX, 128, 1);
    frame_.setTextColor(selectedGreen_, TFT_BLACK);
    frame_.drawString(apPassword != nullptr ? apPassword : "", kCenterX, 142, 1);

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString("URL", kCenterX, 162, 1);
    frame_.setTextColor(selectedGreen_, TFT_BLACK);
    frame_.drawString(ipAddress != nullptr ? ipAddress : "192.168.4.1", kCenterX, 176, 1);

    if (statusText != nullptr && statusText[0] != '\0')
    {
        frame_.setTextColor(labelGreen_, TFT_BLACK);
        frame_.drawString(statusText, kCenterX, 204, 1);
    }

    frame_.pushSprite(0, 0);
}

void RadarRenderer::printDisplaySetup()
{
    setup_t setup;
    tft_.getSetup(setup);

    DebugLog::println("TFT_eSPI compiled setup:");
    DebugLog::printf("  driver: 0x%04X\r\n", setup.tft_driver);
    DebugLog::printf("  size: %dx%d\r\n", setup.tft_width, setup.tft_height);
    DebugLog::printf("  MOSI=%d MISO=%d SCLK=%d\r\n",
                     setup.pin_tft_mosi,
                     setup.pin_tft_miso,
                     setup.pin_tft_clk);
    DebugLog::printf("  CS=%d DC=%d RST=%d\r\n",
                     setup.pin_tft_cs,
                     setup.pin_tft_dc,
                     setup.pin_tft_rst);
    DebugLog::printf("  SPI frequency: %lu Hz\r\n",
                     static_cast<unsigned long>(setup.tft_spi_freq) * 100000UL);
}

void RadarRenderer::initDisplay()
{
    printDisplaySetup();

    DebugLog::println("Initializing GC9A01 TFT...");
    tft_.init();
    tft_.setRotation(0);
    // This GC9A01 panel needs inversion enabled: otherwise black appears white
    // and green radar graphics appear magenta/red.
    tft_.invertDisplay(true);
    tft_.fillScreen(TFT_BLACK);

    DebugLog::println("GC9A01 init complete.");
}

void RadarRenderer::initFrameBuffer()
{
    DebugLog::println("Creating 240x240 16-bit sprite frame buffer...");
    frame_.setColorDepth(16);
    void *buffer = frame_.createSprite(240, 240);

    if (buffer != nullptr)
    {
        frameBufferReady_ = true;
        DebugLog::println("16-bit sprite frame buffer ready.");
        return;
    }

    DebugLog::println("16-bit sprite allocation failed, trying 8-bit sprite...");
    frame_.deleteSprite();
    frame_.setColorDepth(8);
    buffer = frame_.createSprite(240, 240);

    if (buffer != nullptr)
    {
        frameBufferReady_ = true;
        DebugLog::println("8-bit sprite frame buffer ready.");
        return;
    }

    frameBufferReady_ = false;
    DebugLog::println("ERROR: sprite frame buffer allocation failed.");
}

void RadarRenderer::initColors()
{
    radarGreen_ = tft_.color565(0, 220, 70);
    dimGreen_ = tft_.color565(0, 42, 18);
    sweepGreen_ = tft_.color565(80, 255, 120);
    aircraftGreen_ = tft_.color565(0, 210, 70);
    selectedGreen_ = tft_.color565(140, 255, 160);
    labelGreen_ = tft_.color565(0, 120, 48);

    for (uint8_t i = 0; i < kScanTrailCount; ++i)
    {
        const float t = static_cast<float>(i + 1) / kScanTrailProfileCount;
        const float eased = powf(t, 2.45f);
        const float brightness = powf(1.0f - t, 1.55f);
        const uint8_t green = 7 + static_cast<uint8_t>(150.0f * brightness);
        const uint8_t blue = 3 + static_cast<uint8_t>(44.0f * brightness);

        sweepTrailOffsetDeg_[i] = 0.8f + eased * 88.0f;
        sweepTrail_[i] = tft_.color565(0, green, blue);
    }
}

void RadarRenderer::radarToScreen(float bearingDeg,
                                  float distanceKm,
                                  float maxRangeKm,
                                  int16_t &x,
                                  int16_t &y) const
{
    const float range = constrain(distanceKm, 0.0f, maxRangeKm);
    const float radius = (range / maxRangeKm) * kRadarRadius;
    const float radians = bearingDeg * DEG_TO_RAD;

    x = kCenterX + static_cast<int16_t>(sinf(radians) * radius);
    y = kCenterY - static_cast<int16_t>(cosf(radians) * radius);
}

void RadarRenderer::drawRangeRings(TFT_eSprite &canvas)
{
    canvas.drawCircle(kCenterX, kCenterY, kRadarRadius, radarGreen_);
    canvas.drawCircle(kCenterX, kCenterY, 79, dimGreen_);
    canvas.drawCircle(kCenterX, kCenterY, 39, dimGreen_);
}

void RadarRenderer::drawReferenceLines(TFT_eSprite &canvas)
{
    canvas.drawLine(kCenterX - kRadarRadius, kCenterY,
                    kCenterX + kRadarRadius, kCenterY, dimGreen_);
    canvas.drawLine(kCenterX, kCenterY - kRadarRadius,
                    kCenterX, kCenterY + kRadarRadius, dimGreen_);
}

void RadarRenderer::drawRadarBackground(TFT_eSprite &canvas)
{
    canvas.fillSprite(TFT_BLACK);
    drawRangeRings(canvas);
    drawReferenceLines(canvas);
    canvas.fillCircle(kCenterX, kCenterY, 3, radarGreen_);
}

void RadarRenderer::drawScanSweep(TFT_eSprite &canvas)
{
    for (uint8_t i = kScanTrailCount; i > 0; --i)
    {
        const uint8_t index = i - 1;
        const float angle = AircraftModel::wrapDegrees(sweepAngleDeg_ - sweepTrailOffsetDeg_[index]);
        const float radians = angle * DEG_TO_RAD;
        const int16_t endX = kCenterX + static_cast<int16_t>(sinf(radians) * kRadarRadius);
        const int16_t endY = kCenterY - static_cast<int16_t>(cosf(radians) * kRadarRadius);
        canvas.drawLine(kCenterX, kCenterY, endX, endY, sweepTrail_[index]);
    }

    const float radians = sweepAngleDeg_ * DEG_TO_RAD;
    const int16_t endX = kCenterX + static_cast<int16_t>(sinf(radians) * kRadarRadius);
    const int16_t endY = kCenterY - static_cast<int16_t>(cosf(radians) * kRadarRadius);
    canvas.drawLine(kCenterX, kCenterY, endX, endY, sweepGreen_);
}

void RadarRenderer::drawAircraftTriangle(TFT_eSprite &canvas,
                                         int16_t x,
                                         int16_t y,
                                         float headingDeg,
                                         uint16_t color)
{
    const float heading = headingDeg * DEG_TO_RAD;
    const float left = (headingDeg + 145.0f) * DEG_TO_RAD;
    const float right = (headingDeg - 145.0f) * DEG_TO_RAD;

    const int16_t tipX = x + static_cast<int16_t>(sinf(heading) * 6);
    const int16_t tipY = y - static_cast<int16_t>(cosf(heading) * 6);
    const int16_t leftX = x + static_cast<int16_t>(sinf(left) * 4);
    const int16_t leftY = y - static_cast<int16_t>(cosf(left) * 4);
    const int16_t rightX = x + static_cast<int16_t>(sinf(right) * 4);
    const int16_t rightY = y - static_cast<int16_t>(cosf(right) * 4);

    canvas.fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, color);
}

void RadarRenderer::drawMinimalAircraftLabel(TFT_eSprite &canvas,
                                             const Aircraft &target,
                                             int16_t x,
                                             int16_t y)
{
    const bool placeLeft = x > kCenterX;
    int16_t labelX = placeLeft ? x - 10 : x + 10;
    int16_t labelY = y - 8;

    if (y < 34)
    {
        labelY = y + 12;
    }
    else if (y > 202)
    {
        labelY = y - 24;
    }

    labelY = constrain(labelY, 22, 206);

    if (placeLeft)
    {
        labelX = constrain(labelX, 48, 222);
        canvas.setTextDatum(MR_DATUM);
    }
    else
    {
        labelX = constrain(labelX, 18, 192);
        canvas.setTextDatum(ML_DATUM);
    }

    char line[28];
    snprintf(line, sizeof(line), "%s", target.callsign);
    canvas.setTextColor(labelGreen_, TFT_BLACK);
    canvas.drawString(line, labelX, labelY, 1);

    snprintf(line, sizeof(line), "%.0fkm %.0fm", target.distanceKm, target.altitudeM);
    canvas.drawString(line, labelX, labelY + 10, 1);
}

void RadarRenderer::drawAircraftSymbol(TFT_eSprite &canvas,
                                       const Aircraft &target,
                                       bool selected,
                                       float maxRangeKm,
                                       bool showLabels)
{
    if (!target.valid)
    {
        return;
    }

    int16_t x = 0;
    int16_t y = 0;
    radarToScreen(target.bearingDeg, target.distanceKm, maxRangeKm, x, y);

    const bool litBySweep = AircraftModel::shortestAngleDelta(target.bearingDeg, sweepAngleDeg_) < 8.0f;
    const uint16_t color = selected ? selectedGreen_ : (litBySweep ? sweepGreen_ : aircraftGreen_);

    drawAircraftTriangle(canvas, x, y, target.headingDeg, color);
    if (selected)
    {
        canvas.drawCircle(x, y, 8, selectedGreen_);
        if (showLabels)
        {
            drawMinimalAircraftLabel(canvas, target, x, y);
        }
    }
}

void RadarRenderer::drawAircraftTargets(TFT_eSprite &canvas,
                                        const Aircraft *aircraft,
                                        uint8_t aircraftCount,
                                        uint8_t selectedAircraftIndex,
                                        const AppConfig &config)
{
    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        drawAircraftSymbol(canvas,
                           aircraft[i],
                           i == selectedAircraftIndex,
                           config.maxRangeKm,
                           config.showLabels);
    }
}

void RadarRenderer::drawStatusText(TFT_eSprite &canvas, const char *statusText)
{
    if (statusText == nullptr || statusText[0] == '\0')
    {
        return;
    }

    canvas.setTextDatum(BC_DATUM);
    canvas.setTextColor(labelGreen_, TFT_BLACK);
    canvas.drawString(statusText, kCenterX, 226, 1);
}
