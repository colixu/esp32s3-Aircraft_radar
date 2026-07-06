#include "RealRadarTrackManager.h"

#include <math.h>
#include <string.h>

#include "../aircraft/AircraftModel.h"
#include "../utils/GeoPredict.h"
#include "../utils/GeoUtils.h"

namespace
{
    float smoothAngle(float currentDeg, float targetDeg, float alpha)
    {
        float delta = AircraftModel::wrapDegrees(targetDeg) - AircraftModel::wrapDegrees(currentDeg);
        if (delta > 180.0f)
        {
            delta -= 360.0f;
        }
        else if (delta < -180.0f)
        {
            delta += 360.0f;
        }
        return AircraftModel::wrapDegrees(currentDeg + delta * alpha);
    }
}

void RealRadarTrackManager::begin()
{
    memset(tracks_, 0, sizeof(tracks_));
}

void RealRadarTrackManager::mergeSnapshot(const OpenSkySnapshot &snapshot,
                                          const UserSettings &settings,
                                          uint32_t now,
                                          RealRadarTrackStats &stats)
{
    stats.rawStateCount = snapshot.rawStateCount;
    stats.validPositionCount = snapshot.validPositionCount;

    for (uint8_t i = 0; i < snapshot.aircraftCount; ++i)
    {
        const ApiAircraft &source = snapshot.aircraft[i];
        if (!source.valid || source.icao24[0] == '\0')
        {
            continue;
        }

        int8_t index = findTrackByIcao24(source.icao24);
        if (index >= 0)
        {
            updateTrackFromApi(tracks_[index], source, settings, now, false);
            ++stats.matchedTracks;
            continue;
        }

        index = findFreeTrack();
        if (index < 0)
        {
            continue;
        }

        updateTrackFromApi(tracks_[index], source, settings, now, true);
        ++stats.newTracks;
    }

    pruneStaleTracks(settings, now);

    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        if (!tracks_[i].valid)
        {
            continue;
        }

        if (tracks_[i].stale)
        {
            ++stats.staleTracks;
        }
        else
        {
            ++stats.activeTracks;
        }
    }
}

void RealRadarTrackManager::updatePrediction(const UserSettings &settings, uint32_t now)
{
    pruneStaleTracks(settings, now);

    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        TrackedAircraft &track = tracks_[i];
        if (!track.valid)
        {
            continue;
        }

        if (track.lastPredictMs == 0)
        {
            track.lastPredictMs = now;
            continue;
        }

        uint32_t deltaMs = now - track.lastPredictMs;
        if (deltaMs > settings.predictionMaxDeltaMs)
        {
            deltaMs = settings.predictionMaxDeltaMs;
        }
        track.lastPredictMs = now;

        float predictedLat = track.displayLat;
        float predictedLon = track.displayLon;
        if (!track.onGround && track.displaySpeedMs > settings.minAirborneSpeedMs * 0.25f)
        {
            GeoPredict::predictLatLon(track.displayLat,
                                      track.displayLon,
                                      track.displaySpeedMs,
                                      track.displayHeadingDeg,
                                      static_cast<float>(deltaMs) / 1000.0f,
                                      predictedLat,
                                      predictedLon);
        }

        track.displayLat = predictedLat;
        track.displayLon = predictedLon;

        if (!track.stale)
        {
            track.displayLat += (track.apiLat - track.displayLat) * settings.predictionCorrectionAlpha;
            track.displayLon += (track.apiLon - track.displayLon) * settings.predictionCorrectionAlpha;
            track.displayAltitudeM += (track.apiAltitudeM - track.displayAltitudeM) * settings.predictionCorrectionAlpha;
            track.displaySpeedMs += (track.apiSpeedMs - track.displaySpeedMs) * settings.predictionCorrectionAlpha;
            track.displayHeadingDeg = smoothAngle(track.displayHeadingDeg,
                                                  track.apiHeadingDeg,
                                                  settings.predictionCorrectionAlpha);
        }
    }
}

uint8_t RealRadarTrackManager::buildAircraft(const UserSettings &settings,
                                             Aircraft *aircraft,
                                             uint8_t aircraftCapacity,
                                             RealRadarTrackStats &stats) const
{
    AircraftModel::clearAircraft(aircraft, aircraftCapacity);
    uint8_t aircraftCount = 0;

    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        const TrackedAircraft &track = tracks_[i];
        if (!track.valid)
        {
            continue;
        }

        float distanceKm = 0.0f;
        float bearingDeg = 0.0f;
        if (!GeoUtils::geoToRadar(settings.radarCenterLat,
                                  settings.radarCenterLon,
                                  track.displayLat,
                                  track.displayLon,
                                  distanceKm,
                                  bearingDeg))
        {
            continue;
        }

        if (distanceKm > settings.maxRangeKm)
        {
            ++stats.filteredRange;
            continue;
        }

        if (!settings.showGroundTraffic)
        {
            if (track.onGround)
            {
                ++stats.filteredGround;
                continue;
            }

            if (track.displayAltitudeM < settings.minAirborneAltitudeM)
            {
                ++stats.filteredAltitude;
                continue;
            }

            if (track.displaySpeedMs < settings.minAirborneSpeedMs)
            {
                ++stats.filteredSpeed;
                continue;
            }
        }

        addAircraftSorted(aircraft, aircraftCapacity, aircraftCount, track, distanceKm, bearingDeg);
    }

    stats.renderedAircraftCount = aircraftCount;
    return aircraftCount;
}

int8_t RealRadarTrackManager::findTrackByIcao24(const char *icao24) const
{
    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        if (tracks_[i].valid && strncmp(tracks_[i].icao24, icao24, sizeof(tracks_[i].icao24)) == 0)
        {
            return i;
        }
    }
    return -1;
}

int8_t RealRadarTrackManager::findFreeTrack() const
{
    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        if (!tracks_[i].valid)
        {
            return i;
        }
    }
    return -1;
}

void RealRadarTrackManager::updateTrackFromApi(TrackedAircraft &track,
                                               const ApiAircraft &source,
                                               const UserSettings &settings,
                                               uint32_t now,
                                               bool isNewTrack)
{
    const bool hadDisplayPosition = track.valid;
    strncpy(track.icao24, source.icao24, sizeof(track.icao24) - 1);
    track.icao24[sizeof(track.icao24) - 1] = '\0';
    strncpy(track.callsign, source.callsign, sizeof(track.callsign) - 1);
    track.callsign[sizeof(track.callsign) - 1] = '\0';

    track.apiLat = source.lat;
    track.apiLon = source.lon;
    track.apiAltitudeM = source.altitudeM;
    track.apiSpeedMs = source.speedMs;
    track.apiHeadingDeg = source.headingDeg;
    track.onGround = source.onGround;
    track.lastApiUpdateMs = now;
    track.lastSeenMs = now;
    track.stale = false;
    track.valid = true;

    if (isNewTrack || !hadDisplayPosition)
    {
        track.displayLat = source.lat;
        track.displayLon = source.lon;
        track.displayAltitudeM = source.altitudeM;
        track.displaySpeedMs = source.speedMs;
        track.displayHeadingDeg = source.headingDeg;
        track.lastPredictMs = now;
        return;
    }

    float snapDistanceKm = 0.0f;
    float ignoredBearingDeg = 0.0f;
    if (GeoUtils::geoToRadar(track.displayLat,
                             track.displayLon,
                             source.lat,
                             source.lon,
                             snapDistanceKm,
                             ignoredBearingDeg) &&
        snapDistanceKm > settings.predictionResetDistanceKm)
    {
        track.displayLat = source.lat;
        track.displayLon = source.lon;
        track.displayAltitudeM = source.altitudeM;
        track.displaySpeedMs = source.speedMs;
        track.displayHeadingDeg = source.headingDeg;
        track.lastPredictMs = now;
    }
}

void RealRadarTrackManager::pruneStaleTracks(const UserSettings &settings, uint32_t now)
{
    (void)settings;

    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        TrackedAircraft &track = tracks_[i];
        if (!track.valid)
        {
            continue;
        }

        const uint32_t ageMs = now - track.lastSeenMs;
        if (ageMs > settings.staleTimeoutMs)
        {
            memset(&track, 0, sizeof(track));
            continue;
        }

        if (ageMs > settings.staleGraceMs)
        {
            track.stale = true;
        }
    }
}

void RealRadarTrackManager::addAircraftSorted(Aircraft *aircraft,
                                              uint8_t aircraftCapacity,
                                              uint8_t &aircraftCount,
                                              const TrackedAircraft &track,
                                              float distanceKm,
                                              float bearingDeg) const
{
    if (aircraftCount >= aircraftCapacity && distanceKm >= aircraft[aircraftCount - 1].distanceKm)
    {
        return;
    }

    uint8_t insertIndex = aircraftCount;
    if (aircraftCount < aircraftCapacity)
    {
        ++aircraftCount;
    }
    else
    {
        insertIndex = aircraftCapacity - 1;
    }

    const char *displayName = track.callsign[0] != '\0' ? track.callsign : track.icao24;
    AircraftModel::setAircraft(aircraft[insertIndex],
                               displayName,
                               distanceKm,
                               bearingDeg,
                               track.displayAltitudeM,
                               track.displaySpeedMs,
                               track.displayHeadingDeg,
                               true);

    while (insertIndex > 0 &&
           aircraft[insertIndex].distanceKm < aircraft[insertIndex - 1].distanceKm)
    {
        const Aircraft temp = aircraft[insertIndex - 1];
        aircraft[insertIndex - 1] = aircraft[insertIndex];
        aircraft[insertIndex] = temp;
        --insertIndex;
    }
}
