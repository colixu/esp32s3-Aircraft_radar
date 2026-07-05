#include "FakeDataProvider.h"

namespace
{
    const float kBearingRateDeg[AircraftModel::kAircraftCount] = {0.55f, -0.32f, 0.24f, -0.42f, 0.36f, -0.20f};

    void polarToPlane(float bearingDeg, float distanceKm, float &x, float &y)
    {
        const float radians = bearingDeg * DEG_TO_RAD;
        x = sinf(radians) * distanceKm;
        y = cosf(radians) * distanceKm;
    }

    float headingFromMovement(float previousBearingDeg,
                              float previousDistanceKm,
                              float nextBearingDeg,
                              float nextDistanceKm,
                              float fallbackHeadingDeg)
    {
        float previousX = 0.0f;
        float previousY = 0.0f;
        float nextX = 0.0f;
        float nextY = 0.0f;

        polarToPlane(previousBearingDeg, previousDistanceKm, previousX, previousY);
        polarToPlane(nextBearingDeg, nextDistanceKm, nextX, nextY);

        const float dx = nextX - previousX;
        const float dy = nextY - previousY;
        if (fabsf(dx) < 0.0001f && fabsf(dy) < 0.0001f)
        {
            return fallbackHeadingDeg;
        }

        return AircraftModel::wrapDegrees(atan2f(dx, dy) / DEG_TO_RAD);
    }
}

void FakeDataProvider::begin()
{
    AircraftModel::setAircraft(aircraft_[0], "CES1234", 24.0f, 18.0f, 4200.0f, 168.0f, 192.0f, true);
    AircraftModel::setAircraft(aircraft_[1], "ANA789", 46.0f, 76.0f, 9100.0f, 236.0f, 248.0f, true);
    AircraftModel::setAircraft(aircraft_[2], "JAL456", 62.0f, 138.0f, 8400.0f, 221.0f, 315.0f, true);
    AircraftModel::setAircraft(aircraft_[3], "CCA998", 38.0f, 221.0f, 6800.0f, 205.0f, 42.0f, true);
    AircraftModel::setAircraft(aircraft_[4], "CSN320", 72.0f, 302.0f, 10300.0f, 244.0f, 118.0f, true);
    AircraftModel::setAircraft(aircraft_[5], "HXA217", 86.0f, 342.0f, 7600.0f, 190.0f, 172.0f, true);
}

void FakeDataProvider::update()
{
    for (uint8_t i = 0; i < AircraftModel::kAircraftCount; ++i)
    {
        if (!aircraft_[i].valid)
        {
            continue;
        }

        const float previousBearingDeg = aircraft_[i].bearingDeg;
        const float previousDistanceKm = aircraft_[i].distanceKm;

        aircraft_[i].bearingDeg = AircraftModel::wrapDegrees(aircraft_[i].bearingDeg + kBearingRateDeg[i]);
        aircraft_[i].distanceKm += distanceRateKm_[i];

        if (aircraft_[i].distanceKm < 12.0f || aircraft_[i].distanceKm > 96.0f)
        {
            aircraft_[i].distanceKm = constrain(aircraft_[i].distanceKm, 12.0f, 96.0f);
            distanceRateKm_[i] = -distanceRateKm_[i];
        }

        aircraft_[i].headingDeg = headingFromMovement(previousBearingDeg,
                                                      previousDistanceKm,
                                                      aircraft_[i].bearingDeg,
                                                      aircraft_[i].distanceKm,
                                                      aircraft_[i].headingDeg);
    }
}

const Aircraft *FakeDataProvider::aircraft() const
{
    return aircraft_;
}

Aircraft *FakeDataProvider::aircraft()
{
    return aircraft_;
}

uint8_t FakeDataProvider::count() const
{
    return AircraftModel::kAircraftCount;
}
