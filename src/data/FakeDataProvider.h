#pragma once

#include "../aircraft/AircraftModel.h"

class FakeDataProvider
{
public:
    void begin();
    void update();

    const Aircraft *aircraft() const;
    Aircraft *aircraft();
    uint8_t count() const;

private:
    Aircraft aircraft_[AircraftModel::kAircraftCount];
    float distanceRateKm_[AircraftModel::kAircraftCount] = {-0.18f, 0.12f, -0.08f, 0.16f, -0.11f, 0.09f};
};
