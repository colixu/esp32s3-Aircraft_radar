#include "ApiTestView.h"

#include "../app/DebugLog.h"

ApiTestView::ApiTestView(TFT_eSPI &display) :
    tft_(display),
    frame_(&display)
{
}

void ApiTestView::begin()
{
    initColors();
    initDisplay();
    initFrameBuffer();
}

bool ApiTestView::isReady() const
{
    return frameBufferReady_;
}

void ApiTestView::render(const WifiManagerSimple &wifi, const OpenSkyProvider &openSky)
{
    if (!frameBufferReady_)
    {
        return;
    }

    frame_.fillSprite(TFT_BLACK);
    drawHeader(wifi, openSky);
    drawAircraftList(openSky);
    frame_.pushSprite(0, 0);
}

void ApiTestView::initDisplay()
{
    DebugLog::println("Initializing GC9A01 TFT for API test...");
    tft_.init();
    tft_.setRotation(0);
    tft_.invertDisplay(true);
    tft_.fillScreen(TFT_BLACK);
}

void ApiTestView::initFrameBuffer()
{
    DebugLog::println("Creating API test 240x240 16-bit sprite frame buffer...");
    frame_.setColorDepth(16);
    void *buffer = frame_.createSprite(240, 240);

    if (buffer != nullptr)
    {
        frameBufferReady_ = true;
        DebugLog::println("API test 16-bit sprite frame buffer ready.");
        return;
    }

    DebugLog::println("API test 16-bit sprite failed, trying 8-bit sprite...");
    frame_.deleteSprite();
    frame_.setColorDepth(8);
    buffer = frame_.createSprite(240, 240);

    if (buffer != nullptr)
    {
        frameBufferReady_ = true;
        DebugLog::println("API test 8-bit sprite frame buffer ready.");
        return;
    }

    frameBufferReady_ = false;
    DebugLog::println("ERROR: API test sprite frame buffer allocation failed.");
}

void ApiTestView::initColors()
{
    green_ = tft_.color565(20, 235, 95);
    dimGreen_ = tft_.color565(0, 150, 58);
    whiteGreen_ = tft_.color565(175, 255, 190);
    errorRed_ = tft_.color565(255, 60, 60);
}

void ApiTestView::drawHeader(const WifiManagerSimple &wifi, const OpenSkyProvider &openSky)
{
    frame_.setTextDatum(TC_DATUM);
    frame_.setTextColor(whiteGreen_, TFT_BLACK);
    frame_.drawString("API TEST", 120, 26, 2);

    frame_.setTextColor(dimGreen_, TFT_BLACK);
    frame_.drawString("OpenSky /states/all", 120, 46, 1);

    frame_.setTextDatum(TL_DATUM);

    char line[36];
    snprintf(line, sizeof(line), "WiFi %-7s RSSI %d", wifi.statusText(), wifi.rssi());
    frame_.setTextColor(wifi.isConnected() ? green_ : errorRed_, TFT_BLACK);
    frame_.drawString(line, 48, 68, 1);

    snprintf(line, sizeof(line), "HTTP: %d", openSky.httpStatusCode());
    frame_.setTextColor(openSky.httpStatusCode() == 200 ? green_ : errorRed_, TFT_BLACK);
    frame_.drawString(line, 48, 82, 1);

    snprintf(line, sizeof(line), "Bytes: %lu", static_cast<unsigned long>(openSky.payloadLength()));
    frame_.setTextColor(dimGreen_, TFT_BLACK);
    frame_.drawString(line, 48, 96, 1);

    snprintf(line, sizeof(line), "Planes: %u", openSky.aircraftCount());
    frame_.setTextColor(green_, TFT_BLACK);
    frame_.drawString(line, 48, 110, 1);

    if (openSky.lastSuccessMs() > 0)
    {
        const uint32_t ageSec = (millis() - openSky.lastSuccessMs()) / 1000;
        snprintf(line, sizeof(line), "Age: %lus", static_cast<unsigned long>(ageSec));
    }
    else
    {
        snprintf(line, sizeof(line), "Age: never");
    }
    frame_.setTextColor(dimGreen_, TFT_BLACK);
    frame_.drawString(line, 48, 124, 1);

    snprintf(line, sizeof(line), "Msg: %.21s", openSky.lastError());
    frame_.setTextColor(openSky.httpStatusCode() == 200 ? dimGreen_ : errorRed_, TFT_BLACK);
    frame_.drawString(line, 48, 138, 1);
}

void ApiTestView::drawAircraftList(const OpenSkyProvider &openSky)
{
    const ApiAircraft *aircraft = openSky.aircraft();
    const uint8_t count = min<uint8_t>(openSky.aircraftCount(), 3);

    frame_.setTextDatum(TL_DATUM);
    frame_.setTextColor(whiteGreen_, TFT_BLACK);
    frame_.drawString("FIRST AIRCRAFT", 54, 156, 1);

    if (count == 0)
    {
        frame_.setTextColor(dimGreen_, TFT_BLACK);
        frame_.drawString("No valid lat/lon yet", 54, 172, 1);
        return;
    }

    char line[42];
    for (uint8_t i = 0; i < count; ++i)
    {
        snprintf(line,
                 sizeof(line),
                 "%u %-8s %.0fm %.0f",
                 i + 1,
                 aircraft[i].callsign,
                 aircraft[i].altitudeM,
                 aircraft[i].speedMs);
        frame_.setTextColor(i == 0 ? green_ : dimGreen_, TFT_BLACK);
        frame_.drawString(line, 54, 172 + i * 12, 1);
    }

    const ApiAircraft &first = aircraft[0];
    snprintf(line, sizeof(line), "LAT %.3f", first.lat);
    frame_.setTextColor(dimGreen_, TFT_BLACK);
    frame_.drawString(line, 64, 210, 1);

    snprintf(line, sizeof(line), "LON %.3f", first.lon);
    frame_.drawString(line, 64, 222, 1);
}
