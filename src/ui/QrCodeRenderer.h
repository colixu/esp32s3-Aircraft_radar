#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace QrCodeRenderer
{
    bool drawWifiQr(TFT_eSprite &canvas,
                    const char *ssid,
                    const char *password,
                    int16_t centerX,
                    int16_t topY,
                    int16_t maxSize);
}
