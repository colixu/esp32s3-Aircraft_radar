#include "RealRadarTrackManager.h"

#include <math.h>
#include <string.h>

#include "../aircraft/AircraftModel.h"
#include "../app/DebugLog.h"
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

    float lerpFloat(float start, float target, float amount)
    {
        return start + (target - start) * amount;
    }

    float smoothStep(float value)
    {
        if (value < 0.0f)
        {
            return 0.0f;
        }
        if (value > 1.0f)
        {
            return 1.0f;
        }

        return value * value * (3.0f - 2.0f * value);
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
            const uint8_t updateResult = updateTrackFromApi(tracks_[index], source, settings, now, false);
            if (updateResult == 1)
            {
                ++stats.jumpResetCount;
            }
            else if (updateResult == 2)
            {
                ++stats.correctionStartedCount;
            }
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

        if (track.lastApiUpdateMs == 0)
        {
            continue;
        }

        uint32_t elapsedSinceApiMs = now - track.lastApiUpdateMs;
        if (elapsedSinceApiMs > settings.prediction.predictionMaxMs)
        {
            elapsedSinceApiMs = settings.prediction.predictionMaxMs;
            track.stale = true;
        }

        float predictedLat = track.apiLat;
        float predictedLon = track.apiLon;
        const bool canPredict = settings.prediction.enabled &&
                                !track.onGround &&
                                track.apiSpeedMs >= settings.prediction.lowSpeedThresholdMs;

        if (canPredict)
        {
            GeoPredict::predictLatLon(track.apiLat,
                                      track.apiLon,
                                      track.apiSpeedMs,
                                      track.apiHeadingDeg,
                                      static_cast<float>(elapsedSinceApiMs) / 1000.0f,
                                      predictedLat,
                                      predictedLon);
        }

        if (track.correctionActive)
        {
            const uint32_t correctionElapsedMs = now - track.correctionStartMs;
            const float correctionT = track.correctionDurationMs > 0 ?
                                      static_cast<float>(correctionElapsedMs) / static_cast<float>(track.correctionDurationMs) :
                                      1.0f;
            const float easedT = smoothStep(correctionT);
            track.displayLat = lerpFloat(track.correctionStartLat, predictedLat, easedT);
            track.displayLon = lerpFloat(track.correctionStartLon, predictedLon, easedT);
            track.displayAltitudeM = lerpFloat(track.correctionStartAltitudeM, track.apiAltitudeM, easedT);
            track.displaySpeedMs += (track.apiSpeedMs - track.displaySpeedMs) * settings.prediction.followAlpha;
            track.displayHeadingDeg = smoothAngle(track.correctionStartHeadingDeg, track.apiHeadingDeg, easedT);

            if (correctionT >= 1.0f)
            {
                track.correctionActive = false;
            }
        }
        else
        {
            track.displayLat += (predictedLat - track.displayLat) * settings.prediction.followAlpha;
            track.displayLon += (predictedLon - track.displayLon) * settings.prediction.followAlpha;
            track.displayAltitudeM += (track.apiAltitudeM - track.displayAltitudeM) * settings.prediction.followAlpha;
            track.displaySpeedMs += (track.apiSpeedMs - track.displaySpeedMs) * settings.prediction.followAlpha;
            track.displayHeadingDeg = smoothAngle(track.displayHeadingDeg,
                                                  track.apiHeadingDeg,
                                                  settings.prediction.followAlpha);
        }
        track.lastPredictMs = now;
    }
}

void RealRadarTrackManager::printPredictionSummary(const UserSettings &settings, uint32_t now) const
{
    uint8_t activeCount = 0;
    uint8_t staleCount = 0;
    uint8_t correctionCount = 0;
    const TrackedAircraft *firstTrack = nullptr;

    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        const TrackedAircraft &track = tracks_[i];
        if (!track.valid)
        {
            continue;
        }

        if (track.stale)
        {
            ++staleCount;
        }
        else
        {
            ++activeCount;
        }

        if (track.correctionActive)
        {
            ++correctionCount;
        }

        if (firstTrack == nullptr)
        {
            firstTrack = &track;
        }
    }

    DebugLog::println("Prediction summary:");
    DebugLog::printf("  active=%u stale=%u correction=%u prediction=%u\r\n",
                     activeCount,
                     staleCount,
                     correctionCount,
                     settings.prediction.enabled ? 1 : 0);

    if (firstTrack == nullptr)
    {
        DebugLog::println("  no tracks");
        return;
    }

    float apiDistanceKm = 0.0f;
    float apiBearingDeg = 0.0f;
    float displayDistanceKm = 0.0f;
    float displayBearingDeg = 0.0f;
    GeoUtils::geoToRadar(settings.location.centerLat,
                         settings.location.centerLon,
                         firstTrack->apiLat,
                         firstTrack->apiLon,
                         apiDistanceKm,
                         apiBearingDeg);
    GeoUtils::geoToRadar(settings.location.centerLat,
                         settings.location.centerLon,
                         firstTrack->displayLat,
                         firstTrack->displayLon,
                         displayDistanceKm,
                         displayBearingDeg);

    DebugLog::printf("  first=%s api=%.1fkm/%.0f display=%.1fkm/%.0f elapsed=%lus\r\n",
                     firstTrack->callsign[0] != '\0' ? firstTrack->callsign : firstTrack->icao24,
                     apiDistanceKm,
                     apiBearingDeg,
                     displayDistanceKm,
                     displayBearingDeg,
                     static_cast<unsigned long>((now - firstTrack->lastApiUpdateMs) / 1000));
}

uint8_t RealRadarTrackManager::buildAircraft(const UserSettings &settings,
                                             Aircraft *aircraft,
                                             uint8_t aircraftCapacity,
                                             RealRadarTrackStats &stats) const
{
    AircraftModel::clearAircraft(aircraft, aircraftCapacity);
    uint8_t aircraftCount = 0;
    const float displayRangeKm = settings.location.maxRangeKm;
    const float aircraftRangeKm = effectiveFetchRangeKm(settings);

    for (uint8_t i = 0; i < kMaxTracks; ++i)
    {
        const TrackedAircraft &track = tracks_[i];
        if (!track.valid)
        {
            continue;
        }

        float distanceKm = 0.0f;
        float bearingDeg = 0.0f;
        if (!GeoUtils::geoToRadar(settings.location.centerLat,
                                  settings.location.centerLon,
                                  track.displayLat,
                                  track.displayLon,
                                  distanceKm,
                                  bearingDeg))
        {
            continue;
        }

        if (distanceKm > aircraftRangeKm)
        {
            ++stats.filteredRange;
            continue;
        }

        if (!settings.filter.showGroundTraffic)
        {
            if (track.onGround)
            {
                ++stats.filteredGround;
                continue;
            }

            if (track.displayAltitudeM < settings.filter.minAirborneAltitudeM)
            {
                ++stats.filteredAltitude;
                continue;
            }

            if (track.displaySpeedMs < settings.filter.minAirborneSpeedMs)
            {
                ++stats.filteredSpeed;
                continue;
            }
        }

        if (distanceKm > displayRangeKm)
        {
            ++stats.edgeDotCandidates;
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

uint8_t RealRadarTrackManager::updateTrackFromApi(TrackedAircraft &track,
                                                  const ApiAircraft &source,
                                                  const UserSettings &settings,
                                                  uint32_t now,
                                                  bool isNewTrack)
{
    const bool hadDisplayPosition = track.valid;
    const uint32_t previousApiUpdateMs = track.lastApiUpdateMs;
    const uint32_t apiIntervalMs = previousApiUpdateMs > 0 ? now - previousApiUpdateMs : 0;
    const float previousDisplayLat = track.displayLat;
    const float previousDisplayLon = track.displayLon;
    const float previousDisplayAltitudeM = track.displayAltitudeM;
    const float previousDisplayHeadingDeg = track.displayHeadingDeg;

    strncpy(track.icao24, source.icao24, sizeof(track.icao24) - 1);
    track.icao24[sizeof(track.icao24) - 1] = '\0';
    strncpy(track.callsign, source.callsign, sizeof(track.callsign) - 1);
    track.callsign[sizeof(track.callsign) - 1] = '\0';
    strncpy(track.type, source.type, sizeof(track.type) - 1);
    track.type[sizeof(track.type) - 1] = '\0';

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
        track.correctionActive = false;
        track.lastPredictMs = now;
        return 0;
    }

    float apiDisplayErrorKm = 0.0f;
    float ignoredBearingDeg = 0.0f;
    if (GeoUtils::geoToRadar(previousDisplayLat,
                             previousDisplayLon,
                             source.lat,
                             source.lon,
                             apiDisplayErrorKm,
                             ignoredBearingDeg))
    {
        if (apiDisplayErrorKm > settings.prediction.jumpResetDistanceKm)
        {
            track.displayLat = source.lat;
            track.displayLon = source.lon;
            track.displayAltitudeM = source.altitudeM;
            track.displaySpeedMs = source.speedMs;
            track.displayHeadingDeg = source.headingDeg;
            track.correctionActive = false;
            track.lastPredictMs = now;
            DebugLog::printf("  jump reset: %s err=%.1fkm\r\n",
                             track.callsign[0] != '\0' ? track.callsign : track.icao24,
                             apiDisplayErrorKm);
            return 1;
        }

        if (settings.prediction.correctionEnabled &&
            apiIntervalMs >= settings.prediction.correctionMinApiIntervalMs &&
            apiDisplayErrorKm >= settings.prediction.correctionStartDistanceKm)
        {
            track.correctionActive = true;
            track.correctionStartMs = now;
            track.correctionDurationMs = settings.prediction.correctionDurationMs;
            track.correctionStartLat = previousDisplayLat;
            track.correctionStartLon = previousDisplayLon;
            track.correctionStartAltitudeM = previousDisplayAltitudeM;
            track.correctionStartHeadingDeg = previousDisplayHeadingDeg;
            DebugLog::printf("  correction start: %s err=%.1fkm duration=%lums\r\n",
                             track.callsign[0] != '\0' ? track.callsign : track.icao24,
                             apiDisplayErrorKm,
                             static_cast<unsigned long>(track.correctionDurationMs));
            return 2;
        }
    }

    track.correctionActive = false;
    return 0;
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
        if (ageMs > settings.prediction.staleTimeoutMs)
        {
            memset(&track, 0, sizeof(track));
            continue;
        }

        if (ageMs > settings.prediction.staleTimeoutMs / 2)
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
    strncpy(aircraft[insertIndex].type, track.type, sizeof(aircraft[insertIndex].type) - 1);
    aircraft[insertIndex].type[sizeof(aircraft[insertIndex].type) - 1] = '\0';

    while (insertIndex > 0 &&
           aircraft[insertIndex].distanceKm < aircraft[insertIndex - 1].distanceKm)
    {
        const Aircraft temp = aircraft[insertIndex - 1];
        aircraft[insertIndex - 1] = aircraft[insertIndex];
        aircraft[insertIndex] = temp;
        --insertIndex;
    }
}
