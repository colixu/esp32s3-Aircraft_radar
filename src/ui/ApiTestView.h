#pragma once

#include <TFT_eSPI.h>

#include "../app/WifiManagerSimple.h"
#include "../data/OpenSkyProvider.h"

class ApiTestView
{
public:
    explicit ApiTestView(TFT_eSPI &display);

    void begin();
    bool isReady() const;
    void render(const WifiManagerSimple &wifi, const OpenSkyProvider &openSky);

private:
    TFT_eSPI &tft_;
    TFT_eSprite frame_;
    bool frameBufferReady_ = false;

    uint16_t green_;
    uint16_t dimGreen_;
    uint16_t whiteGreen_;
    uint16_t errorRed_;

    void initDisplay();
    void initFrameBuffer();
    void initColors();
    void drawHeader(const WifiManagerSimple &wifi, const OpenSkyProvider &openSky);
    void drawAircraftList(const OpenSkyProvider &openSky);
};
