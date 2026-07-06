#include "RadarApp.h"

#include "DebugLog.h"
#include "../utils/GeoUtils.h"

RadarApp::RadarApp() :
    settingsStore_(config_),
    tft_(),
    renderer_(tft_),
    apiTestView_(tft_)
{
}

void RadarApp::begin()
{
    DebugLog::begin(115200);
    delay(800);
    DebugLog::println();
    DebugLog::println("ESP32-S3 GC9A01 aircraft radar");
    settingsStore_.begin();
    settingsStore_.load(settings_);
    inputManager_.begin(settings_.uiButtonPin);

    if (config_.appMode == AppMode::ApiTest)
    {
        beginApiTest();
        return;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        beginRealRadar();
        return;
    }

    beginRadarDemo();
}

void RadarApp::update()
{
    const uint32_t now = millis();
    updateInput();

    if (config_.appMode == AppMode::ApiTest)
    {
        updateApiTest(now);
        return;
    }

    if (config_.appMode == AppMode::RealRadar)
    {
        updateRealRadar(now);
        return;
    }

    updateRadarDemo(now);
}

void RadarApp::beginRadarDemo()
{
    DebugLog::println("Starting RadarDemo mode.");
    dataProvider_.begin();
    renderer_.begin();
    renderFrame();

    DebugLog::println("Fake aircraft radar UI is running.");
}

void RadarApp::beginApiTest()
{
    DebugLog::println("Starting ApiTest mode.");
    DebugLog::println("Logs use the upload USB/UART port at 115200 baud.");
    apiTestView_.begin();
    wifi_.begin();
    renderApiTestScreen();
    printApiTestSerialStatus();
}

void RadarApp::beginRealRadar()
{
    DebugLog::println("Starting RealRadar mode.");
    DebugLog::printf("Radar center: lat=%.5f lon=%.5f range=%.0fkm\r\n",
                     settings_.radarCenterLat,
                     settings_.radarCenterLon,
                     settings_.maxRangeKm);
    DebugLog::printf("RealRadar filters: ground=%s minAlt=%.0fm minSpeed=%.1fm/s\r\n",
                     settings_.showGroundTraffic ? "show" : "hide",
                     settings_.minAirborneAltitudeM,
                     settings_.minAirborneSpeedMs);
    DebugLog::printf("OpenSky bbox: lat %.4f..%.4f lon %.4f..%.4f\r\n",
                     config_.openSkyLamin,
                     config_.openSkyLamax,
                     config_.openSkyLomin,
                     config_.openSkyLomax);

    AircraftModel::clearAircraft(realAircraft_, AircraftModel::kAircraftCount);
    realAircraftCount_ = 0;
    updateRealRadarStatus();

    renderer_.begin();
    wifi_.begin();
    renderRealRadarFrame();
}

void RadarApp::updateInput()
{
    inputManager_.update();

    if (inputManager_.wasUiSwitchPressed())
    {
        switchUiTheme();
    }
}

void RadarApp::switchUiTheme()
{
    settings_.uiTheme = nextUiTheme(settings_.uiTheme);
    DebugLog::printf("UI theme switched: %s\r\n", uiThemeName(settings_.uiTheme));
    settingsStore_.save(settings_);

    if (config_.appMode == AppMode::RealRadar)
    {
        renderRealRadarFrame();
    }
}

void RadarApp::updateRadarDemo(uint32_t now)
{
    updateAircraftData(now);
    updateSelectedAircraftForList(dataProvider_.aircraft(), dataProvider_.count(), now);

    if (now - lastFrameMs_ < config_.frameIntervalMs)
    {
        return;
    }
    lastFrameMs_ = now;

    renderFrame();
    renderer_.advanceSweep(config_.sweepStepDeg);
}

void RadarApp::updateApiTest(uint32_t now)
{
    wifi_.update(now, config_.wifiReconnectIntervalMs);

    if (wifi_.isConnected() &&
        (lastApiRequestMs_ == 0 || now - lastApiRequestMs_ >= settings_.apiRequestIntervalMs))
    {
        lastApiRequestMs_ = now;
        openSky_.requestStates(config_);
        printApiTestSerialStatus();
    }

    if (now - lastApiSerialMs_ >= config_.apiSerialStatusIntervalMs)
    {
        lastApiSerialMs_ = now;
        printApiTestSerialStatus();
    }

    if (now - lastApiScreenMs_ < config_.apiScreenRefreshMs)
    {
        return;
    }

    lastApiScreenMs_ = now;
    renderApiTestScreen();
}

void RadarApp::updateRealRadar(uint32_t now)
{
    wifi_.update(now, config_.wifiReconnectIntervalMs);

    if (wifi_.isConnected() &&
        (lastApiRequestMs_ == 0 || now - lastApiRequestMs_ >= config_.apiRequestIntervalMs))
    {
        lastApiRequestMs_ = now;
        requestRealTraffic();
    }

    updateSelectedAircraftForList(realAircraft_, realAircraftCount_, now);

    if (now - lastFrameMs_ < config_.frameIntervalMs)
    {
        return;
    }

    lastFrameMs_ = now;
    updateRealRadarStatus();
    renderRealRadarFrame();
    renderer_.advanceSweep(config_.sweepStepDeg);
}

void RadarApp::updateAircraftData(uint32_t now)
{
    if (now - lastAircraftUpdateMs_ < config_.aircraftUpdateIntervalMs)
    {
        return;
    }
    lastAircraftUpdateMs_ = now;
    dataProvider_.update();
}

void RadarApp::updateSelectedAircraft(uint32_t now)
{
    updateSelectedAircraftForList(dataProvider_.aircraft(), dataProvider_.count(), now);
}

void RadarApp::updateSelectedAircraftForList(const Aircraft *aircraft, uint8_t aircraftCount, uint32_t now)
{
    if (aircraftCount == 0 || AircraftModel::countValid(aircraft, aircraftCount) == 0)
    {
        selectedAircraftIndex_ = 0;
        return;
    }

    if (selectedAircraftIndex_ >= aircraftCount || !aircraft[selectedAircraftIndex_].valid)
    {
        const int8_t firstValid = AircraftModel::firstValidIndex(aircraft, aircraftCount);
        selectedAircraftIndex_ = firstValid >= 0 ? static_cast<uint8_t>(firstValid) : 0;
        lastSelectionMs_ = now;
        return;
    }

    if (now - lastSelectionMs_ < config_.selectionIntervalMs)
    {
        return;
    }
    lastSelectionMs_ = now;

    for (uint8_t attempts = 0; attempts < aircraftCount; ++attempts)
    {
        selectedAircraftIndex_ = (selectedAircraftIndex_ + 1) % aircraftCount;
        if (aircraft[selectedAircraftIndex_].valid)
        {
            return;
        }
    }
}

void RadarApp::renderFrame()
{
    renderer_.renderRadarFrame(dataProvider_.aircraft(),
                               dataProvider_.count(),
                               selectedAircraftIndex_,
                               config_,
                               settings_.uiTheme);
}

void RadarApp::renderRealRadarFrame()
{
    renderer_.renderRadarFrame(realAircraft_,
                               realAircraftCount_,
                               selectedAircraftIndex_,
                               config_,
                               settings_.uiTheme,
                               realRadarStatus_);
}

void RadarApp::renderApiTestScreen()
{
    apiTestView_.render(wifi_, openSky_);
}

void RadarApp::requestRealTraffic()
{
    DebugLog::println("RealRadar API update:");
    const bool requestOk = openSky_.requestStates(config_);

    DebugLog::printf("  OpenSky raw states=%u valid lat/lon=%u stored=%u\r\n",
                     openSky_.rawStateCount(),
                     openSky_.validPositionCount(),
                     openSky_.aircraftCount());

    if (requestOk || openSky_.httpStatusCode() == 200)
    {
        convertApiAircraftToRadar();
    }
    else
    {
        DebugLog::printf("  API update failed, keeping previous radar data. status=%s\r\n",
                         openSky_.lastError());
    }

    updateRealRadarStatus();
}

void RadarApp::convertApiAircraftToRadar()
{
    AircraftModel::clearAircraft(realAircraft_, AircraftModel::kAircraftCount);
    realAircraftCount_ = 0;
    RealRadarFilterStats stats;

    const ApiAircraft *apiAircraft = openSky_.aircraft();
    for (uint8_t i = 0; i < openSky_.aircraftCount(); ++i)
    {
        const ApiAircraft &source = apiAircraft[i];
        if (!source.valid)
        {
            continue;
        }

        float distanceKm = 0.0f;
        float bearingDeg = 0.0f;
        if (!GeoUtils::geoToRadar(settings_.radarCenterLat,
                                  settings_.radarCenterLon,
                                  source.lat,
                                  source.lon,
                                  distanceKm,
                                  bearingDeg))
        {
            continue;
        }

        if (distanceKm > settings_.maxRangeKm)
        {
            ++stats.filteredRange;
            continue;
        }

        if (!settings_.showGroundTraffic)
        {
            if (source.onGround)
            {
                ++stats.filteredGround;
                continue;
            }

            if (source.altitudeM < settings_.minAirborneAltitudeM)
            {
                ++stats.filteredAltitude;
                continue;
            }

            if (source.speedMs < settings_.minAirborneSpeedMs)
            {
                ++stats.filteredSpeed;
                continue;
            }
        }

        const char *displayName = source.callsign[0] != '\0' ? source.callsign : source.icao24;
        if (displayName[0] == '\0')
        {
            continue;
        }

        addRealAircraftSorted(source, displayName, distanceKm, bearingDeg);
    }

    printRealRadarFilterSummary(stats);

    const int8_t firstValid = AircraftModel::firstValidIndex(realAircraft_, realAircraftCount_);
    selectedAircraftIndex_ = firstValid >= 0 ? static_cast<uint8_t>(firstValid) : 0;
}

void RadarApp::addRealAircraftSorted(const ApiAircraft &source,
                                     const char *displayName,
                                     float distanceKm,
                                     float bearingDeg)
{
    if (realAircraftCount_ >= AircraftModel::kAircraftCount &&
        distanceKm >= realAircraft_[realAircraftCount_ - 1].distanceKm)
    {
        return;
    }

    uint8_t insertIndex = realAircraftCount_;
    if (realAircraftCount_ < AircraftModel::kAircraftCount)
    {
        ++realAircraftCount_;
    }
    else
    {
        insertIndex = AircraftModel::kAircraftCount - 1;
    }

    AircraftModel::setAircraft(realAircraft_[insertIndex],
                               displayName,
                               distanceKm,
                               bearingDeg,
                               source.altitudeM,
                               source.speedMs,
                               source.headingDeg,
                               true);
    realAircraftLat_[insertIndex] = source.lat;
    realAircraftLon_[insertIndex] = source.lon;
    realAircraftOnGround_[insertIndex] = source.onGround;

    while (insertIndex > 0 &&
           realAircraft_[insertIndex].distanceKm < realAircraft_[insertIndex - 1].distanceKm)
    {
        const Aircraft aircraftTemp = realAircraft_[insertIndex - 1];
        realAircraft_[insertIndex - 1] = realAircraft_[insertIndex];
        realAircraft_[insertIndex] = aircraftTemp;

        const float latTemp = realAircraftLat_[insertIndex - 1];
        realAircraftLat_[insertIndex - 1] = realAircraftLat_[insertIndex];
        realAircraftLat_[insertIndex] = latTemp;

        const float lonTemp = realAircraftLon_[insertIndex - 1];
        realAircraftLon_[insertIndex - 1] = realAircraftLon_[insertIndex];
        realAircraftLon_[insertIndex] = lonTemp;

        const bool onGroundTemp = realAircraftOnGround_[insertIndex - 1];
        realAircraftOnGround_[insertIndex - 1] = realAircraftOnGround_[insertIndex];
        realAircraftOnGround_[insertIndex] = onGroundTemp;

        --insertIndex;
    }
}

void RadarApp::printRealRadarFilterSummary(const RealRadarFilterStats &stats)
{
    DebugLog::println("RealRadar filter summary:");
    DebugLog::printf("  raw=%u\r\n", openSky_.rawStateCount());
    DebugLog::printf("  valid_pos=%u\r\n", openSky_.validPositionCount());
    DebugLog::printf("  filtered_ground=%u\r\n", stats.filteredGround);
    DebugLog::printf("  filtered_altitude=%u\r\n", stats.filteredAltitude);
    DebugLog::printf("  filtered_speed=%u\r\n", stats.filteredSpeed);
    DebugLog::printf("  filtered_range=%u\r\n", stats.filteredRange);
    DebugLog::printf("  radar=%u\r\n", realAircraftCount_);

    if (realAircraftCount_ == 0)
    {
        DebugLog::println("  no airborne aircraft after filter");
        return;
    }

    for (uint8_t i = 0; i < realAircraftCount_; ++i)
    {
        const Aircraft &target = realAircraft_[i];
        DebugLog::printf("  radar #%u %-11s lat=%.5f lon=%.5f dist=%.1fkm bearing=%.0f alt=%.0fm speed=%.1fm/s hdg=%.0f onGround=%u\r\n",
                         i + 1,
                         target.callsign,
                         realAircraftLat_[i],
                         realAircraftLon_[i],
                         target.distanceKm,
                         target.bearingDeg,
                         target.altitudeM,
                         target.speedMs,
                         target.headingDeg,
                         realAircraftOnGround_[i] ? 1 : 0);
    }
}

void RadarApp::updateRealRadarStatus()
{
    if (!wifi_.isConnected())
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "WIFI LOST");
        return;
    }

    if (openSky_.httpStatusCode() == 429)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "HTTP 429");
        return;
    }

    if (openSky_.httpStatusCode() != 0 && openSky_.httpStatusCode() != 200)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "API ERROR");
        return;
    }

    if (openSky_.httpStatusCode() == 0)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "NO DATA");
        return;
    }

    if (realAircraftCount_ == 0)
    {
        snprintf(realRadarStatus_, sizeof(realRadarStatus_), "NO AIRBORNE");
        return;
    }

    const uint32_t ageSec = openSky_.lastSuccessMs() > 0 ?
                            (millis() - openSky_.lastSuccessMs()) / 1000 :
                            0;
    snprintf(realRadarStatus_, sizeof(realRadarStatus_), "LIVE N=%u %lus", realAircraftCount_, static_cast<unsigned long>(ageSec));
}

void RadarApp::printApiTestSerialStatus()
{
    DebugLog::println("API test status:");
    DebugLog::printf("  Query box: lat %.4f..%.4f lon %.4f..%.4f\r\n",
                     config_.openSkyLamin,
                     config_.openSkyLamax,
                     config_.openSkyLomin,
                     config_.openSkyLomax);
    DebugLog::printf("  WiFi=%s RSSI=%d\r\n", wifi_.statusText(), wifi_.rssi());
    DebugLog::printf("  HTTP=%d bytes=%lu planes=%u status=%s\r\n",
                     openSky_.httpStatusCode(),
                     static_cast<unsigned long>(openSky_.payloadLength()),
                     openSky_.aircraftCount(),
                     openSky_.lastError());

    if (openSky_.lastSuccessMs() > 0)
    {
        DebugLog::printf("  Last success: %lus ago\r\n",
                         static_cast<unsigned long>((millis() - openSky_.lastSuccessMs()) / 1000));
    }
    else
    {
        DebugLog::println("  Last success: never");
    }

    const ApiAircraft *aircraft = openSky_.aircraft();
    const uint8_t count = min<uint8_t>(openSky_.aircraftCount(), 3);
    if (count == 0)
    {
        DebugLog::println("  No aircraft parsed yet.");
        return;
    }

    for (uint8_t i = 0; i < count; ++i)
    {
        DebugLog::printf("  #%u %s lat=%.5f lon=%.5f alt=%.0fm speed=%.1fm/s hdg=%.0f\r\n",
                         i + 1,
                         aircraft[i].callsign,
                         aircraft[i].lat,
                         aircraft[i].lon,
                         aircraft[i].altitudeM,
                         aircraft[i].speedMs,
                         aircraft[i].headingDeg);
    }
}
