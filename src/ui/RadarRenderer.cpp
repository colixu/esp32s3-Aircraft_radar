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

    void drawModernWideLine(TFT_eSprite &canvas,
                            int16_t x0,
                            int16_t y0,
                            int16_t x1,
                            int16_t y1,
                            uint16_t color)
    {
        canvas.drawLine(x0, y0, x1, y1, color);
        if (abs(x1 - x0) >= abs(y1 - y0))
        {
            canvas.drawLine(x0, y0 + 1, x1, y1 + 1, color);
        }
        else
        {
            canvas.drawLine(x0 + 1, y0, x1 + 1, y1, color);
        }
    }

    void drawModernWideCircle(TFT_eSprite &canvas,
                              int16_t x,
                              int16_t y,
                              int16_t radius,
                              uint16_t color)
    {
        canvas.drawCircle(x, y, radius, color);
        canvas.drawCircle(x, y, radius - 1, color);
    }
}

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
            renderModernRadarFrame(aircraft, aircraftCount, selectedAircraftIndex, config, statusText);
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
    canvas.fillSprite(modernBg_);

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
    for (uint8_t i = 1; i <= ModernRadarTheme::ringCount; ++i)
    {
        const int16_t radius = (ModernRadarTheme::outerRadius * i) / ModernRadarTheme::ringCount;
        drawModernWideCircle(canvas,
                             ModernRadarTheme::centerX,
                             ModernRadarTheme::centerY,
                             radius,
                             modernGrid_);
    }
}

void RadarRenderer::drawModernReferenceCrosshairs(TFT_eSprite &canvas)
{
    drawModernWideLine(canvas,
                       ModernRadarTheme::centerX - ModernRadarTheme::outerRadius,
                       ModernRadarTheme::centerY,
                       ModernRadarTheme::centerX + ModernRadarTheme::outerRadius,
                       ModernRadarTheme::centerY,
                       modernGrid_);
    drawModernWideLine(canvas,
                       ModernRadarTheme::centerX,
                       ModernRadarTheme::centerY - ModernRadarTheme::outerRadius,
                       ModernRadarTheme::centerX,
                       ModernRadarTheme::centerY + ModernRadarTheme::outerRadius,
                       modernGrid_);
}

void RadarRenderer::drawModernReferenceCardinals(TFT_eSprite &canvas)
{
    canvas.setTextColor(modernText_, modernBg_);
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
    char label[16];
    snprintf(label, sizeof(label), "%.0fkm", max(config.maxRangeKm, 1.0f));

    canvas.setTextDatum(MR_DATUM);
    const int16_t textWidth = canvas.textWidth(label, 1);
    const int16_t labelX = ModernRadarTheme::centerX + ModernRadarTheme::outerRadius - 6;
    const int16_t labelY = ModernRadarTheme::centerY;
    canvas.fillRect(labelX - textWidth - 2, labelY - 5, textWidth + 4, 11, modernBg_);
    canvas.setTextColor(modernGrid_, modernBg_);
    canvas.drawString(label, labelX, labelY, 1);
}

void RadarRenderer::drawModernReferenceCenterDot(TFT_eSprite &canvas)
{
    canvas.fillCircle(ModernRadarTheme::centerX,
                      ModernRadarTheme::centerY,
                      ModernRadarTheme::centerDotRadius,
                      modernCenter_);
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
    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    const float heading = headingDeg * DEG_TO_RAD;
    const float normal = heading + HALF_PI;

    const int16_t noseX = x + static_cast<int16_t>(sinf(heading) * ModernRadarTheme::noseLength);
    const int16_t noseY = y - static_cast<int16_t>(cosf(heading) * ModernRadarTheme::noseLength);
    const int16_t tailCenterX = x - static_cast<int16_t>(sinf(heading) * ModernRadarTheme::tailLength);
    const int16_t tailCenterY = y + static_cast<int16_t>(cosf(heading) * ModernRadarTheme::tailLength);
    const int16_t tailLeftX = tailCenterX + static_cast<int16_t>(sinf(normal) * ModernRadarTheme::tailHalfWidth);
    const int16_t tailLeftY = tailCenterY - static_cast<int16_t>(cosf(normal) * ModernRadarTheme::tailHalfWidth);
    const int16_t tailRightX = tailCenterX - static_cast<int16_t>(sinf(normal) * ModernRadarTheme::tailHalfWidth);
    const int16_t tailRightY = tailCenterY + static_cast<int16_t>(cosf(normal) * ModernRadarTheme::tailHalfWidth);

    canvas.fillTriangle(noseX, noseY, tailLeftX, tailLeftY, tailRightX, tailRightY, modernAircraft_);
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
    drawModernWideLine(canvas, startX, startY, endX, endY, modernVector_);
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

    const bool labelRight = x < ModernRadarTheme::centerX;
    const int16_t symbolHalf = ModernRadarTheme::noseLength + ModernRadarTheme::tailHalfWidth;
    const int16_t labelWidth = max(canvas.textWidth(callsign, 1), canvas.textWidth(altitude, 1));
    const int16_t blockHeight = 20;
    int16_t anchorX = labelRight ?
                      x + symbolHalf + ModernRadarTheme::aircraftLabelGap :
                      x - symbolHalf - ModernRadarTheme::aircraftLabelGap;
    int16_t labelY = constrain(y - blockHeight / 2, 2, ModernRadarTheme::size - blockHeight - 2);

    if (labelRight)
    {
        anchorX = constrain(anchorX, 2, ModernRadarTheme::size - labelWidth - 2);
        canvas.fillRect(anchorX - 1, labelY - 1, labelWidth + 2, blockHeight + 2, modernBg_);
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextColor(modernText_, modernBg_);
        canvas.drawString(callsign, anchorX, labelY, 1);
        canvas.setTextColor(modernTagAlt_, modernBg_);
        canvas.drawString(altitude, anchorX, labelY + 10, 1);
        return;
    }

    anchorX = constrain(anchorX, labelWidth + 2, ModernRadarTheme::size - 2);
    canvas.fillRect(anchorX - labelWidth - 1, labelY - 1, labelWidth + 2, blockHeight + 2, modernBg_);
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(modernText_, modernBg_);
    canvas.drawString(callsign, anchorX, labelY, 1);
    canvas.setTextColor(modernTagAlt_, modernBg_);
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
    canvas.fillCircle(x, y, ModernRadarTheme::beyondRingDotRadius, modernAircraft_);
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
                            static_cast<float>(ModernRadarTheme::outerRadius);
    insideOuterRing = rawRadius <= static_cast<float>(ModernRadarTheme::outerRadius);

    const float radius = constrain(rawRadius,
                                   0.0f,
                                   static_cast<float>(ModernRadarTheme::outerRadius -
                                                      ModernRadarTheme::insideRingInset));
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
                        static_cast<float>(ModernRadarTheme::outerRadius);
    const float scaled = rawPx * ModernRadarTheme::trackLengthScale;
    return constrain(static_cast<int>(scaled),
                     ModernRadarTheme::speedLineMinPx,
                     ModernRadarTheme::outerRadius);
}

void RadarRenderer::modernReferenceNoseTip(int16_t x,
                                           int16_t y,
                                           float headingDeg,
                                           int16_t &tipX,
                                           int16_t &tipY) const
{
    const float heading = headingDeg * DEG_TO_RAD;
    tipX = x + static_cast<int16_t>(sinf(heading) * ModernRadarTheme::noseLength);
    tipY = y - static_cast<int16_t>(cosf(heading) * ModernRadarTheme::noseLength);
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
    const float limit = static_cast<float>((ModernRadarTheme::outerRadius - 1) *
                                           (ModernRadarTheme::outerRadius - 1));

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
