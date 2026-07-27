#include <Arduino.h>

#include "app/RadarApp.h"

SET_LOOP_TASK_STACK_SIZE(12 * 1024);

RadarApp app;

void setup()
{
    app.begin();
}

void loop()
{
    app.update();
    app.idle();
}
