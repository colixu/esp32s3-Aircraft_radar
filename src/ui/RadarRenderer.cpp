#include "RadarRenderer.h"

#include <math.h>

#include "../app/DebugLog.h"
#include "QrCodeRenderer.h"

namespace
{
    struct ModernRadarTheme
    {
        static constexpr int16_t size = 240;
        static constexpr int16_t centerX = 120;
        static constexpr int16_t centerY = 120;
        static constexpr int16_t outerRadius = 107;
        static constexpr uint8_t ringCount = 4;
        static constexpr float gridStrokeHalfWidth = 1.0f;
        static constexpr int16_t centerDotRadius = 2;
        static constexpr int16_t noseLength = 8;
        static constexpr int16_t tailLength = 3;
        static constexpr int16_t tailHalfWidth = 4;
        static constexpr int16_t aircraftLabelGap = 1;
        static constexpr int16_t insideRingInset = noseLength + tailHalfWidth + 1;
        static constexpr int16_t beyondRingDotRadius = 4;
        static constexpr int16_t beyondRingScreenMargin = 2;
        static constexpr uint32_t trackHorizonSec = 60;
        static constexpr int16_t speedLineMinPx = 2;
        static constexpr float trackRefOuterKm = 13.3f;
        static constexpr float trackLengthScale = 1.5f / 5.0f;
        static constexpr float trackLineHalfWidth = 1.0f;
    };

    struct CyberpunkRadarTheme
    {
        static constexpr int16_t size = 240;
        static constexpr int16_t centerX = 120;
        static constexpr int16_t centerY = 120;
        static constexpr int16_t aircraftNoseLength = 7;
        static constexpr int16_t aircraftTailLength = 3;
        static constexpr int16_t aircraftTailHalfWidth = 4;
        static constexpr uint32_t trackHorizonSec = 45;
        static constexpr int16_t speedLineMinPx = 3;
        static constexpr int16_t speedLineMaxPx = 28;
    };

    void drawModernWideLine(TFT_eSprite &canvas,
                            int16_t x0,
                            int16_t y0,
                            int16_t x1,
                            int16_t y1,
                            uint16_t color,
                            uint8_t width)
    {
        canvas.drawLine(x0, y0, x1, y1, color);
        for (uint8_t i = 1; i < width; ++i)
        {
            const int16_t offset = static_cast<int16_t>(i);
            if (abs(x1 - x0) >= abs(y1 - y0))
            {
                canvas.drawLine(x0, y0 + offset, x1, y1 + offset, color);
            }
            else
            {
                canvas.drawLine(x0 + offset, y0, x1 + offset, y1, color);
            }
        }
    }

    void drawModernWideCircle(TFT_eSprite &canvas,
                              int16_t x,
                              int16_t y,
                              int16_t radius,
                              uint16_t color,
                              uint8_t width)
    {
        for (uint8_t i = 0; i < width; ++i)
        {
            canvas.drawCircle(x, y, radius - i, color);
        }
    }
}

RadarRenderer::RadarRenderer(TFT_eSPI &display) :
    tft_(display),
    frame_(&display)
{
    loadDefaultRadarUiTuning(defaultUiTuning_);
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

void RadarRenderer::setUiTuning(const RadarUiTuning *tuning)
{
    uiTuning_ = tuning;
}

const ModernRadarTuning &RadarRenderer::modernTuning() const
{
    return uiTuning_ != nullptr ? uiTuning_->modern : defaultUiTuning_.modern;
}

const CyberpunkRadarTuning &RadarRenderer::cyberpunkTuning() const
{
    return uiTuning_ != nullptr ? uiTuning_->cyberpunk : defaultUiTuning_.cyberpunk;
}

uint16_t RadarRenderer::color565Scaled(const RgbColor &color, float globalBrightness, float localBrightness)
{
    const float brightness = constrain(globalBrightness, 0.0f, 2.0f) *
                             constrain(localBrightness, 0.0f, 2.0f);
    const uint8_t r = static_cast<uint8_t>(constrain(static_cast<int>(color.r * brightness), 0, 255));
    const uint8_t g = static_cast<uint8_t>(constrain(static_cast<int>(color.g * brightness), 0, 255));
    const uint8_t b = static_cast<uint8_t>(constrain(static_cast<int>(color.b * brightness), 0, 255));
    return tft_.color565(r, g, b);
}

uint16_t RadarRenderer::modernBackgroundColor()
{
    const ModernRadarTuning &tuning = modernTuning();
    return color565Scaled(tuning.background,
                          tuning.globalBrightness,
                          tuning.backgroundBrightness);
}

uint16_t RadarRenderer::modernGridColor()
{
    const ModernRadarTuning &tuning = modernTuning();
    return color565Scaled(tuning.grid,
                          tuning.globalBrightness,
                          tuning.gridBrightness);
}

uint16_t RadarRenderer::modernTextColor()
{
    const ModernRadarTuning &tuning = modernTuning();
    return color565Scaled(tuning.text,
                          tuning.globalBrightness,
                          tuning.textBrightness);
}

uint16_t RadarRenderer::modernAircraftColor()
{
    const ModernRadarTuning &tuning = modernTuning();
    return color565Scaled(tuning.aircraft, tuning.globalBrightness, 1.0f);
}

uint16_t RadarRenderer::modernVectorColor()
{
    const ModernRadarTuning &tuning = modernTuning();
    return color565Scaled(tuning.vector, tuning.globalBrightness, 1.0f);
}

uint16_t RadarRenderer::modernAltitudeColor()
{
    const ModernRadarTuning &tuning = modernTuning();
    return color565Scaled(tuning.altitudeText,
                          tuning.globalBrightness,
                          tuning.textBrightness);
}

uint16_t RadarRenderer::modernCenterColor()
{
    const ModernRadarTuning &tuning = modernTuning();
    return color565Scaled(tuning.selected,
                          tuning.globalBrightness,
                          tuning.textBrightness);
}

uint16_t RadarRenderer::cyberpunkColor(const RgbColor &color, float localBrightness)
{
    return color565Scaled(color, cyberpunkTuning().globalBrightness, localBrightness);
}

uint16_t RadarRenderer::cyberBackgroundColor()
{
    return cyberpunkColor(cyberpunkTuning().background, 1.0f);
}

uint16_t RadarRenderer::cyberNoiseColor()
{
    return cyberpunkColor(cyberpunkTuning().backgroundNoise, 0.8f);
}

uint16_t RadarRenderer::cyberOuterRingColor()
{
    return cyberpunkColor(cyberpunkTuning().outerRing, cyberpunkTuning().ringBrightness);
}

uint16_t RadarRenderer::cyberRingColor()
{
    return cyberpunkColor(cyberpunkTuning().ring, cyberpunkTuning().ringBrightness);
}

uint16_t RadarRenderer::cyberRingDimColor()
{
    return cyberpunkColor(cyberpunkTuning().ringDim, cyberpunkTuning().ringBrightness);
}

uint16_t RadarRenderer::cyberCrosshairColor()
{
    return cyberpunkColor(cyberpunkTuning().crosshair, cyberpunkTuning().ringBrightness * 0.75f);
}

uint16_t RadarRenderer::cyberTickColor()
{
    return cyberpunkColor(cyberpunkTuning().tick, cyberpunkTuning().ringBrightness);
}

uint16_t RadarRenderer::cyberMagentaColor()
{
    return cyberpunkColor(cyberpunkTuning().magenta, cyberpunkTuning().ringBrightness);
}

uint16_t RadarRenderer::cyberAircraftColor()
{
    return cyberpunkColor(cyberpunkTuning().aircraft, cyberpunkTuning().aircraftBrightness);
}

uint16_t RadarRenderer::cyberAircraftGlowColor()
{
    return cyberpunkColor(cyberpunkTuning().aircraftGlow, cyberpunkTuning().aircraftBrightness * 0.65f);
}

uint16_t RadarRenderer::cyberTextColor()
{
    return cyberpunkColor(cyberpunkTuning().text, cyberpunkTuning().textBrightness);
}

uint16_t RadarRenderer::cyberAltitudeColor()
{
    return cyberpunkColor(cyberpunkTuning().altitudeText, cyberpunkTuning().textBrightness);
}

uint16_t RadarRenderer::cyberSelectedColor()
{
    return cyberpunkColor(cyberpunkTuning().selected, cyberpunkTuning().textBrightness);
}

uint16_t RadarRenderer::cyberSweepColor()
{
    return cyberpunkColor(cyberpunkTuning().sweep, cyberpunkTuning().sweepBrightness);
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
            renderModernRadarFrame(aircraft, aircraftCount, selectedAircraftIndex, config, statusText);
            return;
        case UiTheme::CyberpunkRadar:
            renderCyberpunkRadarFrame(aircraft, aircraftCount, selectedAircraftIndex, config, statusText);
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

void RadarRenderer::renderModernRadarFrame(const Aircraft *aircraft,
                                           uint8_t aircraftCount,
                                           uint8_t selectedAircraftIndex,
                                           const AppConfig &config,
                                           const char *statusText)
{
    (void)selectedAircraftIndex;
    (void)statusText;

    drawModernReferenceRadarFrame(frame_, config);
    drawModernReferenceAircraft(frame_, aircraft, aircraftCount, config);
    frame_.pushSprite(0, 0);
}

void RadarRenderer::drawModernReferenceRadarFrame(TFT_eSprite &canvas, const AppConfig &config)
{
    canvas.fillSprite(modernBackgroundColor());

    drawModernReferenceGrid(canvas, config);
    drawModernReferenceCenterDot(canvas);
    drawModernReferenceCardinals(canvas);
    drawModernReferenceScaleLabel(canvas, config);
}

void RadarRenderer::drawModernReferenceGrid(TFT_eSprite &canvas, const AppConfig &config)
{
    (void)config;

    drawModernReferenceRings(canvas);
    drawModernReferenceCrosshairs(canvas);
}

void RadarRenderer::drawModernReferenceRings(TFT_eSprite &canvas)
{
    const ModernRadarTuning &tuning = modernTuning();
    const uint8_t ringCount = max<uint8_t>(tuning.ringCount, 1);
    for (uint8_t i = 1; i <= ringCount; ++i)
    {
        const int16_t radius = (static_cast<int16_t>(tuning.outerRadius) * i) / ringCount;
        drawModernWideCircle(canvas,
                             ModernRadarTheme::centerX,
                             ModernRadarTheme::centerY,
                             radius,
                             modernGridColor(),
                             tuning.lineWidth);
    }
}

void RadarRenderer::drawModernReferenceCrosshairs(TFT_eSprite &canvas)
{
    const ModernRadarTuning &tuning = modernTuning();
    drawModernWideLine(canvas,
                       ModernRadarTheme::centerX - tuning.outerRadius,
                       ModernRadarTheme::centerY,
                       ModernRadarTheme::centerX + tuning.outerRadius,
                       ModernRadarTheme::centerY,
                       modernGridColor(),
                       tuning.lineWidth);
    drawModernWideLine(canvas,
                       ModernRadarTheme::centerX,
                       ModernRadarTheme::centerY - tuning.outerRadius,
                       ModernRadarTheme::centerX,
                       ModernRadarTheme::centerY + tuning.outerRadius,
                       modernGridColor(),
                       tuning.lineWidth);
}

void RadarRenderer::drawModernReferenceCardinals(TFT_eSprite &canvas)
{
    canvas.setTextColor(modernTextColor(), modernBackgroundColor());
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("N", ModernRadarTheme::centerX, -1, 2);
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString("S", ModernRadarTheme::centerX, ModernRadarTheme::size + 3, 2);
    canvas.setTextDatum(ML_DATUM);
    canvas.drawString("W", 0, ModernRadarTheme::centerY, 2);
    canvas.setTextDatum(MR_DATUM);
    canvas.drawString("E", ModernRadarTheme::size - 1, ModernRadarTheme::centerY, 2);
}

void RadarRenderer::drawModernReferenceScaleLabel(TFT_eSprite &canvas, const AppConfig &config)
{
    const ModernRadarTuning &tuning = modernTuning();
    char label[16];
    snprintf(label, sizeof(label), "%.0fkm", max(config.maxRangeKm, 1.0f));

    canvas.setTextDatum(MR_DATUM);
    const int16_t textWidth = canvas.textWidth(label, 1);
    const int16_t labelX = ModernRadarTheme::centerX + tuning.outerRadius - 6;
    const int16_t labelY = ModernRadarTheme::centerY;
    canvas.fillRect(labelX - textWidth - 2, labelY - 5, textWidth + 4, 11, modernBackgroundColor());
    canvas.setTextColor(modernGridColor(), modernBackgroundColor());
    canvas.drawString(label, labelX, labelY, 1);
}

void RadarRenderer::drawModernReferenceCenterDot(TFT_eSprite &canvas)
{
    canvas.fillCircle(ModernRadarTheme::centerX,
                      ModernRadarTheme::centerY,
                      modernTuning().centerDotRadius,
                      modernCenterColor());
}

void RadarRenderer::drawModernReferenceAircraft(TFT_eSprite &canvas,
                                                const Aircraft *aircraft,
                                                uint8_t aircraftCount,
                                                const AppConfig &config)
{
    if (aircraft == nullptr)
    {
        return;
    }

    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        int16_t x = 0;
        int16_t y = 0;
        bool insideOuterRing = false;
        if (modernReferenceToScreen(aircraft[i], config, x, y, insideOuterRing) && !insideOuterRing)
        {
            drawModernReferenceBeyondDot(canvas, aircraft[i]);
        }
    }

    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        int16_t x = 0;
        int16_t y = 0;
        bool insideOuterRing = false;
        if (modernReferenceToScreen(aircraft[i], config, x, y, insideOuterRing) && insideOuterRing)
        {
            drawModernReferenceSpeedVector(canvas, aircraft[i], x, y);
        }
    }

    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        int16_t x = 0;
        int16_t y = 0;
        bool insideOuterRing = false;
        if (modernReferenceToScreen(aircraft[i], config, x, y, insideOuterRing) && insideOuterRing)
        {
            drawModernReferenceAircraftSymbol(canvas, aircraft[i], x, y);
            if (config.showLabels)
            {
                drawModernReferenceAircraftTag(canvas, aircraft[i], x, y);
            }
        }
    }
}

void RadarRenderer::drawModernReferenceAircraftSymbol(TFT_eSprite &canvas,
                                                      const Aircraft &target,
                                                      int16_t x,
                                                      int16_t y)
{
    const ModernRadarTuning &tuning = modernTuning();
    const float scale = tuning.aircraftScale;
    const float noseLength = ModernRadarTheme::noseLength * scale;
    const float tailLength = ModernRadarTheme::tailLength * scale;
    const float tailHalfWidth = ModernRadarTheme::tailHalfWidth * scale;
    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    const float heading = headingDeg * DEG_TO_RAD;
    const float normal = heading + HALF_PI;

    const int16_t noseX = x + static_cast<int16_t>(sinf(heading) * noseLength);
    const int16_t noseY = y - static_cast<int16_t>(cosf(heading) * noseLength);
    const int16_t tailCenterX = x - static_cast<int16_t>(sinf(heading) * tailLength);
    const int16_t tailCenterY = y + static_cast<int16_t>(cosf(heading) * tailLength);
    const int16_t tailLeftX = tailCenterX + static_cast<int16_t>(sinf(normal) * tailHalfWidth);
    const int16_t tailLeftY = tailCenterY - static_cast<int16_t>(cosf(normal) * tailHalfWidth);
    const int16_t tailRightX = tailCenterX - static_cast<int16_t>(sinf(normal) * tailHalfWidth);
    const int16_t tailRightY = tailCenterY + static_cast<int16_t>(cosf(normal) * tailHalfWidth);

    canvas.fillTriangle(noseX, noseY, tailLeftX, tailLeftY, tailRightX, tailRightY, modernAircraftColor());
}

void RadarRenderer::drawModernReferenceSpeedVector(TFT_eSprite &canvas,
                                                   const Aircraft &target,
                                                   int16_t x,
                                                   int16_t y)
{
    if (!isfinite(target.speedMs) || target.speedMs <= 0.0f)
    {
        return;
    }

    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    int16_t startX = 0;
    int16_t startY = 0;
    modernReferenceNoseTip(x, y, headingDeg, startX, startY);

    const float heading = headingDeg * DEG_TO_RAD;
    const int lineLength = modernReferenceSpeedLineLengthPx(target.speedMs);
    int16_t endX = startX + static_cast<int16_t>(sinf(heading) * lineLength);
    int16_t endY = startY - static_cast<int16_t>(cosf(heading) * lineLength);
    clipModernReferencePointToOuterRing(startX, startY, endX, endY);
    drawModernWideLine(canvas,
                       startX,
                       startY,
                       endX,
                       endY,
                       modernVectorColor(),
                       modernTuning().lineWidth);
}

void RadarRenderer::drawModernReferenceAircraftTag(TFT_eSprite &canvas,
                                                   const Aircraft &target,
                                                   int16_t x,
                                                   int16_t y)
{
    char callsign[16];
    char altitude[16];
    snprintf(callsign,
             sizeof(callsign),
             "%s",
             target.callsign[0] != '\0' ? target.callsign : "UNKNOWN");
    snprintf(altitude, sizeof(altitude), "%.0fm", target.altitudeM);

    const ModernRadarTuning &tuning = modernTuning();
    const bool labelRight = x < ModernRadarTheme::centerX;
    const int16_t symbolHalf = static_cast<int16_t>((ModernRadarTheme::noseLength +
                                                     ModernRadarTheme::tailHalfWidth) *
                                                    tuning.aircraftScale);
    const int16_t labelWidth = max(canvas.textWidth(callsign, 1), canvas.textWidth(altitude, 1));
    const int16_t blockHeight = 20;
    int16_t anchorX = labelRight ?
                      x + symbolHalf + tuning.labelGap :
                      x - symbolHalf - tuning.labelGap;
    int16_t labelY = constrain(y - blockHeight / 2, 2, ModernRadarTheme::size - blockHeight - 2);

    if (labelRight)
    {
        anchorX = constrain(anchorX, 2, ModernRadarTheme::size - labelWidth - 2);
        canvas.fillRect(anchorX - 1, labelY - 1, labelWidth + 2, blockHeight + 2, modernBackgroundColor());
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextColor(modernTextColor(), modernBackgroundColor());
        canvas.drawString(callsign, anchorX, labelY, 1);
        canvas.setTextColor(modernAltitudeColor(), modernBackgroundColor());
        canvas.drawString(altitude, anchorX, labelY + 10, 1);
        return;
    }

    anchorX = constrain(anchorX, labelWidth + 2, ModernRadarTheme::size - 2);
    canvas.fillRect(anchorX - labelWidth - 1, labelY - 1, labelWidth + 2, blockHeight + 2, modernBackgroundColor());
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(modernTextColor(), modernBackgroundColor());
    canvas.drawString(callsign, anchorX, labelY, 1);
    canvas.setTextColor(modernAltitudeColor(), modernBackgroundColor());
    canvas.drawString(altitude, anchorX, labelY + 10, 1);
}

void RadarRenderer::drawModernReferenceBeyondDot(TFT_eSprite &canvas, const Aircraft &target)
{
    if (!isfinite(target.bearingDeg))
    {
        return;
    }

    const float bearing = target.bearingDeg * DEG_TO_RAD;
    const float dx = sinf(bearing);
    const float dy = -cosf(bearing);
    const float maxX = static_cast<float>(ModernRadarTheme::size - 1 - ModernRadarTheme::beyondRingScreenMargin);
    const float minX = static_cast<float>(ModernRadarTheme::beyondRingScreenMargin);
    const float maxY = maxX;
    const float minY = minX;
    float edgeScale = 1000.0f;

    if (dx > 0.001f)
    {
        edgeScale = min(edgeScale, (maxX - ModernRadarTheme::centerX) / dx);
    }
    else if (dx < -0.001f)
    {
        edgeScale = min(edgeScale, (minX - ModernRadarTheme::centerX) / dx);
    }
    if (dy > 0.001f)
    {
        edgeScale = min(edgeScale, (maxY - ModernRadarTheme::centerY) / dy);
    }
    else if (dy < -0.001f)
    {
        edgeScale = min(edgeScale, (minY - ModernRadarTheme::centerY) / dy);
    }

    const int16_t x = ModernRadarTheme::centerX + static_cast<int16_t>(dx * edgeScale);
    const int16_t y = ModernRadarTheme::centerY + static_cast<int16_t>(dy * edgeScale);
    canvas.fillCircle(x, y, ModernRadarTheme::beyondRingDotRadius, modernAircraftColor());
}

bool RadarRenderer::modernReferenceToScreen(const Aircraft &target,
                                            const AppConfig &config,
                                            int16_t &x,
                                            int16_t &y,
                                            bool &insideOuterRing) const
{
    if (!target.valid || !isfinite(target.distanceKm) || !isfinite(target.bearingDeg))
    {
        return false;
    }

    const float maxRangeKm = max(config.maxRangeKm, 1.0f);
    const float rawRadius = (target.distanceKm / maxRangeKm) *
                            static_cast<float>(modernTuning().outerRadius);
    insideOuterRing = rawRadius <= static_cast<float>(modernTuning().outerRadius);

    const float insideInset = (ModernRadarTheme::noseLength + ModernRadarTheme::tailHalfWidth) *
                              modernTuning().aircraftScale + 1.0f;
    const float radius = constrain(rawRadius,
                                   0.0f,
                                   static_cast<float>(modernTuning().outerRadius) - insideInset);
    const float bearing = target.bearingDeg * DEG_TO_RAD;
    x = ModernRadarTheme::centerX + static_cast<int16_t>(sinf(bearing) * radius);
    y = ModernRadarTheme::centerY - static_cast<int16_t>(cosf(bearing) * radius);
    return true;
}

int RadarRenderer::modernReferenceSpeedLineLengthPx(float speedMs) const
{
    if (!isfinite(speedMs) || speedMs <= 0.0f)
    {
        return 0;
    }

    const float distanceKm = speedMs * static_cast<float>(ModernRadarTheme::trackHorizonSec) / 1000.0f;
    const float rawPx = (distanceKm / ModernRadarTheme::trackRefOuterKm) *
                        static_cast<float>(modernTuning().outerRadius);
    const float scaled = rawPx * ModernRadarTheme::trackLengthScale * modernTuning().vectorScale;
    return constrain(static_cast<int>(scaled),
                     ModernRadarTheme::speedLineMinPx,
                     static_cast<int>(modernTuning().outerRadius));
}

void RadarRenderer::modernReferenceNoseTip(int16_t x,
                                           int16_t y,
                                           float headingDeg,
                                           int16_t &tipX,
                                           int16_t &tipY) const
{
    const float heading = headingDeg * DEG_TO_RAD;
    const float noseLength = ModernRadarTheme::noseLength * modernTuning().aircraftScale;
    tipX = x + static_cast<int16_t>(sinf(heading) * noseLength);
    tipY = y - static_cast<int16_t>(cosf(heading) * noseLength);
}

void RadarRenderer::clipModernReferencePointToOuterRing(int16_t x0,
                                                        int16_t y0,
                                                        int16_t &x1,
                                                        int16_t &y1) const
{
    const float dx = static_cast<float>(x1 - x0);
    const float dy = static_cast<float>(y1 - y0);
    const float endDx = static_cast<float>(x1 - ModernRadarTheme::centerX);
    const float endDy = static_cast<float>(y1 - ModernRadarTheme::centerY);
    const int16_t outerRadius = modernTuning().outerRadius;
    const float limit = static_cast<float>((outerRadius - 1) * (outerRadius - 1));

    if (endDx * endDx + endDy * endDy <= limit)
    {
        return;
    }

    float low = 0.0f;
    float high = 1.0f;
    for (uint8_t i = 0; i < 8; ++i)
    {
        const float mid = (low + high) * 0.5f;
        const float testX = static_cast<float>(x0) + dx * mid - ModernRadarTheme::centerX;
        const float testY = static_cast<float>(y0) + dy * mid - ModernRadarTheme::centerY;
        if (testX * testX + testY * testY <= limit)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }

    x1 = x0 + static_cast<int16_t>(dx * low);
    y1 = y0 + static_cast<int16_t>(dy * low);
}

void RadarRenderer::renderCyberpunkRadarFrame(const Aircraft *aircraft,
                                              uint8_t aircraftCount,
                                              uint8_t selectedAircraftIndex,
                                              const AppConfig &config,
                                              const char *statusText)
{
    drawCyberpunkBackground(frame_);
    drawCyberpunkRings(frame_);
    drawCyberpunkOuterTicks(frame_);
    drawCyberpunkCrosshair(frame_);
    drawCyberpunkSweep(frame_);
    drawCyberpunkCenter(frame_);
    drawCyberpunkCardinals(frame_);
    drawCyberpunkAircraftTargets(frame_, aircraft, aircraftCount, selectedAircraftIndex, config);
    drawCyberpunkStatusText(frame_, statusText, aircraftCount, config);
    frame_.pushSprite(0, 0);
}

void RadarRenderer::drawCyberpunkBackground(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    canvas.fillSprite(cyberBackgroundColor());

    const int32_t radiusSq = static_cast<int32_t>(tuning.outerRadius) *
                             static_cast<int32_t>(tuning.outerRadius);
    const uint16_t noise = cyberNoiseColor();
    for (uint8_t i = 0; i < 64; ++i)
    {
        const int16_t x = static_cast<int16_t>((i * 47U + 31U) % CyberpunkRadarTheme::size);
        const int16_t y = static_cast<int16_t>((i * 73U + 19U) % CyberpunkRadarTheme::size);
        const int16_t dx = x - CyberpunkRadarTheme::centerX;
        const int16_t dy = y - CyberpunkRadarTheme::centerY;
        if (static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy <= radiusSq)
        {
            canvas.drawPixel(x, y, noise);
        }
    }
}

void RadarRenderer::drawCyberpunkRings(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const uint8_t ringCount = max<uint8_t>(tuning.ringCount, 1);
    const uint8_t width = max<uint8_t>(tuning.lineWidth, 1);

    drawModernWideCircle(canvas,
                         CyberpunkRadarTheme::centerX,
                         CyberpunkRadarTheme::centerY,
                         tuning.outerRadius,
                         cyberOuterRingColor(),
                         width);
    canvas.drawCircle(CyberpunkRadarTheme::centerX,
                      CyberpunkRadarTheme::centerY,
                      tuning.outerRadius - 3,
                      cyberRingDimColor());

    for (uint8_t i = 1; i <= ringCount; ++i)
    {
        const int16_t radius = (static_cast<int16_t>(tuning.innerRadarRadius) * i) / ringCount;
        const uint16_t color = i == ringCount ? cyberRingColor() : cyberRingDimColor();
        drawModernWideCircle(canvas,
                             CyberpunkRadarTheme::centerX,
                             CyberpunkRadarTheme::centerY,
                             radius,
                             color,
                             width);
    }
}

void RadarRenderer::drawCyberpunkOuterTicks(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    for (uint16_t deg = 0; deg < 360; deg += 10)
    {
        const bool major = deg % 30 == 0;
        const float radians = static_cast<float>(deg) * DEG_TO_RAD;
        const int16_t tickLength = major ? tuning.majorTickLength : tuning.minorTickLength;
        const int16_t outer = tuning.outerRadius;
        const int16_t inner = outer - tickLength;
        const int16_t x0 = CyberpunkRadarTheme::centerX + static_cast<int16_t>(sinf(radians) * outer);
        const int16_t y0 = CyberpunkRadarTheme::centerY - static_cast<int16_t>(cosf(radians) * outer);
        const int16_t x1 = CyberpunkRadarTheme::centerX + static_cast<int16_t>(sinf(radians) * inner);
        const int16_t y1 = CyberpunkRadarTheme::centerY - static_cast<int16_t>(cosf(radians) * inner);
        canvas.drawLine(x0, y0, x1, y1, major ? cyberTickColor() : cyberRingDimColor());
    }
}

void RadarRenderer::drawCyberpunkCardinals(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const int16_t radius = tuning.outerRadius - 16;

    canvas.setTextColor(cyberTextColor(), cyberBackgroundColor());
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("N", CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY - radius, 2);
    canvas.drawString("S", CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY + radius, 2);
    canvas.drawString("E", CyberpunkRadarTheme::centerX + radius, CyberpunkRadarTheme::centerY, 2);
    canvas.drawString("W", CyberpunkRadarTheme::centerX - radius, CyberpunkRadarTheme::centerY, 2);

    canvas.drawTriangle(CyberpunkRadarTheme::centerX,
                        CyberpunkRadarTheme::centerY - tuning.outerRadius + 4,
                        CyberpunkRadarTheme::centerX - 4,
                        CyberpunkRadarTheme::centerY - tuning.outerRadius + 12,
                        CyberpunkRadarTheme::centerX + 4,
                        CyberpunkRadarTheme::centerY - tuning.outerRadius + 12,
                        cyberMagentaColor());
}

void RadarRenderer::drawCyberpunkCrosshair(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const int16_t radius = tuning.innerRadarRadius;
    const uint16_t dim = cyberRingDimColor();

    canvas.drawLine(CyberpunkRadarTheme::centerX - radius,
                    CyberpunkRadarTheme::centerY,
                    CyberpunkRadarTheme::centerX + radius,
                    CyberpunkRadarTheme::centerY,
                    cyberCrosshairColor());
    canvas.drawLine(CyberpunkRadarTheme::centerX,
                    CyberpunkRadarTheme::centerY - radius,
                    CyberpunkRadarTheme::centerX,
                    CyberpunkRadarTheme::centerY + radius,
                    cyberCrosshairColor());

    for (uint16_t deg = 45; deg < 360; deg += 90)
    {
        const float radians = static_cast<float>(deg) * DEG_TO_RAD;
        const int16_t x = static_cast<int16_t>(sinf(radians) * radius);
        const int16_t y = static_cast<int16_t>(cosf(radians) * radius);
        canvas.drawLine(CyberpunkRadarTheme::centerX - x,
                        CyberpunkRadarTheme::centerY + y,
                        CyberpunkRadarTheme::centerX + x,
                        CyberpunkRadarTheme::centerY - y,
                        dim);
    }
}

void RadarRenderer::drawCyberpunkSweep(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const uint8_t trailCount = 10;
    const float trailStepDeg = 5.0f;

    for (uint8_t i = trailCount; i > 0; --i)
    {
        const float age = static_cast<float>(i) / trailCount;
        const float angle = AircraftModel::wrapDegrees(sweepAngleDeg_ - i * trailStepDeg);
        const float radians = angle * DEG_TO_RAD;
        const int16_t endX = CyberpunkRadarTheme::centerX +
                             static_cast<int16_t>(sinf(radians) * tuning.innerRadarRadius);
        const int16_t endY = CyberpunkRadarTheme::centerY -
                             static_cast<int16_t>(cosf(radians) * tuning.innerRadarRadius);
        const float local = tuning.sweepBrightness *
                            tuning.sweepTrailStrength *
                            (1.0f - age) *
                            (1.0f - age);
        const uint16_t color = color565Scaled(tuning.sweep, tuning.globalBrightness, local);
        canvas.drawLine(CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY, endX, endY, color);
    }

    const int8_t halfWidth = static_cast<int8_t>(max(1, static_cast<int>(roundf(tuning.sweepWidth))));
    for (int8_t offset = -halfWidth; offset <= halfWidth; ++offset)
    {
        const float angle = AircraftModel::wrapDegrees(sweepAngleDeg_ + offset * 0.75f);
        const float radians = angle * DEG_TO_RAD;
        const int16_t endX = CyberpunkRadarTheme::centerX +
                             static_cast<int16_t>(sinf(radians) * tuning.innerRadarRadius);
        const int16_t endY = CyberpunkRadarTheme::centerY -
                             static_cast<int16_t>(cosf(radians) * tuning.innerRadarRadius);
        const float local = tuning.sweepBrightness * (offset == 0 ? 1.0f : 0.45f);
        const uint16_t color = color565Scaled(tuning.sweep, tuning.globalBrightness, local);
        canvas.drawLine(CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY, endX, endY, color);
    }
}

void RadarRenderer::drawCyberpunkCenter(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    canvas.drawCircle(CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY, 7, cyberMagentaColor());
    canvas.fillCircle(CyberpunkRadarTheme::centerX,
                      CyberpunkRadarTheme::centerY,
                      tuning.centerDotRadius,
                      cyberSelectedColor());
}

void RadarRenderer::drawCyberpunkAircraftTargets(TFT_eSprite &canvas,
                                                 const Aircraft *aircraft,
                                                 uint8_t aircraftCount,
                                                 uint8_t selectedAircraftIndex,
                                                 const AppConfig &config)
{
    if (aircraft == nullptr)
    {
        return;
    }

    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        int16_t x = 0;
        int16_t y = 0;
        bool inside = false;
        if (cyberpunkToScreen(aircraft[i], config, x, y, inside) && inside)
        {
            drawCyberpunkSpeedVector(canvas, aircraft[i], x, y);
        }
    }

    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        int16_t x = 0;
        int16_t y = 0;
        bool inside = false;
        if (!cyberpunkToScreen(aircraft[i], config, x, y, inside) || !inside)
        {
            continue;
        }

        const bool selected = i == selectedAircraftIndex;
        drawCyberpunkAircraftSymbol(canvas, aircraft[i], x, y, selected);
        if (config.showLabels && (selected || aircraftCount <= 6 || (i % 3) == 0))
        {
            drawCyberpunkAircraftLabel(canvas, aircraft[i], x, y, selected);
        }
    }
}

void RadarRenderer::drawCyberpunkAircraftSymbol(TFT_eSprite &canvas,
                                                const Aircraft &target,
                                                int16_t x,
                                                int16_t y,
                                                bool selected)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const float scale = tuning.aircraftScale;
    const float noseLength = CyberpunkRadarTheme::aircraftNoseLength * scale;
    const float tailLength = CyberpunkRadarTheme::aircraftTailLength * scale;
    const float tailHalfWidth = CyberpunkRadarTheme::aircraftTailHalfWidth * scale;
    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    const float heading = headingDeg * DEG_TO_RAD;
    const float normal = heading + HALF_PI;

    const int16_t noseX = x + static_cast<int16_t>(sinf(heading) * noseLength);
    const int16_t noseY = y - static_cast<int16_t>(cosf(heading) * noseLength);
    const int16_t tailCenterX = x - static_cast<int16_t>(sinf(heading) * tailLength);
    const int16_t tailCenterY = y + static_cast<int16_t>(cosf(heading) * tailLength);
    const int16_t tailLeftX = tailCenterX + static_cast<int16_t>(sinf(normal) * tailHalfWidth);
    const int16_t tailLeftY = tailCenterY - static_cast<int16_t>(cosf(normal) * tailHalfWidth);
    const int16_t tailRightX = tailCenterX - static_cast<int16_t>(sinf(normal) * tailHalfWidth);
    const int16_t tailRightY = tailCenterY + static_cast<int16_t>(cosf(normal) * tailHalfWidth);

    canvas.drawCircle(x, y, selected ? 7 : 4, selected ? cyberSelectedColor() : cyberAircraftGlowColor());
    canvas.fillTriangle(noseX, noseY, tailLeftX, tailLeftY, tailRightX, tailRightY, cyberAircraftColor());

    if (selected)
    {
        canvas.drawCircle(x, y, 10, cyberMagentaColor());
    }
}

void RadarRenderer::drawCyberpunkSpeedVector(TFT_eSprite &canvas,
                                             const Aircraft &target,
                                             int16_t x,
                                             int16_t y)
{
    if (!isfinite(target.speedMs) || target.speedMs <= 0.0f)
    {
        return;
    }

    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    const float heading = headingDeg * DEG_TO_RAD;
    const float noseLength = CyberpunkRadarTheme::aircraftNoseLength * cyberpunkTuning().aircraftScale;
    const int16_t startX = x + static_cast<int16_t>(sinf(heading) * noseLength);
    const int16_t startY = y - static_cast<int16_t>(cosf(heading) * noseLength);
    const int lineLength = cyberpunkSpeedVectorLengthPx(target.speedMs);
    int16_t endX = startX + static_cast<int16_t>(sinf(heading) * lineLength);
    int16_t endY = startY - static_cast<int16_t>(cosf(heading) * lineLength);
    clipCyberpunkPointToInnerRadar(startX, startY, endX, endY);
    drawModernWideLine(canvas, startX, startY, endX, endY, cyberMagentaColor(), cyberpunkTuning().lineWidth);
}

void RadarRenderer::drawCyberpunkAircraftLabel(TFT_eSprite &canvas,
                                               const Aircraft &target,
                                               int16_t x,
                                               int16_t y,
                                               bool selected)
{
    char callsign[16];
    char altitude[16];
    snprintf(callsign, sizeof(callsign), "%s", target.callsign[0] != '\0' ? target.callsign : "UNKNOWN");
    snprintf(altitude, sizeof(altitude), "%.0fm", target.altitudeM);

    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const bool labelRight = x < CyberpunkRadarTheme::centerX;
    const int16_t symbolHalf = static_cast<int16_t>((CyberpunkRadarTheme::aircraftNoseLength +
                                                     CyberpunkRadarTheme::aircraftTailHalfWidth) *
                                                    tuning.aircraftScale);
    const int16_t labelWidth = selected ?
                               max(canvas.textWidth(callsign, 1), canvas.textWidth(altitude, 1)) :
                               canvas.textWidth(callsign, 1);
    const int16_t blockHeight = selected ? 19 : 9;
    int16_t anchorX = labelRight ?
                      x + symbolHalf + tuning.labelGap + 2 :
                      x - symbolHalf - tuning.labelGap - 2;
    int16_t labelY = constrain(y - blockHeight / 2, 14, CyberpunkRadarTheme::size - blockHeight - 14);

    canvas.drawLine(x,
                    y,
                    labelRight ? anchorX - 2 : anchorX + 2,
                    labelY + 4,
                    selected ? cyberMagentaColor() : cyberRingDimColor());

    if (labelRight)
    {
        anchorX = constrain(anchorX, 10, CyberpunkRadarTheme::size - labelWidth - 12);
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextColor(selected ? cyberSelectedColor() : cyberTextColor(), cyberBackgroundColor());
        canvas.drawString(callsign, anchorX, labelY, 1);
        if (selected)
        {
            canvas.setTextColor(cyberAltitudeColor(), cyberBackgroundColor());
            canvas.drawString(altitude, anchorX, labelY + 9, 1);
        }
        return;
    }

    anchorX = constrain(anchorX, labelWidth + 12, CyberpunkRadarTheme::size - 10);
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(selected ? cyberSelectedColor() : cyberTextColor(), cyberBackgroundColor());
    canvas.drawString(callsign, anchorX, labelY, 1);
    if (selected)
    {
        canvas.setTextColor(cyberAltitudeColor(), cyberBackgroundColor());
        canvas.drawString(altitude, anchorX, labelY + 9, 1);
    }
}

void RadarRenderer::drawCyberpunkStatusText(TFT_eSprite &canvas,
                                            const char *statusText,
                                            uint8_t aircraftCount,
                                            const AppConfig &config)
{
    char status[32];
    if (statusText != nullptr && statusText[0] != '\0')
    {
        snprintf(status, sizeof(status), "%s", statusText);
    }
    else
    {
        snprintf(status, sizeof(status), "CYBER N=%u", aircraftCount);
    }

    char range[16];
    snprintf(range, sizeof(range), "%.0fkm", max(config.maxRangeKm, 1.0f));

    canvas.setTextDatum(TC_DATUM);
    canvas.setTextColor(cyberTextColor(), cyberBackgroundColor());
    canvas.drawString("CYBER", CyberpunkRadarTheme::centerX, 18, 1);
    canvas.setTextDatum(BC_DATUM);
    canvas.setTextColor(cyberAltitudeColor(), cyberBackgroundColor());
    canvas.drawString(status, CyberpunkRadarTheme::centerX, 226, 1);
    canvas.setTextDatum(MR_DATUM);
    canvas.drawString(range, CyberpunkRadarTheme::centerX + cyberpunkTuning().outerRadius - 8, CyberpunkRadarTheme::centerY + 12, 1);
}

bool RadarRenderer::cyberpunkToScreen(const Aircraft &target,
                                      const AppConfig &config,
                                      int16_t &x,
                                      int16_t &y,
                                      bool &insideRadar) const
{
    if (!target.valid || !isfinite(target.distanceKm) || !isfinite(target.bearingDeg))
    {
        return false;
    }

    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const float maxRangeKm = max(config.maxRangeKm, 1.0f);
    const float rawRadius = (target.distanceKm / maxRangeKm) * static_cast<float>(tuning.innerRadarRadius);
    insideRadar = rawRadius <= static_cast<float>(tuning.innerRadarRadius);
    const float radius = constrain(rawRadius, 0.0f, static_cast<float>(tuning.innerRadarRadius) - 8.0f);
    const float bearing = target.bearingDeg * DEG_TO_RAD;
    x = CyberpunkRadarTheme::centerX + static_cast<int16_t>(sinf(bearing) * radius);
    y = CyberpunkRadarTheme::centerY - static_cast<int16_t>(cosf(bearing) * radius);
    return true;
}

int RadarRenderer::cyberpunkSpeedVectorLengthPx(float speedMs) const
{
    if (!isfinite(speedMs) || speedMs <= 0.0f)
    {
        return 0;
    }

    const float distanceKm = speedMs * static_cast<float>(CyberpunkRadarTheme::trackHorizonSec) / 1000.0f;
    const float rawPx = distanceKm * 2.0f * cyberpunkTuning().vectorScale;
    return constrain(static_cast<int>(rawPx),
                     CyberpunkRadarTheme::speedLineMinPx,
                     CyberpunkRadarTheme::speedLineMaxPx);
}

void RadarRenderer::clipCyberpunkPointToInnerRadar(int16_t x0,
                                                   int16_t y0,
                                                   int16_t &x1,
                                                   int16_t &y1) const
{
    const float dx = static_cast<float>(x1 - x0);
    const float dy = static_cast<float>(y1 - y0);
    const float endDx = static_cast<float>(x1 - CyberpunkRadarTheme::centerX);
    const float endDy = static_cast<float>(y1 - CyberpunkRadarTheme::centerY);
    const int16_t radius = cyberpunkTuning().innerRadarRadius - 2;
    const float limit = static_cast<float>(radius * radius);

    if (endDx * endDx + endDy * endDy <= limit)
    {
        return;
    }

    float low = 0.0f;
    float high = 1.0f;
    for (uint8_t i = 0; i < 8; ++i)
    {
        const float mid = (low + high) * 0.5f;
        const float testX = static_cast<float>(x0) + dx * mid - CyberpunkRadarTheme::centerX;
        const float testY = static_cast<float>(y0) + dy * mid - CyberpunkRadarTheme::centerY;
        if (testX * testX + testY * testY <= limit)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }

    x1 = x0 + static_cast<int16_t>(dx * low);
    y1 = y0 + static_cast<int16_t>(dy * low);
}

void RadarRenderer::advanceSweep(float stepDeg)
{
    sweepAngleDeg_ = AircraftModel::wrapDegrees(sweepAngleDeg_ + stepDeg);
}

void RadarRenderer::renderSetupPortalFrame(const char *apSsid,
                                           const char *apPassword,
                                           const char *ipAddress,
                                           const char *statusText,
                                           SetupDisplayMode mode)
{
    renderSettingsFrame(apSsid,
                        apPassword,
                        ipAddress,
                        nullptr,
                        statusText,
                        mode == SetupDisplayMode::QrCode ? SettingsDisplayMode::ApQr : SettingsDisplayMode::ApDetails);
}

void RadarRenderer::renderSettingsFrame(const char *apSsid,
                                        const char *apPassword,
                                        const char *apIpAddress,
                                        const char *staIpAddress,
                                        const char *statusText,
                                        SettingsDisplayMode mode)
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
    frame_.drawCircle(kCenterX, kCenterY, 116, radarGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 78, dimGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 40, dimGreen_);

    if (mode == SettingsDisplayMode::ApQr)
    {
        frame_.setTextDatum(MC_DATUM);
        frame_.setTextColor(sweepGreen_, TFT_BLACK);
        frame_.drawString("SETUP", kCenterX, 28, 2);

        const bool qrOk = QrCodeRenderer::drawWifiQr(frame_,
                                                     apSsid,
                                                     apPassword,
                                                     kCenterX,
                                                     46,
                                                     152);

        frame_.setTextDatum(MC_DATUM);
        frame_.setTextColor(labelGreen_, TFT_BLACK);
        if (qrOk)
        {
            frame_.drawString(statusText != nullptr && statusText[0] != '\0' ? statusText : "Scan to connect",
                              kCenterX,
                              204,
                              1);
            frame_.drawString(apIpAddress != nullptr ? apIpAddress : "192.168.4.1", kCenterX, 218, 1);
        }
        else
        {
            frame_.drawString("QR payload too long", kCenterX, 110, 1);
            frame_.drawString("Press q for details", kCenterX, 126, 1);
        }

        frame_.pushSprite(0, 0);
        return;
    }

    if (mode == SettingsDisplayMode::StaQr)
    {
        char url[40];
        snprintf(url, sizeof(url), "http://%s/", staIpAddress != nullptr ? staIpAddress : "0.0.0.0");

        frame_.setTextDatum(MC_DATUM);
        frame_.setTextColor(sweepGreen_, TFT_BLACK);
        frame_.drawString("SETTINGS", kCenterX, 28, 2);

        const bool qrOk = QrCodeRenderer::drawTextQr(frame_, url, kCenterX, 46, 152);
        frame_.setTextColor(labelGreen_, TFT_BLACK);
        frame_.drawString(qrOk ? "Scan to open" : "QR payload too long", kCenterX, 204, 1);
        frame_.drawString(url, kCenterX, 218, 1);
        frame_.pushSprite(0, 0);
        return;
    }

    if (mode == SettingsDisplayMode::StaDetails)
    {
        char url[40];
        snprintf(url, sizeof(url), "http://%s/", staIpAddress != nullptr ? staIpAddress : "0.0.0.0");

        frame_.setTextDatum(MC_DATUM);
        frame_.setTextColor(sweepGreen_, TFT_BLACK);
        frame_.drawString("SETTINGS", kCenterX, 74, 2);
        frame_.setTextColor(labelGreen_, TFT_BLACK);
        frame_.drawString("WiFi connected", kCenterX, 106, 1);
        frame_.drawString("IP", kCenterX, 130, 1);
        frame_.setTextColor(selectedGreen_, TFT_BLACK);
        frame_.drawString(staIpAddress != nullptr ? staIpAddress : "", kCenterX, 144, 1);
        frame_.setTextColor(labelGreen_, TFT_BLACK);
        frame_.drawString("URL", kCenterX, 166, 1);
        frame_.setTextColor(selectedGreen_, TFT_BLACK);
        frame_.drawString(url, kCenterX, 180, 1);
        frame_.pushSprite(0, 0);
        return;
    }

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
    frame_.drawString(apIpAddress != nullptr ? apIpAddress : "192.168.4.1", kCenterX, 176, 1);

    if (statusText != nullptr && statusText[0] != '\0')
    {
        frame_.setTextColor(labelGreen_, TFT_BLACK);
        frame_.drawString(statusText, kCenterX, 204, 1);
    }

    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderSystemStatusFrame(const char *line1,
                                            const char *line2,
                                            const char *line3)
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
    frame_.drawCircle(kCenterX, kCenterY, 116, radarGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 78, dimGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 40, dimGreen_);
    frame_.drawLine(kCenterX - 80, kCenterY, kCenterX + 80, kCenterY, dimGreen_);
    frame_.drawLine(kCenterX, kCenterY - 80, kCenterX, kCenterY + 80, dimGreen_);

    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(sweepGreen_, TFT_BLACK);
    frame_.drawString(line1 != nullptr ? line1 : "STATUS", kCenterX, 96, 2);

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    if (line2 != nullptr && line2[0] != '\0')
    {
        frame_.drawString(line2, kCenterX, 124, 1);
    }
    if (line3 != nullptr && line3[0] != '\0')
    {
        frame_.drawString(line3, kCenterX, 140, 1);
    }

    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderClockFrame(const char *timeText,
                                     const char *dateText,
                                     const char *nextRunText,
                                     const char *hintText)
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
    frame_.drawCircle(kCenterX, kCenterY, 116, dimGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 78, dimGreen_);

    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString("IDLE", kCenterX, 52, 1);

    frame_.setTextColor(sweepGreen_, TFT_BLACK);
    frame_.drawString(timeText != nullptr ? timeText : "--:--", kCenterX, 104, 4);

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    if (dateText != nullptr && dateText[0] != '\0')
    {
        frame_.drawString(dateText, kCenterX, 138, 1);
    }
    if (nextRunText != nullptr && nextRunText[0] != '\0')
    {
        frame_.drawString(nextRunText, kCenterX, 164, 1);
    }
    if (hintText != nullptr && hintText[0] != '\0')
    {
        frame_.drawString(hintText, kCenterX, 190, 1);
    }

    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderBlankFrame()
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
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
    modernBg_ = tft_.color565(0, 0, 0);
    modernGrid_ = tft_.color565(16, 100, 32);
    modernText_ = tft_.color565(255, 255, 255);
    modernCenter_ = tft_.color565(255, 255, 255);
    modernAircraft_ = tft_.color565(255, 0, 0);
    modernVector_ = tft_.color565(255, 0, 255);
    modernTagType_ = tft_.color565(255, 200, 0);
    modernTagAlt_ = tft_.color565(90, 200, 255);

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
