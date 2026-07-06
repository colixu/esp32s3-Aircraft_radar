#include "GeoUtils.h"

#include <math.h>

#include "../aircraft/AircraftModel.h"

namespace GeoUtils
{
    namespace
    {
        constexpr float kEarthRadiusKm = 6371.0f;

        bool isValidLatLon(float lat, float lon)
        {
            return isfinite(lat) &&
                   isfinite(lon) &&
                   lat >= -90.0f &&
                   lat <= 90.0f &&
                   lon >= -180.0f &&
                   lon <= 180.0f;
        }
    }

    bool geoToRadar(float centerLat,
                    float centerLon,
                    float targetLat,
                    float targetLon,
                    float &distanceKm,
                    float &bearingDeg)
    {
        if (!isValidLatLon(centerLat, centerLon) || !isValidLatLon(targetLat, targetLon))
        {
            distanceKm = 0.0f;
            bearingDeg = 0.0f;
            return false;
        }

        const float lat1 = centerLat * DEG_TO_RAD;
        const float lat2 = targetLat * DEG_TO_RAD;
        const float dLat = (targetLat - centerLat) * DEG_TO_RAD;
        const float dLon = (targetLon - centerLon) * DEG_TO_RAD;

        const float sinHalfLat = sinf(dLat * 0.5f);
        const float sinHalfLon = sinf(dLon * 0.5f);
        const float a = sinHalfLat * sinHalfLat +
                        cosf(lat1) * cosf(lat2) * sinHalfLon * sinHalfLon;
        const float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
        distanceKm = kEarthRadiusKm * c;

        const float y = sinf(dLon) * cosf(lat2);
        const float x = cosf(lat1) * sinf(lat2) -
                        sinf(lat1) * cosf(lat2) * cosf(dLon);
        bearingDeg = AircraftModel::wrapDegrees(atan2f(y, x) * RAD_TO_DEG);
        return true;
    }
}
