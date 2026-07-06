#include "GeoPredict.h"

#include <math.h>

namespace GeoPredict
{
    bool predictLatLon(float lat,
                       float lon,
                       float speedMs,
                       float headingDeg,
                       float deltaSeconds,
                       float &outLat,
                       float &outLon)
    {
        if (!isfinite(lat) || !isfinite(lon) || !isfinite(speedMs) || !isfinite(headingDeg))
        {
            outLat = lat;
            outLon = lon;
            return false;
        }

        if (deltaSeconds <= 0.0f || speedMs <= 0.0f)
        {
            outLat = lat;
            outLon = lon;
            return true;
        }

        const float distanceKm = speedMs * deltaSeconds / 1000.0f;
        const float headingRad = headingDeg * DEG_TO_RAD;
        const float deltaNorthKm = cosf(headingRad) * distanceKm;
        const float deltaEastKm = sinf(headingRad) * distanceKm;
        const float kmPerLatDeg = 111.32f;
        const float latRad = lat * DEG_TO_RAD;
        const float kmPerLonDeg = kmPerLatDeg * cosf(latRad);

        outLat = lat + deltaNorthKm / kmPerLatDeg;
        if (fabsf(kmPerLonDeg) < 0.001f)
        {
            outLon = lon;
            return true;
        }

        outLon = lon + deltaEastKm / kmPerLonDeg;
        return true;
    }
}
