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
    bool wasSaveSettingsPressed();
    bool wasLoadSettingsPressed();
    bool wasPrintTimePressed();
    bool wasPrintModePressed();
    bool wasHelpPressed();
    bool wasConfigPortalPressed();
    bool wasExitConfigPortalPressed();
    bool wasSetupDisplayTogglePressed();
    bool wasRebootPressed();

private:
    int buttonPin_ = -1;
    bool uiSwitchPressed_ = false;
    bool rangeSwitchPressed_ = false;
    bool groundTogglePressed_ = false;
    bool printSettingsPressed_ = false;
    bool resetDefaultsPressed_ = false;
    bool saveSettingsPressed_ = false;
    bool loadSettingsPressed_ = false;
    bool printTimePressed_ = false;
    bool printModePressed_ = false;
    bool helpPressed_ = false;
    bool configPortalPressed_ = false;
    bool exitConfigPortalPressed_ = false;
    bool setupDisplayTogglePressed_ = false;
    bool rebootPressed_ = false;
};
