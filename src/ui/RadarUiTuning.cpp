#include "RadarUiTuning.h"

#include "../app/DebugLog.h"

namespace
{
    template <typename T>
    T clampValue(T value, T minValue, T maxValue)
    {
        if (value < minValue)
        {
            return minValue;
        }
        if (value > maxValue)
        {
            return maxValue;
        }
        return value;
    }
}

void loadDefaultRadarUiTuning(RadarUiTuning &tuning)
{
    tuning.modern.background = {0, 0, 0};
    tuning.modern.grid = {16, 100, 32};
    tuning.modern.text = {210, 220, 220};
    tuning.modern.aircraft = {255, 0, 0};
    tuning.modern.vector = {255, 0, 255};
    tuning.modern.altitudeText = {90, 200, 255};
    tuning.modern.selected = {255, 255, 255};

    tuning.modern.globalBrightness = 1.0f;
    tuning.modern.backgroundBrightness = 1.0f;
    tuning.modern.gridBrightness = 1.0f;
    tuning.modern.textBrightness = 1.0f;

    tuning.modern.outerRadius = 107;
    tuning.modern.ringCount = 4;
    tuning.modern.lineWidth = 2;
    tuning.modern.centerDotRadius = 2;

    tuning.modern.aircraftScale = 1.0f;
    tuning.modern.vectorScale = 1.0f;
    tuning.modern.labelGap = 0;

    tuning.cyberpunk.background = {1, 4, 12};
    tuning.cyberpunk.backgroundNoise = {0, 40, 80};
    tuning.cyberpunk.outerRing = {0, 220, 255};
    tuning.cyberpunk.ring = {0, 120, 200};
    tuning.cyberpunk.ringDim = {0, 45, 90};
    tuning.cyberpunk.crosshair = {0, 210, 255};
    tuning.cyberpunk.tick = {0, 180, 255};
    tuning.cyberpunk.magenta = {255, 0, 220};
    tuning.cyberpunk.aircraft = {255, 95, 45};
    tuning.cyberpunk.aircraftGlow = {255, 40, 20};
    tuning.cyberpunk.text = {180, 240, 255};
    tuning.cyberpunk.altitudeText = {80, 210, 255};
    tuning.cyberpunk.selected = {255, 255, 255};
    tuning.cyberpunk.sweep = {0, 200, 255};

    tuning.cyberpunk.globalBrightness = 1.0f;
    tuning.cyberpunk.ringBrightness = 1.0f;
    tuning.cyberpunk.textBrightness = 1.0f;
    tuning.cyberpunk.aircraftBrightness = 1.0f;
    tuning.cyberpunk.sweepBrightness = 1.0f;

    tuning.cyberpunk.outerRadius = 114;
    tuning.cyberpunk.innerRadarRadius = 102;
    tuning.cyberpunk.ringCount = 5;
    tuning.cyberpunk.lineWidth = 1;
    tuning.cyberpunk.majorTickLength = 8;
    tuning.cyberpunk.minorTickLength = 4;
    tuning.cyberpunk.centerDotRadius = 2;

    tuning.cyberpunk.aircraftScale = 1.0f;
    tuning.cyberpunk.vectorScale = 1.0f;
    tuning.cyberpunk.sweepWidth = 3.0f;
    tuning.cyberpunk.sweepTrailStrength = 0.75f;
    tuning.cyberpunk.labelGap = 3;

    sanitizeRadarUiTuning(tuning);
}

void sanitizeRadarUiTuning(RadarUiTuning &tuning)
{
    tuning.modern.globalBrightness = clampValue(tuning.modern.globalBrightness, 0.0f, 2.0f);
    tuning.modern.backgroundBrightness = clampValue(tuning.modern.backgroundBrightness, 0.0f, 2.0f);
    tuning.modern.gridBrightness = clampValue(tuning.modern.gridBrightness, 0.0f, 2.0f);
    tuning.modern.textBrightness = clampValue(tuning.modern.textBrightness, 0.0f, 2.0f);

    tuning.modern.outerRadius = clampValue<uint8_t>(tuning.modern.outerRadius, 60, 118);
    tuning.modern.ringCount = clampValue<uint8_t>(tuning.modern.ringCount, 1, 6);
    tuning.modern.lineWidth = clampValue<uint8_t>(tuning.modern.lineWidth, 1, 4);
    tuning.modern.centerDotRadius = clampValue<uint8_t>(tuning.modern.centerDotRadius, 1, 6);

    tuning.modern.aircraftScale = clampValue(tuning.modern.aircraftScale, 0.4f, 2.0f);
    tuning.modern.vectorScale = clampValue(tuning.modern.vectorScale, 0.0f, 3.0f);
    tuning.modern.labelGap = clampValue<int8_t>(tuning.modern.labelGap, -8, 16);

    tuning.cyberpunk.globalBrightness = clampValue(tuning.cyberpunk.globalBrightness, 0.0f, 2.0f);
    tuning.cyberpunk.ringBrightness = clampValue(tuning.cyberpunk.ringBrightness, 0.0f, 2.0f);
    tuning.cyberpunk.textBrightness = clampValue(tuning.cyberpunk.textBrightness, 0.0f, 2.0f);
    tuning.cyberpunk.aircraftBrightness = clampValue(tuning.cyberpunk.aircraftBrightness, 0.0f, 2.0f);
    tuning.cyberpunk.sweepBrightness = clampValue(tuning.cyberpunk.sweepBrightness, 0.0f, 2.0f);

    tuning.cyberpunk.outerRadius = clampValue<uint8_t>(tuning.cyberpunk.outerRadius, 80, 118);
    tuning.cyberpunk.innerRadarRadius = clampValue<uint8_t>(tuning.cyberpunk.innerRadarRadius, 60, tuning.cyberpunk.outerRadius);
    tuning.cyberpunk.ringCount = clampValue<uint8_t>(tuning.cyberpunk.ringCount, 1, 6);
    tuning.cyberpunk.lineWidth = clampValue<uint8_t>(tuning.cyberpunk.lineWidth, 1, 4);
    tuning.cyberpunk.majorTickLength = clampValue<uint8_t>(tuning.cyberpunk.majorTickLength, 1, 16);
    tuning.cyberpunk.minorTickLength = clampValue<uint8_t>(tuning.cyberpunk.minorTickLength, 1, 16);
    tuning.cyberpunk.centerDotRadius = clampValue<uint8_t>(tuning.cyberpunk.centerDotRadius, 1, 6);

    tuning.cyberpunk.aircraftScale = clampValue(tuning.cyberpunk.aircraftScale, 0.4f, 2.0f);
    tuning.cyberpunk.vectorScale = clampValue(tuning.cyberpunk.vectorScale, 0.0f, 3.0f);
    tuning.cyberpunk.sweepWidth = clampValue(tuning.cyberpunk.sweepWidth, 1.0f, 8.0f);
    tuning.cyberpunk.sweepTrailStrength = clampValue(tuning.cyberpunk.sweepTrailStrength, 0.0f, 1.5f);
    tuning.cyberpunk.labelGap = clampValue<int8_t>(tuning.cyberpunk.labelGap, -8, 16);
}

void printRadarUiTuning(const RadarUiTuning &tuning)
{
    const ModernRadarTuning &modern = tuning.modern;
    DebugLog::println("ModernRadarTuning:");
    DebugLog::printf("  background = {%u, %u, %u}\r\n", modern.background.r, modern.background.g, modern.background.b);
    DebugLog::printf("  grid = {%u, %u, %u}\r\n", modern.grid.r, modern.grid.g, modern.grid.b);
    DebugLog::printf("  text = {%u, %u, %u}\r\n", modern.text.r, modern.text.g, modern.text.b);
    DebugLog::printf("  aircraft = {%u, %u, %u}\r\n", modern.aircraft.r, modern.aircraft.g, modern.aircraft.b);
    DebugLog::printf("  vector = {%u, %u, %u}\r\n", modern.vector.r, modern.vector.g, modern.vector.b);
    DebugLog::printf("  altitudeText = {%u, %u, %u}\r\n", modern.altitudeText.r, modern.altitudeText.g, modern.altitudeText.b);
    DebugLog::printf("  selected = {%u, %u, %u}\r\n", modern.selected.r, modern.selected.g, modern.selected.b);
    DebugLog::printf("  globalBrightness = %.2f\r\n", modern.globalBrightness);
    DebugLog::printf("  backgroundBrightness = %.2f\r\n", modern.backgroundBrightness);
    DebugLog::printf("  gridBrightness = %.2f\r\n", modern.gridBrightness);
    DebugLog::printf("  textBrightness = %.2f\r\n", modern.textBrightness);
    DebugLog::printf("  outerRadius = %u\r\n", modern.outerRadius);
    DebugLog::printf("  ringCount = %u\r\n", modern.ringCount);
    DebugLog::printf("  lineWidth = %u\r\n", modern.lineWidth);
    DebugLog::printf("  centerDotRadius = %u\r\n", modern.centerDotRadius);
    DebugLog::printf("  aircraftScale = %.2f\r\n", modern.aircraftScale);
    DebugLog::printf("  vectorScale = %.2f\r\n", modern.vectorScale);
    DebugLog::printf("  labelGap = %d\r\n", modern.labelGap);

    const CyberpunkRadarTuning &cyber = tuning.cyberpunk;
    DebugLog::println("CyberpunkRadarTuning:");
    DebugLog::printf("  background = {%u, %u, %u}\r\n", cyber.background.r, cyber.background.g, cyber.background.b);
    DebugLog::printf("  backgroundNoise = {%u, %u, %u}\r\n", cyber.backgroundNoise.r, cyber.backgroundNoise.g, cyber.backgroundNoise.b);
    DebugLog::printf("  outerRing = {%u, %u, %u}\r\n", cyber.outerRing.r, cyber.outerRing.g, cyber.outerRing.b);
    DebugLog::printf("  ring = {%u, %u, %u}\r\n", cyber.ring.r, cyber.ring.g, cyber.ring.b);
    DebugLog::printf("  ringDim = {%u, %u, %u}\r\n", cyber.ringDim.r, cyber.ringDim.g, cyber.ringDim.b);
    DebugLog::printf("  crosshair = {%u, %u, %u}\r\n", cyber.crosshair.r, cyber.crosshair.g, cyber.crosshair.b);
    DebugLog::printf("  tick = {%u, %u, %u}\r\n", cyber.tick.r, cyber.tick.g, cyber.tick.b);
    DebugLog::printf("  magenta = {%u, %u, %u}\r\n", cyber.magenta.r, cyber.magenta.g, cyber.magenta.b);
    DebugLog::printf("  aircraft = {%u, %u, %u}\r\n", cyber.aircraft.r, cyber.aircraft.g, cyber.aircraft.b);
    DebugLog::printf("  aircraftGlow = {%u, %u, %u}\r\n", cyber.aircraftGlow.r, cyber.aircraftGlow.g, cyber.aircraftGlow.b);
    DebugLog::printf("  text = {%u, %u, %u}\r\n", cyber.text.r, cyber.text.g, cyber.text.b);
    DebugLog::printf("  altitudeText = {%u, %u, %u}\r\n", cyber.altitudeText.r, cyber.altitudeText.g, cyber.altitudeText.b);
    DebugLog::printf("  selected = {%u, %u, %u}\r\n", cyber.selected.r, cyber.selected.g, cyber.selected.b);
    DebugLog::printf("  sweep = {%u, %u, %u}\r\n", cyber.sweep.r, cyber.sweep.g, cyber.sweep.b);
    DebugLog::printf("  globalBrightness = %.2f\r\n", cyber.globalBrightness);
    DebugLog::printf("  ringBrightness = %.2f\r\n", cyber.ringBrightness);
    DebugLog::printf("  textBrightness = %.2f\r\n", cyber.textBrightness);
    DebugLog::printf("  aircraftBrightness = %.2f\r\n", cyber.aircraftBrightness);
    DebugLog::printf("  sweepBrightness = %.2f\r\n", cyber.sweepBrightness);
    DebugLog::printf("  outerRadius = %u\r\n", cyber.outerRadius);
    DebugLog::printf("  innerRadarRadius = %u\r\n", cyber.innerRadarRadius);
    DebugLog::printf("  ringCount = %u\r\n", cyber.ringCount);
    DebugLog::printf("  lineWidth = %u\r\n", cyber.lineWidth);
    DebugLog::printf("  majorTickLength = %u\r\n", cyber.majorTickLength);
    DebugLog::printf("  minorTickLength = %u\r\n", cyber.minorTickLength);
    DebugLog::printf("  centerDotRadius = %u\r\n", cyber.centerDotRadius);
    DebugLog::printf("  aircraftScale = %.2f\r\n", cyber.aircraftScale);
    DebugLog::printf("  vectorScale = %.2f\r\n", cyber.vectorScale);
    DebugLog::printf("  sweepWidth = %.2f\r\n", cyber.sweepWidth);
    DebugLog::printf("  sweepTrailStrength = %.2f\r\n", cyber.sweepTrailStrength);
    DebugLog::printf("  labelGap = %d\r\n", cyber.labelGap);
}
