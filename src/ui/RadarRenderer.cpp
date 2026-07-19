#include "RadarRenderer.h"

#include <math.h>
#include <string.h>

#include "../app/DebugLog.h"
#include "QrCodeRenderer.h"

#ifndef USE_CYBERPUNK_STATIC_BG
#define USE_CYBERPUNK_STATIC_BG 1
#endif

#if USE_CYBERPUNK_STATIC_BG
#include "../data/UI/cyberpunk_bg_240_rgb565.h"
#endif

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

    // Inspired by MatixYo/ESP32-Plane-Radar:
    // https://github.com/MatixYo/ESP32-Plane-Radar
    struct PlaneRadarTheme
    {
        static constexpr int16_t size = 240;
        static constexpr int16_t centerX = 120;
        static constexpr int16_t centerY = 120;
        static constexpr int16_t outerRadius = 107;
        static constexpr int16_t cardinalNorthOffsetY = -1;
        static constexpr int16_t cardinalSouthOffsetY = 3;
        static constexpr int16_t scaleGapFromOuterRing = 6;
        static constexpr uint8_t ringCount = 4;
        static constexpr float gridStrokeHalfWidth = 1.0f;
        static constexpr int16_t centerDotRadius = 2;
        static constexpr int16_t aircraftNoseLength = 8;
        static constexpr int16_t aircraftTailLength = 3;
        static constexpr int16_t aircraftTailHalfWidth = 4;
        static constexpr int16_t aircraftLabelGap = 1;
        static constexpr int16_t aircraftInsideRingInset = aircraftNoseLength + aircraftTailHalfWidth + 1;
        static constexpr int16_t beyondRingDotRadius = 4;
        static constexpr int16_t beyondRingScreenMargin = 2;
        static constexpr uint32_t trackHorizonSec = 60;
        static constexpr int16_t speedLineMinPx = 2;
        static constexpr float trackRefOuterKm = 13.3f;
        static constexpr float trackLengthScale = 1.5f / 5.0f;
        static constexpr int16_t aircraftTagLabelHeightPx = 13;
        static constexpr uint8_t maxDrawItems = 16;
    };

    constexpr int16_t kCyberMapWestCoast[][2] =
    {
        {47, 64}, {35, 75}, {42, 88}, {34, 102}, {49, 113}, {42, 128},
        {57, 141}, {50, 156}, {66, 170}, {62, 184}, {78, 190},
        {93, 180}, {86, 164}, {97, 149}, {84, 135}, {93, 119},
        {80, 105}, {88, 91}, {72, 81}, {64, 68}, {52, 72}, {47, 64}
    };

    constexpr int16_t kCyberMapWestInner[][2] =
    {
        {43, 76}, {55, 84}, {50, 98}, {65, 108}, {58, 124}, {74, 136},
        {67, 151}, {82, 162}, {78, 178}, {91, 184}, {81, 165}, {87, 146},
        {77, 130}, {83, 113}, {70, 96}, {61, 82}, {43, 76}
    };

    constexpr int16_t kCyberMapWestShelf[][2] =
    {
        {21, 92}, {30, 101}, {25, 116}, {37, 129}, {32, 146}, {45, 160},
        {39, 177}, {54, 190}, {45, 173}, {51, 153}, {42, 135}, {47, 118},
        {35, 103}, {21, 92}
    };

    constexpr int16_t kCyberMapEastCoast[][2] =
    {
        {148, 151}, {160, 142}, {172, 151}, {186, 143}, {199, 154},
        {214, 162}, {223, 177}, {216, 190}, {224, 202}, {207, 211},
        {190, 205}, {176, 213}, {162, 197}, {153, 184}, {146, 166},
        {148, 151}
    };

    constexpr int16_t kCyberMapEastIslands[][2] =
    {
        {153, 124}, {166, 130}, {176, 124}, {187, 136}, {202, 134},
        {213, 148}, {205, 156}, {190, 151}, {174, 159}, {163, 146}, {153, 124}
    };

    constexpr int16_t kCyberMapNorthIslands[][2] =
    {
        {126, 49}, {138, 42}, {151, 48}, {164, 40}, {179, 51}, {195, 55},
        {205, 66}
    };

    constexpr int16_t kCyberMapInteriorDots[][2] =
    {
        {40, 86}, {53, 91}, {65, 98}, {37, 106}, {49, 118}, {61, 132},
        {42, 149}, {56, 164}, {70, 176}, {84, 185}, {75, 94}, {84, 112},
        {91, 129}, {87, 151}, {68, 74}, {75, 86}, {58, 148}, {72, 158},
        {92, 174}, {52, 184}, {157, 158}, {168, 166}, {183, 160}, {198, 170},
        {207, 186}, {190, 194}, {172, 188}, {159, 177}, {180, 181}, {202, 198},
        {140, 82}, {153, 76}, {166, 86}, {180, 78}, {195, 91}, {111, 145},
        {128, 153}, {133, 170}, {143, 98}, {158, 105}, {176, 100}, {190, 112}
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

    int planeRadarDistSqFromCenter(int16_t x, int16_t y)
    {
        const int16_t dx = x - PlaneRadarTheme::centerX;
        const int16_t dy = y - PlaneRadarTheme::centerY;
        return static_cast<int>(dx) * static_cast<int>(dx) +
               static_cast<int>(dy) * static_cast<int>(dy);
    }

    float planeRadarOuterRangeKm(const AppConfig &config)
    {
        return max(config.maxRangeKm, 1.0f) * 4.0f / 3.0f;
    }

    float planeRadarInnerRangeKm(const AppConfig &config)
    {
        return planeRadarOuterRangeKm(config) *
               static_cast<float>(PlaneRadarTheme::outerRadius - PlaneRadarTheme::aircraftInsideRingInset) /
               static_cast<float>(PlaneRadarTheme::outerRadius);
    }

    struct PlaneRadarDrawItem
    {
        uint8_t index = 0;
        int16_t x = 0;
        int16_t y = 0;
        int distSq = 0;
    };

    void sortPlaneRadarItemsFarFirst(PlaneRadarDrawItem *items, uint8_t count)
    {
        for (uint8_t i = 1; i < count; ++i)
        {
            const PlaneRadarDrawItem key = items[i];
            uint8_t j = i;
            while (j > 0 && items[j - 1].distSq < key.distSq)
            {
                items[j] = items[j - 1];
                --j;
            }
            items[j] = key;
        }
    }

    int planeRadarAbsDiff(int a, int b)
    {
        return a > b ? a - b : b - a;
    }

    const GFXfont *pickPlaneRadarTagFont(TFT_eSprite &canvas)
    {
        canvas.setTextSize(1);

        canvas.setFreeFont(&FreeSansBold12pt7b);
        const int height12 = canvas.fontHeight();
        canvas.setFreeFont(&FreeSansBold9pt7b);
        const int height9 = canvas.fontHeight();

        const int diff12 = planeRadarAbsDiff(height12, PlaneRadarTheme::aircraftTagLabelHeightPx);
        const int diff9 = planeRadarAbsDiff(height9, PlaneRadarTheme::aircraftTagLabelHeightPx);
        return diff12 < diff9 ? &FreeSansBold12pt7b : &FreeSansBold9pt7b;
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
    return cyberpunkColor(cyberpunkTuning().outerRing,
                          cyberpunkTuning().ringBrightness * cyberpunkTuning().outerGlowBrightness);
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

uint16_t RadarRenderer::cyberMapColor()
{
    return cyberpunkColor(cyberpunkTuning().map, cyberpunkTuning().mapBrightness);
}

uint16_t RadarRenderer::cyberRadialGridColor()
{
    return cyberpunkColor(cyberpunkTuning().ringDim, cyberpunkTuning().radialGridBrightness);
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
        case UiTheme::PlaneRadar:
            renderPlaneRadarFrame(aircraft, aircraftCount, selectedAircraftIndex, config, statusText);
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

    drawModernReferenceRadarFrame(frame_, config);
    drawModernReferenceAircraft(frame_, aircraft, aircraftCount, selectedAircraftIndex, config);
    const bool statusLooksImportant = statusText != nullptr &&
                                      (strstr(statusText, "WIFI") != nullptr ||
                                       strstr(statusText, "AUTH") != nullptr ||
                                       strstr(statusText, "API") != nullptr ||
                                       strstr(statusText, "429") != nullptr ||
                                       strstr(statusText, "ERR") != nullptr ||
                                       strstr(statusText, "NO ") != nullptr);
    if (modernTuning().showStatusText || statusLooksImportant)
    {
        drawStatusText(frame_, statusText);
    }
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
        bool insideOuterRing = false;
        if (modernReferenceToScreen(aircraft[i], config, x, y, insideOuterRing) && !insideOuterRing)
        {
            drawModernReferenceBeyondDot(canvas, aircraft[i]);
        }
    }

    LabelRect usedLabels[kMaxTrackedLabels];
    uint8_t usedLabelCount = 0;
    uint8_t shownLabelCount = 0;
    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        int16_t x = 0;
        int16_t y = 0;
        bool insideOuterRing = false;
        if (modernReferenceToScreen(aircraft[i], config, x, y, insideOuterRing) && insideOuterRing)
        {
            drawModernReferenceAircraftSymbol(canvas, aircraft[i], x, y);
            if (config.showLabels &&
                shouldShowAircraftLabel(aircraft[i],
                                        i,
                                        selectedAircraftIndex,
                                        aircraftCount,
                                        shownLabelCount,
                                        modernTuning().maxLabels))
            {
                const uint8_t before = usedLabelCount;
                drawModernReferenceAircraftTag(canvas,
                                               aircraft[i],
                                               x,
                                               y,
                                               i == selectedAircraftIndex,
                                               usedLabels,
                                               usedLabelCount);
                if (usedLabelCount != before)
                {
                    ++shownLabelCount;
                }
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
                                                   int16_t y,
                                                   bool selected,
                                                   LabelRect *usedLabels,
                                                   uint8_t &usedLabelCount)
{
    char callsign[16];
    char altitude[16];
    snprintf(callsign,
             sizeof(callsign),
             "%s",
             target.callsign[0] != '\0' ? target.callsign : "UNKNOWN");
    snprintf(altitude, sizeof(altitude), "%.0fm", target.altitudeM);

    const ModernRadarTuning &tuning = modernTuning();
    bool labelRight = x < ModernRadarTheme::centerX;
    const int16_t symbolHalf = static_cast<int16_t>((ModernRadarTheme::noseLength +
                                                     ModernRadarTheme::tailHalfWidth) *
                                                    tuning.aircraftScale);
    const int16_t labelWidth = selected ?
                               max(canvas.textWidth(callsign, 1), canvas.textWidth(altitude, 1)) :
                               canvas.textWidth(callsign, 1);
    const int16_t blockHeight = selected ? 20 : 10;
    int16_t labelY = constrain(y - blockHeight / 2, 2, ModernRadarTheme::size - blockHeight - 2);
    int16_t anchorX = 0;
    LabelRect rect = {0, 0, 0, 0};
    bool reserved = false;

    for (uint8_t attempt = 0; attempt < 5 && !reserved; ++attempt)
    {
        labelRight = attempt == 3 ? !labelRight : labelRight;
        int16_t candidateY = labelY;
        if (attempt == 1)
        {
            candidateY = constrain(labelY - 12, 2, ModernRadarTheme::size - blockHeight - 2);
        }
        else if (attempt == 2)
        {
            candidateY = constrain(labelY + 12, 2, ModernRadarTheme::size - blockHeight - 2);
        }
        else if (attempt == 4)
        {
            candidateY = constrain(labelY + 22, 2, ModernRadarTheme::size - blockHeight - 2);
        }

        anchorX = labelRight ?
                  x + symbolHalf + tuning.labelGap + 2 :
                  x - symbolHalf - tuning.labelGap - 2;

        if (labelRight)
        {
            anchorX = constrain(anchorX, 2, ModernRadarTheme::size - labelWidth - 2);
            rect = {static_cast<int16_t>(anchorX - 1), static_cast<int16_t>(candidateY - 1), static_cast<int16_t>(labelWidth + 2), static_cast<int16_t>(blockHeight + 2)};
        }
        else
        {
            anchorX = constrain(anchorX, labelWidth + 2, ModernRadarTheme::size - 2);
            rect = {static_cast<int16_t>(anchorX - labelWidth - 1), static_cast<int16_t>(candidateY - 1), static_cast<int16_t>(labelWidth + 2), static_cast<int16_t>(blockHeight + 2)};
        }

        if (reserveLabelRect(usedLabels, usedLabelCount, rect))
        {
            labelY = candidateY;
            reserved = true;
        }
    }

    if (!reserved)
    {
        if (!selected)
        {
            return;
        }
        if (usedLabelCount < kMaxTrackedLabels)
        {
            usedLabels[usedLabelCount++] = rect;
        }
    }

    if (modernTuning().showLeaderLines)
    {
        canvas.drawLine(x,
                        y,
                        labelRight ? rect.x : static_cast<int16_t>(rect.x + rect.w),
                        static_cast<int16_t>(labelY + 4),
                        modernGridColor());
    }

    canvas.fillRect(rect.x, rect.y, rect.w, rect.h, modernBackgroundColor());
    if (labelRight)
    {
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextColor(modernTextColor(), modernBackgroundColor());
        canvas.drawString(callsign, anchorX, labelY, 1);
        if (selected)
        {
            canvas.setTextColor(modernAltitudeColor(), modernBackgroundColor());
            canvas.drawString(altitude, anchorX, labelY + 10, 1);
        }
        return;
    }

    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(modernTextColor(), modernBackgroundColor());
    canvas.drawString(callsign, anchorX, labelY, 1);
    if (selected)
    {
        canvas.setTextColor(modernAltitudeColor(), modernBackgroundColor());
        canvas.drawString(altitude, anchorX, labelY + 10, 1);
    }
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

void RadarRenderer::renderPlaneRadarFrame(const Aircraft *aircraft,
                                          uint8_t aircraftCount,
                                          uint8_t selectedAircraftIndex,
                                          const AppConfig &config,
                                          const char *statusText)
{
    const bool statusLooksImportant = statusText != nullptr &&
                                      (strstr(statusText, "WIFI") != nullptr ||
                                       strstr(statusText, "AUTH") != nullptr ||
                                       strstr(statusText, "API") != nullptr ||
                                       strstr(statusText, "429") != nullptr ||
                                       strstr(statusText, "ERR") != nullptr ||
                                       strstr(statusText, "NO ") != nullptr);

    drawPlaneRadarBackground(frame_, config);
    drawPlaneRadarAircraft(frame_, aircraft, aircraftCount, selectedAircraftIndex, config);
    if (statusLooksImportant)
    {
        renderPlaneRadarStatusText(frame_,
                                   statusText,
                                   TFT_WHITE,
                                   modernBackgroundColor());
    }
    frame_.pushSprite(0, 0);
}

void RadarRenderer::drawPlaneRadarBackground(TFT_eSprite &canvas, const AppConfig &config)
{
    const uint16_t bg = modernBackgroundColor();
    const uint16_t grid = tft_.color565(16, 100, 32);
    const uint16_t text = TFT_WHITE;
    const uint16_t center = TFT_WHITE;

    canvas.fillSprite(bg);
    canvas.setTextSize(1);

    for (uint8_t i = 1; i <= PlaneRadarTheme::ringCount; ++i)
    {
        const int16_t radius = static_cast<int16_t>((PlaneRadarTheme::outerRadius * i) /
                                                   PlaneRadarTheme::ringCount);
        drawModernWideCircle(canvas,
                             PlaneRadarTheme::centerX,
                             PlaneRadarTheme::centerY,
                             radius,
                             grid,
                             static_cast<uint8_t>(max(1.0f, PlaneRadarTheme::gridStrokeHalfWidth * 2.0f)));
    }

    drawModernWideLine(canvas,
                       PlaneRadarTheme::centerX,
                       PlaneRadarTheme::centerY - PlaneRadarTheme::outerRadius,
                       PlaneRadarTheme::centerX,
                       PlaneRadarTheme::centerY + PlaneRadarTheme::outerRadius,
                       grid,
                       2);
    drawModernWideLine(canvas,
                       PlaneRadarTheme::centerX - PlaneRadarTheme::outerRadius,
                       PlaneRadarTheme::centerY,
                       PlaneRadarTheme::centerX + PlaneRadarTheme::outerRadius,
                       PlaneRadarTheme::centerY,
                       grid,
                       2);

    drawPlaneRadarCardinals(canvas, text, bg);
    drawPlaneRadarRangeLabels(canvas, config, grid, text, bg);
    renderOriginalRunwayOverlay(canvas);
    canvas.fillCircle(PlaneRadarTheme::centerX,
                      PlaneRadarTheme::centerY,
                      PlaneRadarTheme::centerDotRadius,
                      center);
}

void RadarRenderer::drawPlaneRadarCardinals(TFT_eSprite &canvas,
                                            uint16_t textColor,
                                            uint16_t backgroundColor)
{
    // The reference project targets about 14 px cap height for N/S/W/E.
    // TFT_eSPI's built-in font 2 is closer to that size than FreeSansBold9pt.
    constexpr uint8_t cardinalFont = 2;

    canvas.setFreeFont(nullptr);
    canvas.setTextSize(1);
    canvas.setTextColor(textColor, backgroundColor);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("N",
                      PlaneRadarTheme::centerX,
                      PlaneRadarTheme::cardinalNorthOffsetY,
                      cardinalFont);
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString("S",
                      PlaneRadarTheme::centerX,
                      PlaneRadarTheme::size - 1 + PlaneRadarTheme::cardinalSouthOffsetY,
                      cardinalFont);
    canvas.setTextDatum(ML_DATUM);
    canvas.drawString("W", 0, PlaneRadarTheme::centerY, cardinalFont);
    canvas.setTextDatum(MR_DATUM);
    canvas.drawString("E", PlaneRadarTheme::size - 1, PlaneRadarTheme::centerY, cardinalFont);
}

void RadarRenderer::drawPlaneRadarRangeLabels(TFT_eSprite &canvas,
                                              const AppConfig &config,
                                              uint16_t gridColor,
                                              uint16_t textColor,
                                              uint16_t backgroundColor)
{
    (void)textColor;

    const float displayRangeKm = max(config.maxRangeKm, 1.0f);
    char label[16];

    snprintf(label, sizeof(label), "%.0fkm", displayRangeKm);
    canvas.setTextSize(1);
    canvas.setTextDatum(MR_DATUM);
    const int16_t x = PlaneRadarTheme::centerX +
                      PlaneRadarTheme::outerRadius -
                      PlaneRadarTheme::scaleGapFromOuterRing;
    const int16_t y = PlaneRadarTheme::centerY;
    const int16_t textWidth = canvas.textWidth(label, 1);
    const int16_t textHeight = 8;
    canvas.fillRect(x - textWidth - 3,
                    y - textHeight / 2 - 2,
                    textWidth + 6,
                    textHeight + 4,
                    backgroundColor);
    canvas.setTextColor(gridColor, backgroundColor);
    canvas.drawString(label, x, y, 1);
}

void RadarRenderer::drawPlaneRadarAircraft(TFT_eSprite &canvas,
                                           const Aircraft *aircraft,
                                           uint8_t aircraftCount,
                                           uint8_t selectedAircraftIndex,
                                           const AppConfig &config)
{
    if (aircraft == nullptr)
    {
        return;
    }

    const uint16_t aircraftColor = tft_.color565(255, 0, 0);
    const uint16_t vectorColor = tft_.color565(255, 0, 255);
    const uint16_t selectedColor = TFT_WHITE;
    const uint16_t textColor = TFT_WHITE;
    const uint16_t typeColor = tft_.color565(90, 200, 255);
    const uint16_t bg = modernBackgroundColor();

    PlaneRadarDrawItem dots[PlaneRadarTheme::maxDrawItems];
    PlaneRadarDrawItem items[PlaneRadarTheme::maxDrawItems];
    uint8_t dotCount = 0;
    uint8_t itemCount = 0;

    for (uint8_t i = 0; i < aircraftCount; ++i)
    {
        int16_t x = 0;
        int16_t y = 0;
        bool insideOuterRing = false;
        if (!planeRadarToScreen(aircraft[i], config, x, y, insideOuterRing))
        {
            continue;
        }

        if (insideOuterRing)
        {
            if (itemCount < PlaneRadarTheme::maxDrawItems)
            {
                items[itemCount].index = i;
                items[itemCount].x = x;
                items[itemCount].y = y;
                items[itemCount].distSq = planeRadarDistSqFromCenter(x, y);
                ++itemCount;
            }
        }
        else if (dotCount < PlaneRadarTheme::maxDrawItems)
        {
            const float bearing = aircraft[i].bearingDeg * DEG_TO_RAD;
            const int16_t rimRadius = PlaneRadarTheme::centerX - PlaneRadarTheme::beyondRingScreenMargin;
            const int16_t dotX = PlaneRadarTheme::centerX +
                                 static_cast<int16_t>(lroundf(sinf(bearing) * rimRadius));
            const int16_t dotY = PlaneRadarTheme::centerY -
                                 static_cast<int16_t>(lroundf(cosf(bearing) * rimRadius));
            dots[dotCount].index = i;
            dots[dotCount].x = dotX;
            dots[dotCount].y = dotY;
            dots[dotCount].distSq = planeRadarDistSqFromCenter(dotX, dotY);
            ++dotCount;
        }
    }

    sortPlaneRadarItemsFarFirst(dots, dotCount);
    for (uint8_t i = 0; i < dotCount; ++i)
    {
        drawPlaneRadarBeyondDot(canvas, aircraft[dots[i].index], aircraftColor);
    }

    sortPlaneRadarItemsFarFirst(items, itemCount);
    for (uint8_t i = 0; i < itemCount; ++i)
    {
        const Aircraft &target = aircraft[items[i].index];
        drawPlaneRadarSpeedVector(canvas, target, items[i].x, items[i].y, config, vectorColor);
        drawPlaneRadarAircraftSymbol(canvas, target, items[i].x, items[i].y, false, aircraftColor, selectedColor);
    }

    LabelRect usedLabels[kMaxTrackedLabels];
    uint8_t usedLabelCount = 0;
    for (uint8_t i = 0; i < itemCount; ++i)
    {
        const uint8_t aircraftIndex = items[i].index;
        if (config.showLabels)
        {
            drawPlaneRadarAircraftLabel(canvas,
                                        aircraft[aircraftIndex],
                                        items[i].x,
                                        items[i].y,
                                        aircraftIndex == selectedAircraftIndex,
                                        usedLabels,
                                        usedLabelCount,
                                        textColor,
                                        typeColor,
                                        bg);
        }
    }
}

void RadarRenderer::drawPlaneRadarAircraftSymbol(TFT_eSprite &canvas,
                                                 const Aircraft &target,
                                                 int16_t x,
                                                 int16_t y,
                                                 bool selected,
                                                 uint16_t aircraftColor,
                                                 uint16_t selectedColor)
{
    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    const float heading = headingDeg * DEG_TO_RAD;
    const float sinHeading = sinf(heading);
    const float cosHeading = cosf(heading);
    int16_t noseX = 0;
    int16_t noseY = 0;
    planeRadarNoseTip(x, y, headingDeg, noseX, noseY);
    const int16_t baseX = x - static_cast<int16_t>(lroundf(sinHeading * PlaneRadarTheme::aircraftTailLength));
    const int16_t baseY = y + static_cast<int16_t>(lroundf(cosHeading * PlaneRadarTheme::aircraftTailLength));
    const int16_t wingX = static_cast<int16_t>(lroundf(cosHeading * PlaneRadarTheme::aircraftTailHalfWidth));
    const int16_t wingY = static_cast<int16_t>(lroundf(sinHeading * PlaneRadarTheme::aircraftTailHalfWidth));

    canvas.fillTriangle(noseX,
                        noseY,
                        baseX + wingX,
                        baseY + wingY,
                        baseX - wingX,
                        baseY - wingY,
                        aircraftColor);
    (void)selected;
    (void)selectedColor;
}

void RadarRenderer::drawPlaneRadarSpeedVector(TFT_eSprite &canvas,
                                              const Aircraft &target,
                                              int16_t x,
                                              int16_t y,
                                              const AppConfig &config,
                                              uint16_t vectorColor)
{
    if (!isfinite(target.speedMs) || target.speedMs <= 0.0f)
    {
        return;
    }

    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    const float heading = headingDeg * DEG_TO_RAD;
    int16_t startX = 0;
    int16_t startY = 0;
    planeRadarNoseTip(x, y, headingDeg, startX, startY);

    const int length = planeRadarSpeedVectorLengthPx(target.speedMs, config);
    int16_t endX = startX + static_cast<int16_t>(lroundf(sinf(heading) * length));
    int16_t endY = startY - static_cast<int16_t>(lroundf(cosf(heading) * length));
    clipPlaneRadarPointToOuterRing(startX, startY, endX, endY);
    if (endX == startX && endY == startY)
    {
        return;
    }

    drawModernWideLine(canvas, startX, startY, endX, endY, vectorColor, 2);
}

void RadarRenderer::drawPlaneRadarAircraftLabel(TFT_eSprite &canvas,
                                                const Aircraft &target,
                                                int16_t x,
                                                int16_t y,
                                                bool selected,
                                                LabelRect *usedLabels,
                                                uint8_t &usedLabelCount,
                                                uint16_t textColor,
                                                uint16_t typeColor,
                                                uint16_t backgroundColor)
{
    char callsign[16];
    char middleLine[16];
    char altitude[16];
    snprintf(callsign, sizeof(callsign), "%s", target.callsign[0] != '\0' ? target.callsign : "UNKNOWN");
    if (target.type[0] != '\0')
    {
        snprintf(middleLine, sizeof(middleLine), "%s", target.type);
    }
    else if (isfinite(target.speedMs))
    {
        snprintf(middleLine, sizeof(middleLine), "%.0fkt", target.speedMs * 1.943844f);
    }
    else
    {
        snprintf(middleLine, sizeof(middleLine), "--");
    }
    formatPlaneRadarAltitude(target.altitudeM, altitude, sizeof(altitude));

    canvas.setFreeFont(nullptr);
    canvas.setTextSize(1);
    constexpr uint8_t tagFont = 1;
    const int16_t lineHeight = 11;
    const int16_t labelWidth = max(max(canvas.textWidth(callsign, tagFont),
                                       canvas.textWidth(middleLine, tagFont)),
                                   canvas.textWidth(altitude, tagFont));
    const int16_t blockHeight = lineHeight * 3;
    const int16_t symbolHalf = PlaneRadarTheme::aircraftNoseLength +
                               PlaneRadarTheme::aircraftTailHalfWidth;
    const bool labelRight = x < PlaneRadarTheme::centerX;
    int16_t labelX = 0;
    int16_t labelY = constrain(y - blockHeight / 2, 1, PlaneRadarTheme::size - blockHeight - 1);
    LabelRect rect = {0, 0, 0, 0};

    if (labelRight)
    {
        labelX = x + symbolHalf + PlaneRadarTheme::aircraftLabelGap;
        labelX = min<int16_t>(labelX, PlaneRadarTheme::size - labelWidth - 1);
        rect = {labelX, labelY, static_cast<int16_t>(labelWidth + 1), blockHeight};
        canvas.setTextDatum(TL_DATUM);
    }
    else
    {
        labelX = x - symbolHalf - PlaneRadarTheme::aircraftLabelGap;
        labelX = max<int16_t>(labelX, labelWidth + 1);
        rect = {static_cast<int16_t>(labelX - labelWidth), labelY, static_cast<int16_t>(labelWidth + 1), blockHeight};
        canvas.setTextDatum(TR_DATUM);
    }

    (void)reserveLabelRect(usedLabels, usedLabelCount, rect);
    (void)selected;

    canvas.setTextColor(textColor, backgroundColor);
    canvas.drawString(callsign, labelX, labelY, tagFont);
    labelY += lineHeight;
    canvas.setTextColor(typeColor, backgroundColor);
    canvas.drawString(middleLine, labelX, labelY, tagFont);
    labelY += lineHeight;
    canvas.setTextColor(tft_.color565(255, 200, 0), backgroundColor);
    canvas.drawString(altitude, labelX, labelY, tagFont);
}

void RadarRenderer::drawPlaneRadarBeyondDot(TFT_eSprite &canvas,
                                            const Aircraft &target,
                                            uint16_t aircraftColor)
{
    if (!isfinite(target.bearingDeg))
    {
        return;
    }

    const float bearing = target.bearingDeg * DEG_TO_RAD;
    const int16_t rimRadius = PlaneRadarTheme::centerX - PlaneRadarTheme::beyondRingScreenMargin;
    const int16_t x = PlaneRadarTheme::centerX +
                      static_cast<int16_t>(lroundf(sinf(bearing) * rimRadius));
    const int16_t y = PlaneRadarTheme::centerY -
                      static_cast<int16_t>(lroundf(cosf(bearing) * rimRadius));
    canvas.fillCircle(x, y, PlaneRadarTheme::beyondRingDotRadius, aircraftColor);
}

bool RadarRenderer::planeRadarToScreen(const Aircraft &target,
                                       const AppConfig &config,
                                       int16_t &x,
                                       int16_t &y,
                                       bool &insideDisplayRange) const
{
    if (!target.valid || !isfinite(target.distanceKm) || !isfinite(target.bearingDeg))
    {
        return false;
    }

    const float outerRangeKm = planeRadarOuterRangeKm(config);
    const float fetchRangeKm = outerRangeKm * 1.25f;
    if (target.distanceKm > fetchRangeKm)
    {
        return false;
    }

    insideDisplayRange = target.distanceKm <= planeRadarInnerRangeKm(config);
    const float radius = constrain((target.distanceKm / outerRangeKm) *
                                       static_cast<float>(PlaneRadarTheme::outerRadius),
                                   0.0f,
                                   static_cast<float>(PlaneRadarTheme::outerRadius));
    const float bearing = target.bearingDeg * DEG_TO_RAD;
    x = PlaneRadarTheme::centerX + static_cast<int16_t>(lroundf(sinf(bearing) * radius));
    y = PlaneRadarTheme::centerY - static_cast<int16_t>(lroundf(cosf(bearing) * radius));
    return true;
}

int RadarRenderer::planeRadarSpeedVectorLengthPx(float speedMs, const AppConfig &config) const
{
    if (!isfinite(speedMs) || speedMs <= 0.0f)
    {
        return 0;
    }

    (void)config;

    const float knots = speedMs * 1.943844f;
    constexpr float kmPerKnotPerHorizon = 1.852f *
                                          static_cast<float>(PlaneRadarTheme::trackHorizonSec) /
                                          3600.0f;
    const float px = knots *
                     kmPerKnotPerHorizon *
                     static_cast<float>(PlaneRadarTheme::outerRadius) /
                     PlaneRadarTheme::trackRefOuterKm *
                     PlaneRadarTheme::trackLengthScale;
    const int length = static_cast<int>(px + 0.5f);
    if (length < PlaneRadarTheme::speedLineMinPx)
    {
        return PlaneRadarTheme::speedLineMinPx;
    }
    return length;
}

void RadarRenderer::planeRadarNoseTip(int16_t x,
                                      int16_t y,
                                      float headingDeg,
                                      int16_t &tipX,
                                      int16_t &tipY) const
{
    const float heading = headingDeg * DEG_TO_RAD;
    tipX = x + static_cast<int16_t>(lroundf(sinf(heading) * PlaneRadarTheme::aircraftNoseLength));
    tipY = y - static_cast<int16_t>(lroundf(cosf(heading) * PlaneRadarTheme::aircraftNoseLength));
}

void RadarRenderer::clipPlaneRadarPointToOuterRing(int16_t x0,
                                                   int16_t y0,
                                                   int16_t &x1,
                                                   int16_t &y1) const
{
    const float dx = static_cast<float>(x1 - x0);
    const float dy = static_cast<float>(y1 - y0);
    const float endDx = static_cast<float>(x1 - PlaneRadarTheme::centerX);
    const float endDy = static_cast<float>(y1 - PlaneRadarTheme::centerY);
    const int16_t radius = PlaneRadarTheme::outerRadius;
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
        const float testX = static_cast<float>(x0) + dx * mid - PlaneRadarTheme::centerX;
        const float testY = static_cast<float>(y0) + dy * mid - PlaneRadarTheme::centerY;
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

void RadarRenderer::formatPlaneRadarAltitude(float altitudeM, char *buffer, size_t bufferSize) const
{
    if (buffer == nullptr || bufferSize == 0)
    {
        return;
    }

    if (!isfinite(altitudeM))
    {
        snprintf(buffer, bufferSize, "--");
        return;
    }

    const int altitudeFt = static_cast<int>(altitudeM * 3.28084f);
    snprintf(buffer, bufferSize, "%dft", altitudeFt);
}

void RadarRenderer::renderPlaneRadarStatusText(TFT_eSprite &canvas,
                                               const char *statusText,
                                               uint16_t textColor,
                                               uint16_t backgroundColor)
{
    if (statusText == nullptr || statusText[0] == '\0')
    {
        return;
    }

    char status[32];
    snprintf(status, sizeof(status), "%s", statusText);
    char *secondLine = strchr(status, '\n');

    canvas.setTextDatum(BC_DATUM);
    canvas.setTextColor(textColor, backgroundColor);
    if (secondLine != nullptr)
    {
        *secondLine = '\0';
        ++secondLine;
        canvas.drawString(status, PlaneRadarTheme::centerX, 214, 1);
        canvas.drawString(secondLine, PlaneRadarTheme::centerX, 226, 1);
    }
    else
    {
        canvas.drawString(status, PlaneRadarTheme::centerX, 226, 1);
    }
}

void RadarRenderer::renderOriginalRunwayOverlay(TFT_eSprite &canvas)
{
    (void)canvas;
    // TODO: add a lightweight runway overlay if local airport runway data is introduced.
}

void RadarRenderer::renderCyberpunkRadarFrame(const Aircraft *aircraft,
                                              uint8_t aircraftCount,
                                              uint8_t selectedAircraftIndex,
                                              const AppConfig &config,
                                              const char *statusText)
{
#if USE_CYBERPUNK_STATIC_BG
    drawCyberpunkStaticBackground(frame_);
    drawCyberpunkStaticCardinals(frame_);
#else
    drawCyberpunkBackground(frame_);
    drawCyberpunkMapTexture(frame_);
    drawCyberpunkRadialGrid(frame_);
    drawCyberpunkRings(frame_);
    drawCyberpunkOuterScale(frame_);
    drawCyberpunkRangeLabels(frame_, config);
    drawCyberpunkCenter(frame_);
    drawCyberpunkCardinals(frame_);
#endif
    drawCyberpunkAircraftTargets(frame_, aircraft, aircraftCount, selectedAircraftIndex, config);
    drawCyberpunkStatusText(frame_, statusText, aircraftCount, config);
    frame_.pushSprite(0, 0);
}

#if USE_CYBERPUNK_STATIC_BG
void RadarRenderer::drawCyberpunkStaticBackground(TFT_eSprite &canvas)
{
    canvas.setSwapBytes(true);
    canvas.pushImage(0,
                     0,
                     CYBERPUNK_BG_WIDTH,
                     CYBERPUNK_BG_HEIGHT,
                     CYBERPUNK_BG_240);
    canvas.setSwapBytes(false);
}

void RadarRenderer::drawCyberpunkStaticCardinals(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const uint16_t color = color565Scaled(tuning.outerRing, tuning.globalBrightness, 1.2f);

    canvas.setTextColor(color);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("N", CyberpunkRadarTheme::centerX, 17, 1);
    canvas.drawString("W", 16, CyberpunkRadarTheme::centerY, 1);
    canvas.drawString("S", CyberpunkRadarTheme::centerX, 224, 1);
    canvas.drawString("E", 224, CyberpunkRadarTheme::centerY, 1);
}
#endif

void RadarRenderer::drawCyberpunkBackground(TFT_eSprite &canvas)
{
    canvas.fillSprite(cyberBackgroundColor());
}

void RadarRenderer::drawCyberpunkMapTexture(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    if (!tuning.mapEnabled)
    {
        return;
    }

    const uint16_t mapColor = cyberMapColor();
    drawCyberpunkDottedMapPath(canvas,
                               kCyberMapWestCoast,
                               sizeof(kCyberMapWestCoast) / sizeof(kCyberMapWestCoast[0]),
                               mapColor,
                               4);
    drawCyberpunkDottedMapPath(canvas,
                               kCyberMapEastCoast,
                               sizeof(kCyberMapEastCoast) / sizeof(kCyberMapEastCoast[0]),
                               mapColor);
    drawCyberpunkDottedMapPath(canvas,
                               kCyberMapEastIslands,
                               sizeof(kCyberMapEastIslands) / sizeof(kCyberMapEastIslands[0]),
                               mapColor);
    drawCyberpunkDottedMapPath(canvas,
                               kCyberMapNorthIslands,
                               sizeof(kCyberMapNorthIslands) / sizeof(kCyberMapNorthIslands[0]),
                               mapColor);
    drawCyberpunkMapDots(canvas,
                         kCyberMapInteriorDots,
                         sizeof(kCyberMapInteriorDots) / sizeof(kCyberMapInteriorDots[0]),
                         mapColor);

    const uint16_t noise = cyberNoiseColor();
    const uint16_t noisePointCount = static_cast<uint16_t>(58.0f * tuning.mapDensity);
    for (uint16_t i = 0; i < noisePointCount; ++i)
    {
        const uint16_t seed = static_cast<uint16_t>(i * i * 29U + i * 97U + 53U);
        const int16_t x = static_cast<int16_t>((seed * 37U + 19U) % CyberpunkRadarTheme::size);
        const int16_t y = static_cast<int16_t>((seed * 61U + i * 11U + 43U) % CyberpunkRadarTheme::size);
        if (cyberpunkPointInsideOuter(x, y))
        {
            canvas.drawPixel(x, y, noise);
        }
    }
}

void RadarRenderer::drawCyberpunkDottedMapPath(TFT_eSprite &canvas,
                                               const int16_t points[][2],
                                               uint8_t pointCount,
                                               uint16_t color,
                                               uint8_t baseSpacing)
{
    if (points == nullptr || pointCount < 2)
    {
        return;
    }

    const uint8_t spacing = max<uint8_t>(2, baseSpacing);
    for (uint8_t i = 1; i < pointCount; ++i)
    {
        const int16_t x0 = points[i - 1][0];
        const int16_t y0 = points[i - 1][1];
        const int16_t x1 = points[i][0];
        const int16_t y1 = points[i][1];
        const int16_t dx = x1 - x0;
        const int16_t dy = y1 - y0;
        const int16_t steps = max(abs(dx), abs(dy));
        const int16_t divisor = steps > 0 ? steps : 1;
        const int16_t normalX = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
        const int16_t normalY = dx == 0 ? 0 : (dx > 0 ? -1 : 1);
        int16_t step = 0;
        while (step <= steps)
        {
            const int16_t x = x0 + static_cast<int16_t>((static_cast<int32_t>(dx) * step) / divisor);
            const int16_t y = y0 + static_cast<int16_t>((static_cast<int32_t>(dy) * step) / divisor);
            const int16_t jitter = ((step + i * 5) % 4) == 0 ? 1 : (((step + i * 7) % 5) == 0 ? -1 : 0);
            const int16_t dotX = x + normalX * jitter;
            const int16_t dotY = y + normalY * jitter;
            if (cyberpunkPointInsideOuter(dotX, dotY))
            {
                canvas.drawPixel(dotX, dotY, color);
                if (((step + i) % 11) == 0 && cyberpunkPointInsideOuter(dotX + normalX, dotY + normalY))
                {
                    canvas.drawPixel(dotX + normalX, dotY + normalY, color);
                }
            }

            step += spacing + ((step + i * 3) % 4);
        }
    }
}

void RadarRenderer::drawCyberpunkMapDots(TFT_eSprite &canvas,
                                         const int16_t points[][2],
                                         uint8_t pointCount,
                                         uint16_t color)
{
    if (points == nullptr || pointCount == 0)
    {
        return;
    }

    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const uint8_t stride = tuning.mapDensity < 0.65f ? 3 : (tuning.mapDensity < 1.1f ? 2 : 1);
    for (uint8_t i = 0; i < pointCount; ++i)
    {
        if ((i % stride) != 0)
        {
            continue;
        }

        const int16_t x = points[i][0];
        const int16_t y = points[i][1];
        const bool leftMapDot = x < CyberpunkRadarTheme::centerX - 8;
        if (leftMapDot && (i % 5) != 0)
        {
            continue;
        }

        if (cyberpunkPointInsideOuter(x, y))
        {
            canvas.drawPixel(x, y, color);
            if (!leftMapDot && tuning.mapDensity > 1.35f && cyberpunkPointInsideOuter(x + 1, y))
            {
                canvas.drawPixel(x + 1, y, color);
            }
        }
    }
}

void RadarRenderer::drawCyberpunkRadialGrid(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    if (!tuning.radialGridEnabled)
    {
        return;
    }

    const uint8_t stepDeg = max<uint8_t>(tuning.radialGridStepDeg, 1);
    for (uint16_t deg = 0; deg < 360; deg += stepDeg)
    {
        if ((deg % 90) == 0)
        {
            continue;
        }

        const float radians = static_cast<float>(deg) * DEG_TO_RAD;
        const int16_t endX = CyberpunkRadarTheme::centerX +
                             static_cast<int16_t>(sinf(radians) * tuning.innerRadarRadius);
        const int16_t endY = CyberpunkRadarTheme::centerY -
                             static_cast<int16_t>(cosf(radians) * tuning.innerRadarRadius);
        const float local = tuning.radialGridBrightness * ((deg % 30) == 0 ? 1.25f : 0.75f);
        const uint16_t color = color565Scaled(tuning.ringDim, tuning.globalBrightness, local);
        canvas.drawLine(CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY, endX, endY, color);
    }
}

void RadarRenderer::drawCyberpunkRings(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const uint8_t ringCount = max<uint8_t>(tuning.ringCount, 1);
    const uint8_t width = max<uint8_t>(tuning.lineWidth, 1);

    if (tuning.outerGlowWidth > 0)
    {
        drawModernWideCircle(canvas,
                             CyberpunkRadarTheme::centerX,
                             CyberpunkRadarTheme::centerY,
                             tuning.outerRadius,
                             cyberOuterRingColor(),
                             tuning.outerGlowWidth);
    }
    canvas.drawCircle(CyberpunkRadarTheme::centerX,
                      CyberpunkRadarTheme::centerY,
                      tuning.outerRadius - 3,
                      color565Scaled(tuning.ringDim, tuning.globalBrightness, tuning.ringBrightness * 0.65f));

    for (uint8_t i = 1; i <= ringCount; ++i)
    {
        const int16_t radius = (static_cast<int16_t>(tuning.innerRadarRadius) * i) / ringCount;
        drawModernWideCircle(canvas,
                             CyberpunkRadarTheme::centerX,
                             CyberpunkRadarTheme::centerY,
                             radius,
                             cyberRingDimColor(),
                             width);
    }
}

void RadarRenderer::drawCyberpunkOuterScale(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const uint8_t outerStep = max<uint8_t>(tuning.outerTickStepDeg, 1);
    const uint8_t mediumStep = max<uint8_t>(tuning.mediumTickStepDeg, outerStep);
    const uint8_t majorStep = max<uint8_t>(tuning.majorTickStepDeg, mediumStep);

    for (uint16_t deg = 0; deg < 360; deg += outerStep)
    {
        const bool major = (deg % majorStep) == 0;
        const bool medium = (deg % mediumStep) == 0;
        const float radians = static_cast<float>(deg) * DEG_TO_RAD;
        const int16_t tickLength = major ?
                                   tuning.majorTickLength :
                                   (medium ? max<uint8_t>(tuning.minorTickLength + 2, tuning.minorTickLength) :
                                    tuning.minorTickLength);
        const int16_t outer = tuning.outerRadius - 1;
        const int16_t inner = outer - tickLength;
        const int16_t x0 = CyberpunkRadarTheme::centerX + static_cast<int16_t>(sinf(radians) * outer);
        const int16_t y0 = CyberpunkRadarTheme::centerY - static_cast<int16_t>(cosf(radians) * outer);
        const int16_t x1 = CyberpunkRadarTheme::centerX + static_cast<int16_t>(sinf(radians) * inner);
        const int16_t y1 = CyberpunkRadarTheme::centerY - static_cast<int16_t>(cosf(radians) * inner);
        const uint16_t color = major ? cyberTickColor() : (medium ? cyberRingColor() : cyberRingDimColor());
        canvas.drawLine(x0, y0, x1, y1, color);

    }

    drawCyberpunkBearingLabels(canvas);
}

void RadarRenderer::drawCyberpunkBearingLabels(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    if (!tuning.bearingLabelsEnabled)
    {
        return;
    }

    const uint8_t labelStep = max<uint8_t>(tuning.bearingLabelStepDeg, 30);
    const int16_t labelRadius = constrain(static_cast<int16_t>(tuning.outerRadius + tuning.bearingLabelRadiusOffset),
                                          static_cast<int16_t>(tuning.outerRadius - 20),
                                          static_cast<int16_t>(tuning.outerRadius + 2));
    canvas.setTextColor(cyberTickColor(), cyberBackgroundColor());
    canvas.setTextDatum(MC_DATUM);

    for (uint16_t deg = labelStep; deg < 360; deg += labelStep)
    {
        if ((deg % 90) == 0)
        {
            continue;
        }

        char label[4];
        snprintf(label, sizeof(label), "%03u", deg);
        const float radians = static_cast<float>(deg) * DEG_TO_RAD;
        const int16_t x = CyberpunkRadarTheme::centerX + static_cast<int16_t>(sinf(radians) * labelRadius);
        const int16_t y = CyberpunkRadarTheme::centerY - static_cast<int16_t>(cosf(radians) * labelRadius);
        canvas.drawString(label, x, y, 1);
    }
}

void RadarRenderer::drawCyberpunkCardinals(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const int16_t radius = constrain(static_cast<int16_t>(tuning.outerRadius + tuning.cardinalRadiusOffset),
                                     static_cast<int16_t>(tuning.outerRadius - 20),
                                     static_cast<int16_t>(tuning.outerRadius + 2));

    canvas.setTextColor(color565Scaled(tuning.outerRing, tuning.globalBrightness, 1.15f),
                        cyberBackgroundColor());
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("N", CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY - radius, 2);
    canvas.drawString("S", CyberpunkRadarTheme::centerX, CyberpunkRadarTheme::centerY + radius, 2);
    canvas.drawString("E", CyberpunkRadarTheme::centerX + radius, CyberpunkRadarTheme::centerY, 2);
    canvas.drawString("W", CyberpunkRadarTheme::centerX - radius, CyberpunkRadarTheme::centerY, 2);

}

void RadarRenderer::drawCyberpunkCrosshair(TFT_eSprite &canvas)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const int16_t radius = tuning.innerRadarRadius;

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
}

void RadarRenderer::drawCyberpunkRangeLabels(TFT_eSprite &canvas, const AppConfig &config)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    if (!tuning.rangeLabelsEnabled)
    {
        return;
    }

    const uint8_t steps = min<uint8_t>(tuning.ringCount, 5);
    const float maxRangeKm = max(config.maxRangeKm, 1.0f);
    canvas.setTextDatum(MR_DATUM);
    canvas.setTextColor(cyberAltitudeColor(), cyberBackgroundColor());

    for (uint8_t i = 1; i <= steps; ++i)
    {
        const int16_t radius = (static_cast<int16_t>(tuning.innerRadarRadius) * i) / steps;
        const int16_t y = CyberpunkRadarTheme::centerY - radius;
        if (y < 22)
        {
            continue;
        }

        char label[8];
        snprintf(label, sizeof(label), "%.0f", (maxRangeKm * i) / steps);
        const int16_t x = CyberpunkRadarTheme::centerX - 4;
        const int16_t width = canvas.textWidth(label, 1);
        canvas.fillRect(x - width - 2, y - 4, width + 4, 9, cyberBackgroundColor());
        canvas.drawString(label, x, y, 1);
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

    LabelRect usedLabels[kMaxTrackedLabels];
    uint8_t usedLabelCount = 0;
    uint8_t shownLabelCount = 0;
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
        if (config.showLabels &&
            shouldShowAircraftLabel(aircraft[i],
                                    i,
                                    selectedAircraftIndex,
                                    aircraftCount,
                                    shownLabelCount,
                                    cyberpunkTuning().maxLabels))
        {
            const uint8_t before = usedLabelCount;
            drawCyberpunkAircraftLabel(canvas,
                                       aircraft[i],
                                       x,
                                       y,
                                       selected,
                                       usedLabels,
                                       usedLabelCount);
            if (usedLabelCount != before)
            {
                ++shownLabelCount;
            }
        }
    }
}

void RadarRenderer::drawCyberpunkAircraftSymbol(TFT_eSprite &canvas,
                                                const Aircraft &target,
                                                int16_t x,
                                                int16_t y,
                                                bool selected)
{
    const float headingDeg = isfinite(target.headingDeg) ? target.headingDeg : target.bearingDeg;
    drawCyberpunkAircraftSilhouette(canvas, x, y, headingDeg, selected);
}

void RadarRenderer::drawCyberpunkAircraftSilhouette(TFT_eSprite &canvas,
                                                    int16_t x,
                                                    int16_t y,
                                                    float headingDeg,
                                                    bool selected)
{
    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    const float scale = tuning.aircraftScale;
    int16_t noseX = 0;
    int16_t noseY = 0;
    int16_t tailX = 0;
    int16_t tailY = 0;
    int16_t bodyX = 0;
    int16_t bodyY = 0;
    int16_t leftWingX = 0;
    int16_t leftWingY = 0;
    int16_t rightWingX = 0;
    int16_t rightWingY = 0;
    int16_t leftTailX = 0;
    int16_t leftTailY = 0;
    int16_t rightTailX = 0;
    int16_t rightTailY = 0;

    rotateCyberpunkAircraftPoint(0.0f, -8.0f, headingDeg, scale, x, y, noseX, noseY);
    rotateCyberpunkAircraftPoint(0.0f, 6.0f, headingDeg, scale, x, y, tailX, tailY);
    rotateCyberpunkAircraftPoint(0.0f, 1.0f, headingDeg, scale, x, y, bodyX, bodyY);
    rotateCyberpunkAircraftPoint(-6.0f, 1.0f, headingDeg, scale, x, y, leftWingX, leftWingY);
    rotateCyberpunkAircraftPoint(6.0f, 1.0f, headingDeg, scale, x, y, rightWingX, rightWingY);
    rotateCyberpunkAircraftPoint(-3.0f, 6.0f, headingDeg, scale, x, y, leftTailX, leftTailY);
    rotateCyberpunkAircraftPoint(3.0f, 6.0f, headingDeg, scale, x, y, rightTailX, rightTailY);

    const uint16_t glow = cyberAircraftGlowColor();
    const uint16_t body = cyberAircraftColor();
    drawModernWideLine(canvas, noseX, noseY, tailX, tailY, glow, 3);
    drawModernWideLine(canvas, bodyX, bodyY, leftWingX, leftWingY, glow, 3);
    drawModernWideLine(canvas, bodyX, bodyY, rightWingX, rightWingY, glow, 3);
    drawModernWideLine(canvas, tailX, tailY, leftTailX, leftTailY, glow, 2);
    drawModernWideLine(canvas, tailX, tailY, rightTailX, rightTailY, glow, 2);

    drawModernWideLine(canvas, noseX, noseY, tailX, tailY, body, 1);
    drawModernWideLine(canvas, bodyX, bodyY, leftWingX, leftWingY, body, 1);
    drawModernWideLine(canvas, bodyX, bodyY, rightWingX, rightWingY, body, 1);
    canvas.drawLine(tailX, tailY, leftTailX, leftTailY, body);
    canvas.drawLine(tailX, tailY, rightTailX, rightTailY, body);
    canvas.fillCircle(noseX, noseY, 1, cyberSelectedColor());

    if (selected)
    {
        canvas.drawCircle(x, y, 9, cyberSelectedColor());
        canvas.drawCircle(x, y, 12, cyberMagentaColor());
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
    const float tailLength = CyberpunkRadarTheme::aircraftTailLength * cyberpunkTuning().aircraftScale + 4.0f;
    const int16_t startX = x - static_cast<int16_t>(sinf(heading) * tailLength);
    const int16_t startY = y + static_cast<int16_t>(cosf(heading) * tailLength);
    const int lineLength = cyberpunkSpeedVectorLengthPx(target.speedMs);
    int16_t endX = startX - static_cast<int16_t>(sinf(heading) * lineLength);
    int16_t endY = startY + static_cast<int16_t>(cosf(heading) * lineLength);
    clipCyberpunkPointToInnerRadar(startX, startY, endX, endY);
    drawModernWideLine(canvas, startX, startY, endX, endY, cyberMagentaColor(), cyberpunkTuning().lineWidth);
}

void RadarRenderer::drawCyberpunkAircraftLabel(TFT_eSprite &canvas,
                                               const Aircraft &target,
                                               int16_t x,
                                               int16_t y,
                                               bool selected,
                                               LabelRect *usedLabels,
                                               uint8_t &usedLabelCount)
{
    char callsign[16];
    char altitude[16];
    char speed[16];
    snprintf(callsign, sizeof(callsign), "%s", target.callsign[0] != '\0' ? target.callsign : "UNKNOWN");
    snprintf(altitude, sizeof(altitude), "%.0fm", target.altitudeM);
    snprintf(speed, sizeof(speed), "%.0fm/s", target.speedMs);

    const CyberpunkRadarTuning &tuning = cyberpunkTuning();
    bool labelRight = x < CyberpunkRadarTheme::centerX;
    const int16_t symbolHalf = static_cast<int16_t>((CyberpunkRadarTheme::aircraftNoseLength +
                                                     CyberpunkRadarTheme::aircraftTailHalfWidth) *
                                                    tuning.aircraftScale);
    const int16_t labelWidth = selected ?
                               max(max(canvas.textWidth(callsign, 1), canvas.textWidth(altitude, 1)),
                                   canvas.textWidth(speed, 1)) :
                               canvas.textWidth(callsign, 1);
    const int16_t blockHeight = selected ? 28 : 9;
    int16_t labelY = constrain(y - blockHeight / 2, 14, CyberpunkRadarTheme::size - blockHeight - 14);
    int16_t anchorX = 0;
    LabelRect rect = {0, 0, 0, 0};
    bool reserved = false;

    for (uint8_t attempt = 0; attempt < 5 && !reserved; ++attempt)
    {
        labelRight = attempt == 3 ? !labelRight : labelRight;
        int16_t candidateY = labelY;
        if (attempt == 1)
        {
            candidateY = constrain(labelY - 12, 14, CyberpunkRadarTheme::size - blockHeight - 14);
        }
        else if (attempt == 2)
        {
            candidateY = constrain(labelY + 12, 14, CyberpunkRadarTheme::size - blockHeight - 14);
        }
        else if (attempt == 4)
        {
            candidateY = constrain(labelY + 22, 14, CyberpunkRadarTheme::size - blockHeight - 14);
        }

        anchorX = labelRight ?
                  x + symbolHalf + tuning.labelGap + 2 :
                  x - symbolHalf - tuning.labelGap - 2;

        if (labelRight)
        {
            anchorX = constrain(anchorX, 10, CyberpunkRadarTheme::size - labelWidth - 12);
            rect = {static_cast<int16_t>(anchorX - 1), static_cast<int16_t>(candidateY - 1), static_cast<int16_t>(labelWidth + 2), static_cast<int16_t>(blockHeight + 2)};
        }
        else
        {
            anchorX = constrain(anchorX, labelWidth + 12, CyberpunkRadarTheme::size - 10);
            rect = {static_cast<int16_t>(anchorX - labelWidth - 1), static_cast<int16_t>(candidateY - 1), static_cast<int16_t>(labelWidth + 2), static_cast<int16_t>(blockHeight + 2)};
        }

        if (reserveLabelRect(usedLabels, usedLabelCount, rect))
        {
            labelY = candidateY;
            reserved = true;
        }
    }

    if (!reserved)
    {
        if (!selected)
        {
            return;
        }
        if (usedLabelCount < kMaxTrackedLabels)
        {
            usedLabels[usedLabelCount++] = rect;
        }
    }

    if (tuning.showLeaderLines)
    {
        canvas.drawLine(x,
                        y,
                        labelRight ? rect.x : static_cast<int16_t>(rect.x + rect.w),
                        static_cast<int16_t>(labelY + 4),
                        selected ? cyberMagentaColor() : cyberAircraftGlowColor());
    }

    if (labelRight)
    {
        canvas.setTextDatum(TL_DATUM);
    }
    else
    {
        canvas.setTextDatum(TR_DATUM);
    }

    canvas.setTextColor(selected ? cyberSelectedColor() : cyberTextColor(), cyberBackgroundColor());
    canvas.drawString(callsign, anchorX, labelY, 1);
    if (selected)
    {
        canvas.setTextColor(cyberAltitudeColor(), cyberBackgroundColor());
        canvas.drawString(altitude, anchorX, labelY + 9, 1);
        canvas.setTextColor(cyberSelectedColor(), cyberBackgroundColor());
        canvas.drawString(speed, anchorX, labelY + 18, 1);
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
        snprintf(status, sizeof(status), "N=%u", aircraftCount);
    }

    const bool looksLikeError = strstr(status, "WIFI") != nullptr ||
                                strstr(status, "AUTH") != nullptr ||
                                strstr(status, "API") != nullptr ||
                                strstr(status, "429") != nullptr ||
                                strstr(status, "ERR") != nullptr ||
                                strstr(status, "NO ") != nullptr;
    if (!cyberpunkTuning().showStatusText && !looksLikeError)
    {
        return;
    }

    char range[16];
    snprintf(range, sizeof(range), "%.0fkm", max(config.maxRangeKm, 1.0f));

    canvas.setTextDatum(BC_DATUM);
    canvas.setTextColor(cyberAltitudeColor(), cyberBackgroundColor());
    char *secondLine = strchr(status, '\n');
    if (secondLine != nullptr)
    {
        *secondLine = '\0';
        ++secondLine;
        canvas.drawString(status, CyberpunkRadarTheme::centerX, 216, 1);
        canvas.drawString(secondLine, CyberpunkRadarTheme::centerX, 228, 1);
    }
    else
    {
        canvas.drawString(status, CyberpunkRadarTheme::centerX, 226, 1);
    }
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

bool RadarRenderer::shouldShowAircraftLabel(const Aircraft &target,
                                            uint8_t index,
                                            uint8_t selectedIndex,
                                            uint8_t aircraftCount,
                                            uint8_t labelShownCount,
                                            uint8_t maxLabels) const
{
    if (!target.valid)
    {
        return false;
    }

    if (index == selectedIndex)
    {
        return true;
    }

    if (labelShownCount >= maxLabels)
    {
        return false;
    }

    if (aircraftCount <= 5)
    {
        return true;
    }

    if (isfinite(target.distanceKm) && target.distanceKm < 12.0f)
    {
        return labelShownCount < maxLabels;
    }

    const uint8_t stride = aircraftCount > 10 ? 3 : 2;
    return (index % stride) == 0;
}

bool RadarRenderer::reserveLabelRect(LabelRect *usedLabels,
                                     uint8_t &usedLabelCount,
                                     const LabelRect &candidate) const
{
    if (usedLabels == nullptr)
    {
        return false;
    }

    for (uint8_t i = 0; i < usedLabelCount; ++i)
    {
        if (labelsOverlap(usedLabels[i], candidate))
        {
            return false;
        }
    }

    if (usedLabelCount >= kMaxTrackedLabels)
    {
        return false;
    }

    usedLabels[usedLabelCount++] = candidate;
    return true;
}

bool RadarRenderer::labelsOverlap(const LabelRect &a, const LabelRect &b) const
{
    constexpr int16_t padding = 2;
    return a.x < b.x + b.w + padding &&
           a.x + a.w + padding > b.x &&
           a.y < b.y + b.h + padding &&
           a.y + a.h + padding > b.y;
}

void RadarRenderer::rotateCyberpunkAircraftPoint(float localX,
                                                 float localY,
                                                 float headingDeg,
                                                 float scale,
                                                 int16_t originX,
                                                 int16_t originY,
                                                 int16_t &screenX,
                                                 int16_t &screenY) const
{
    const float radians = headingDeg * DEG_TO_RAD;
    const float c = cosf(radians);
    const float s = sinf(radians);
    const float x = localX * scale;
    const float y = localY * scale;
    screenX = originX + static_cast<int16_t>(roundf(x * c - y * s));
    screenY = originY + static_cast<int16_t>(roundf(x * s + y * c));
}

bool RadarRenderer::cyberpunkPointInsideOuter(int16_t x, int16_t y) const
{
    const int16_t dx = x - CyberpunkRadarTheme::centerX;
    const int16_t dy = y - CyberpunkRadarTheme::centerY;
    const int16_t radius = cyberpunkTuning().outerRadius - 2;
    return static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy <=
           static_cast<int32_t>(radius) * radius;
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
    renderSystemStatusFrame(line1, line2, line3, UiTheme::ClassicRadar);
}

void RadarRenderer::renderSystemStatusFrame(const char *line1,
                                            const char *line2,
                                            const char *line3,
                                            UiTheme theme)
{
    if (!frameBufferReady_)
    {
        return;
    }

    uint16_t bg = TFT_BLACK;
    uint16_t ring = radarGreen_;
    uint16_t dim = dimGreen_;
    uint16_t title = sweepGreen_;
    uint16_t text = labelGreen_;
    const char *label = "STATUS";

    if (theme == UiTheme::ModernRadar)
    {
        bg = tft_.color565(4, 10, 28);
        ring = tft_.color565(24, 150, 130);
        dim = tft_.color565(12, 70, 90);
        title = tft_.color565(210, 235, 240);
        text = tft_.color565(105, 205, 220);
        label = "MODERN IDLE";
    }
    else if (theme == UiTheme::CyberpunkRadar)
    {
        bg = tft_.color565(5, 3, 18);
        ring = tft_.color565(52, 150, 255);
        dim = tft_.color565(62, 28, 115);
        title = tft_.color565(255, 82, 210);
        text = tft_.color565(86, 205, 255);
        label = "CYBER IDLE";
    }

    frame_.fillSprite(bg);
    frame_.drawCircle(kCenterX, kCenterY, 116, ring);
    frame_.drawCircle(kCenterX, kCenterY, 78, dim);
    frame_.drawCircle(kCenterX, kCenterY, 40, dim);

    if (theme == UiTheme::CyberpunkRadar)
    {
        frame_.drawLine(34, 78, 206, 78, dim);
        frame_.drawLine(34, 162, 206, 162, dim);
        frame_.drawLine(64, 42, 176, 198, dim);
    }
    else if (theme == UiTheme::ModernRadar)
    {
        frame_.drawLine(kCenterX - 84, kCenterY, kCenterX + 84, kCenterY, dim);
        frame_.drawLine(kCenterX, kCenterY - 84, kCenterX, kCenterY + 84, dim);
        frame_.drawCircle(kCenterX, kCenterY, 4, ring);
    }
    else
    {
        frame_.drawLine(kCenterX - 80, kCenterY, kCenterX + 80, kCenterY, dim);
        frame_.drawLine(kCenterX, kCenterY - 80, kCenterX, kCenterY + 80, dim);
    }

    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(text, bg);
    frame_.drawString(label, kCenterX, 54, 1);

    frame_.setTextColor(title, bg);
    frame_.drawString(line1 != nullptr ? line1 : "STATUS", kCenterX, 100, 2);

    frame_.setTextColor(text, bg);
    if (line2 != nullptr && line2[0] != '\0')
    {
        frame_.drawString(line2, kCenterX, 130, 1);
    }
    if (line3 != nullptr && line3[0] != '\0')
    {
        frame_.drawString(line3, kCenterX, 146, 1);
    }

    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderClockFrame(const char *timeText,
                                     const char *dateText,
                                     const char *nextRunText,
                                     const char *hintText)
{
    renderClockFrame(timeText, dateText, nextRunText, hintText, UiTheme::ClassicRadar);
}

void RadarRenderer::renderClockFrame(const char *timeText,
                                     const char *dateText,
                                     const char *nextRunText,
                                     const char *hintText,
                                     UiTheme theme)
{
    if (!frameBufferReady_)
    {
        return;
    }

    uint16_t bg = TFT_BLACK;
    uint16_t ring = dimGreen_;
    uint16_t accent = sweepGreen_;
    uint16_t text = labelGreen_;
    const char *label = "IDLE";

    if (theme == UiTheme::ModernRadar)
    {
        bg = tft_.color565(4, 10, 28);
        ring = tft_.color565(12, 70, 90);
        accent = tft_.color565(190, 230, 235);
        text = tft_.color565(90, 185, 210);
        label = "MODERN";
    }
    else if (theme == UiTheme::CyberpunkRadar)
    {
        bg = tft_.color565(5, 3, 18);
        ring = tft_.color565(60, 28, 118);
        accent = tft_.color565(255, 82, 210);
        text = tft_.color565(86, 205, 255);
        label = "CYBER";
    }

    frame_.fillSprite(bg);
    frame_.drawCircle(kCenterX, kCenterY, 116, ring);
    frame_.drawCircle(kCenterX, kCenterY, 78, ring);

    if (theme == UiTheme::ModernRadar)
    {
        frame_.drawLine(52, kCenterY, 188, kCenterY, ring);
        frame_.drawLine(kCenterX, 52, kCenterX, 188, ring);
    }
    else if (theme == UiTheme::CyberpunkRadar)
    {
        frame_.drawLine(54, 76, 186, 76, ring);
        frame_.drawLine(42, 164, 198, 164, ring);
        frame_.drawRect(74, 88, 92, 46, ring);
    }

    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(text, bg);
    frame_.drawString(label, kCenterX, 52, 1);

    frame_.setTextColor(accent, bg);
    frame_.drawString(timeText != nullptr ? timeText : "--:--", kCenterX, 104, 4);

    frame_.setTextColor(text, bg);
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

void RadarRenderer::renderLocalMenuFrame(const char *title,
                                         const char *const *items,
                                         uint8_t itemCount,
                                         uint8_t selectedIndex,
                                         const char *hint)
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
    frame_.drawCircle(kCenterX, kCenterY, 116, radarGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 78, dimGreen_);

    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(sweepGreen_, TFT_BLACK);
    frame_.drawString(title != nullptr ? title : "MENU", kCenterX, 32, 2);

    const uint8_t visibleCount = itemCount < 5 ? itemCount : 5;
    uint8_t first = 0;
    if (itemCount > visibleCount)
    {
        if (selectedIndex >= 2)
        {
            first = selectedIndex - 2;
        }
        if (first + visibleCount > itemCount)
        {
            first = itemCount - visibleCount;
        }
    }

    for (uint8_t row = 0; row < visibleCount; ++row)
    {
        const uint8_t itemIndex = first + row;
        const int16_t y = 70 + row * 24;
        const bool selected = itemIndex == selectedIndex;
        const uint16_t textColor = selected ? selectedGreen_ : labelGreen_;

        if (selected)
        {
            frame_.fillRoundRect(44, y - 10, 152, 20, 5, dimGreen_);
            frame_.drawRoundRect(44, y - 10, 152, 20, 5, sweepGreen_);
        }

        frame_.setTextColor(textColor, selected ? dimGreen_ : TFT_BLACK);
        frame_.drawString(items != nullptr && itemIndex < itemCount ? items[itemIndex] : "", kCenterX, y, 1);
    }

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString(hint != nullptr ? hint : "UP/DN Move", kCenterX, 198, 1);
    frame_.drawString("UP2 OK DN2 Back", kCenterX, 214, 1);
    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderLocalAdjustFrame(const char *title,
                                           const char *valueText,
                                           const char *hint1,
                                           const char *hint2)
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
    frame_.drawString(title != nullptr ? title : "ADJUST", kCenterX, 54, 2);

    frame_.setTextColor(selectedGreen_, TFT_BLACK);
    frame_.drawString(valueText != nullptr ? valueText : "--", kCenterX, 112, 4);

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString(hint1 != nullptr ? hint1 : "UP/DN Change", kCenterX, 178, 1);
    frame_.drawString(hint2 != nullptr ? hint2 : "UP2 OK DN2 Back", kCenterX, 196, 1);
    frame_.pushSprite(0, 0);
}

void RadarRenderer::renderLocalConfirmFrame(const char *title,
                                            const char *message,
                                            const char *confirmHint,
                                            const char *cancelHint)
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
    frame_.drawCircle(kCenterX, kCenterY, 116, radarGreen_);
    frame_.drawCircle(kCenterX, kCenterY, 78, dimGreen_);

    frame_.setTextDatum(MC_DATUM);
    frame_.setTextColor(sweepGreen_, TFT_BLACK);
    frame_.drawString(title != nullptr ? title : "CONFIRM", kCenterX, 62, 2);

    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString(message != nullptr ? message : "", kCenterX, 112, 1);

    frame_.setTextColor(selectedGreen_, TFT_BLACK);
    frame_.drawString(confirmHint != nullptr ? confirmHint : "UP2 Confirm", kCenterX, 164, 1);
    frame_.setTextColor(labelGreen_, TFT_BLACK);
    frame_.drawString(cancelHint != nullptr ? cancelHint : "DN2 Back", kCenterX, 184, 1);
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

    char status[32];
    snprintf(status, sizeof(status), "%s", statusText);
    char *secondLine = strchr(status, '\n');

    canvas.setTextDatum(BC_DATUM);
    canvas.setTextColor(labelGreen_, TFT_BLACK);
    if (secondLine != nullptr)
    {
        *secondLine = '\0';
        ++secondLine;
        canvas.drawString(status, kCenterX, 216, 1);
        canvas.drawString(secondLine, kCenterX, 228, 1);
    }
    else
    {
        canvas.drawString(status, kCenterX, 226, 1);
    }
}
