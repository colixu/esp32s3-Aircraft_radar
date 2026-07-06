#pragma once

#include <Arduino.h>

#include "../aircraft/AircraftModel.h"
#include "../app/UserSettings.h"
#include "OpenSkyAsyncUpdater.h"

struct TrackedAircraft
{
    char icao24[8];
    char callsign[12];

    float apiLat = 0.0f;
    float apiLon = 0.0f;
    float apiAltitudeM = 0.0f;
    float apiSpeedMs = 0.0f;
    float apiHeadingDeg = 0.0f;
    bool onGround = false;

    float displayLat = 0.0f;
    float displayLon = 0.0f;
    float displayAltitudeM = 0.0f;
    float displaySpeedMs = 0.0f;
    float displayHeadingDeg = 0.0f;

    uint32_t lastApiUpdateMs = 0;
    uint32_t lastSeenMs = 0;
    uint32_t lastPredictMs = 0;

    bool valid = false;
    bool stale = false;
};

struct RealRadarTrackStats
{
    uint16_t rawStateCount = 0;
    uint16_t validPositionCount = 0;
    uint16_t filteredGround = 0;
    uint16_t filteredAltitude = 0;
    uint16_t filteredSpeed = 0;
    uint16_t filteredRange = 0;
    uint8_t matchedTracks = 0;
    uint8_t newTracks = 0;
    uint8_t staleTracks = 0;
    uint8_t activeTracks = 0;
    uint8_t renderedAircraftCount = 0;
};

class RealRadarTrackManager
{
public:
    static constexpr uint8_t kMaxTracks = 32;

    void begin();
    void mergeSnapshot(const OpenSkySnapshot &snapshot,
                       const UserSettings &settings,
                       uint32_t now,
                       RealRadarTrackStats &stats);
    void updatePrediction(const UserSettings &settings, uint32_t now);
    uint8_t buildAircraft(const UserSettings &settings,
                          Aircraft *aircraft,
                          uint8_t aircraftCapacity,
                          RealRadarTrackStats &stats) const;

private:
    TrackedAircraft tracks_[kMaxTracks];

    int8_t findTrackByIcao24(const char *icao24) const;
    int8_t findFreeTrack() const;
    void updateTrackFromApi(TrackedAircraft &track,
                            const ApiAircraft &source,
                            const UserSettings &settings,
                            uint32_t now,
                            bool isNewTrack);
    void pruneStaleTracks(const UserSettings &settings, uint32_t now);
    void addAircraftSorted(Aircraft *aircraft,
                           uint8_t aircraftCapacity,
                           uint8_t &aircraftCount,
                           const TrackedAircraft &track,
                           float distanceKm,
                           float bearingDeg) const;
};
