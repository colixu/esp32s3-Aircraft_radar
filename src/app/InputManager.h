#pragma once

#include <Arduino.h>

class InputManager
{
public:
    void begin(int buttonPin);
    void update();
    bool wasUiSwitchPressed();

private:
    int buttonPin_ = -1;
    bool uiSwitchPressed_ = false;
};
