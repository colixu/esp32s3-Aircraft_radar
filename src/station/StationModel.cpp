#include "StationModel.h"

#include <math.h>
#include <string.h>

#include "../utils/GeoUtils.h"

namespace StationModel
{
    namespace
    {
        struct StationDefinition
        {
            const char *code;
            float lat;
            float lon;
        };

        constexpr StationDefinition kStations[] =
        {
            {"HND", 35.549393f, 139.779839f},
            {"NRT", 35.764722f, 140.386389f},
            {"IBR", 36.181111f, 140.414722f},
            {"FSZ", 34.796111f, 138.189444f},
            {"NGO", 34.858333f, 136.805278f},
            {"ITM", 34.785528f, 135.438222f},
            {"KIX", 34.427222f, 135.244167f},
            {"CTS", 42.775200f, 141.692283f},
            {"FUK", 33.585942f, 130.450686f},
            {"OKA", 26.195814f, 127.645869f}
        };

        constexpr uint8_t kStationCount = sizeof(kStations) / sizeof(kStations[0]);

        void clearStations(RadarStation *stations, uint8_t stationCapacity)
        {
            for (uint8_t i = 0; i < stationCapacity; ++i)
            {
                memset(&stations[i], 0, sizeof(stations[i]));
            }
        }

        void insertStationSorted(RadarStation *stations,
                                 uint8_t stationCapacity,
                                 uint8_t &stationCount,
                                 const RadarStation &candidate)
        {
            if (stationCapacity == 0)
            {
                return;
            }

            if (stationCount >= stationCapacity &&
                candidate.distanceKm >= stations[stationCount - 1].distanceKm)
            {
                return;
            }

            uint8_t insertIndex = stationCount;
            if (stationCount < stationCapacity)
            {
                ++stationCount;
            }
            else
            {
                insertIndex = stationCapacity - 1;
            }

            stations[insertIndex] = candidate;
            while (insertIndex > 0 &&
                   stations[insertIndex].distanceKm < stations[insertIndex - 1].distanceKm)
            {
                const RadarStation temp = stations[insertIndex - 1];
                stations[insertIndex - 1] = stations[insertIndex];
                stations[insertIndex] = temp;
                --insertIndex;
            }
        }
    }

    uint8_t buildVisibleStations(float centerLat,
                                 float centerLon,
                                 float maxRangeKm,
                                 RadarStation *stations,
                                 uint8_t stationCapacity)
    {
        if (stations == nullptr || stationCapacity == 0 || !isfinite(maxRangeKm) || maxRangeKm <= 0.0f)
        {
            return 0;
        }

        clearStations(stations, stationCapacity);

        uint8_t visibleCount = 0;
        for (uint8_t i = 0; i < kStationCount; ++i)
        {
            float distanceKm = 0.0f;
            float bearingDeg = 0.0f;
            if (!GeoUtils::geoToRadar(centerLat,
                                      centerLon,
                                      kStations[i].lat,
                                      kStations[i].lon,
                                      distanceKm,
                                      bearingDeg))
            {
                continue;
            }

            if (distanceKm > maxRangeKm)
            {
                continue;
            }

            RadarStation station;
            memset(&station, 0, sizeof(station));
            strncpy(station.code, kStations[i].code, sizeof(station.code) - 1);
            station.distanceKm = distanceKm;
            station.bearingDeg = bearingDeg;
            station.valid = true;
            insertStationSorted(stations, stationCapacity, visibleCount, station);
        }

        return visibleCount;
    }
}
