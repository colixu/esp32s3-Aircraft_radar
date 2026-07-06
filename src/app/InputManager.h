#pragma once

#include <Arduino.h>

class InputManager
{
public:
    void begin(int buttonPin);
    void update();
    bool wasUiSwitchPressed();
    bool wasRangeSwitchPressed();
    bool wasGroundTogglePressed();
    bool wasPrintSettingsPressed();
    bool wasResetDefaultsPressed();

private:
    int buttonPin_ = -1;
    bool uiSwitchPressed_ = false;
    bool rangeSwitchPressed_ = false;
    bool groundTogglePressed_ = false;
    bool printSettingsPressed_ = false;
    bool resetDefaultsPressed_ = false;
};
