#pragma once

#include <Arduino.h>

struct RgbColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct ModernRadarTuning
{
    RgbColor background;
    RgbColor grid;
    RgbColor text;
    RgbColor aircraft;
    RgbColor vector;
    RgbColor altitudeText;
    RgbColor selected;

    float globalBrightness;
    float backgroundBrightness;
    float gridBrightness;
    float textBrightness;

    uint8_t outerRadius;
    uint8_t ringCount;
    uint8_t lineWidth;
    uint8_t centerDotRadius;

    float aircraftScale;
    float vectorScale;
    int8_t labelGap;
};

struct CyberpunkRadarTuning
{
    RgbColor background;
    RgbColor backgroundNoise;
    RgbColor outerRing;
    RgbColor ring;
    RgbColor ringDim;
    RgbColor crosshair;
    RgbColor tick;
    RgbColor magenta;
    RgbColor aircraft;
    RgbColor aircraftGlow;
    RgbColor text;
    RgbColor altitudeText;
    RgbColor selected;
    RgbColor sweep;

    float globalBrightness;
    float ringBrightness;
    float textBrightness;
    float aircraftBrightness;
    float sweepBrightness;

    uint8_t outerRadius;
    uint8_t innerRadarRadius;
    uint8_t ringCount;
    uint8_t lineWidth;
    uint8_t majorTickLength;
    uint8_t minorTickLength;
    uint8_t centerDotRadius;

    float aircraftScale;
    float vectorScale;
    float sweepWidth;
    float sweepTrailStrength;
    int8_t labelGap;
};

struct RadarUiTuning
{
    ModernRadarTuning modern;
    CyberpunkRadarTuning cyberpunk;
};

void loadDefaultRadarUiTuning(RadarUiTuning &tuning);
void sanitizeRadarUiTuning(RadarUiTuning &tuning);
void printRadarUiTuning(const RadarUiTuning &tuning);
