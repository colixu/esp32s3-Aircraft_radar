#include <Arduino.h>

#include "app/RadarApp.h"

RadarApp app;

void setup()
{
    app.begin();
}

void loop()
{
    app.update();
}
