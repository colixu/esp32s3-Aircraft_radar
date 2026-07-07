#include "InputManager.h"

void InputManager::begin(int buttonPin)
{
    buttonPin_ = buttonPin;

    if (buttonPin_ >= 0)
    {
        pinMode(buttonPin_, INPUT_PULLUP);
    }
}

void InputManager::update()
{
    while (Serial0.available() > 0)
    {
        const char command = static_cast<char>(Serial0.read());
        if (command == 'u' || command == 'U')
        {
            uiSwitchPressed_ = true;
        }
        else if (command == 'r' || command == 'R')
        {
            rangeSwitchPressed_ = true;
        }
        else if (command == 'g' || command == 'G')
        {
            groundTogglePressed_ = true;
        }
        else if (command == 'p' || command == 'P')
        {
            printSettingsPressed_ = true;
        }
        else if (command == 'd' || command == 'D')
        {
            resetDefaultsPressed_ = true;
        }
        else if (command == 's' || command == 'S')
        {
            saveSettingsPressed_ = true;
        }
        else if (command == 'l' || command == 'L')
        {
            loadSettingsPressed_ = true;
        }
        else if (command == 't' || command == 'T')
        {
            printTimePressed_ = true;
        }
        else if (command == 'm' || command == 'M')
        {
            printModePressed_ = true;
        }
        else if (command == 'w' || command == 'W')
        {
            staSettingsPressed_ = true;
        }
        else if (command == 'a')
        {
            printApiAuthPressed_ = true;
        }
        else if (command == 'A')
        {
            clearAuthTokenPressed_ = true;
        }
        else if (command == 'h' || command == 'H')
        {
            helpPressed_ = true;
        }
        else if (command == 'c' || command == 'C')
        {
            configPortalPressed_ = true;
        }
        else if (command == 'x' || command == 'X')
        {
            exitConfigPortalPressed_ = true;
        }
        else if (command == 'q' || command == 'Q')
        {
            setupDisplayTogglePressed_ = true;
        }
        else if (command == 'b' || command == 'B')
        {
            rebootPressed_ = true;
        }
    }

    // Hardware button support is intentionally reserved for a later pass.
}

bool InputManager::wasUiSwitchPressed()
{
    const bool pressed = uiSwitchPressed_;
    uiSwitchPressed_ = false;
    return pressed;
}

bool InputManager::wasRangeSwitchPressed()
{
    const bool pressed = rangeSwitchPressed_;
    rangeSwitchPressed_ = false;
    return pressed;
}

bool InputManager::wasGroundTogglePressed()
{
    const bool pressed = groundTogglePressed_;
    groundTogglePressed_ = false;
    return pressed;
}

bool InputManager::wasPrintSettingsPressed()
{
    const bool pressed = printSettingsPressed_;
    printSettingsPressed_ = false;
    return pressed;
}

bool InputManager::wasResetDefaultsPressed()
{
    const bool pressed = resetDefaultsPressed_;
    resetDefaultsPressed_ = false;
    return pressed;
}

bool InputManager::wasSaveSettingsPressed()
{
    const bool pressed = saveSettingsPressed_;
    saveSettingsPressed_ = false;
    return pressed;
}

bool InputManager::wasLoadSettingsPressed()
{
    const bool pressed = loadSettingsPressed_;
    loadSettingsPressed_ = false;
    return pressed;
}

bool InputManager::wasPrintTimePressed()
{
    const bool pressed = printTimePressed_;
    printTimePressed_ = false;
    return pressed;
}

bool InputManager::wasPrintModePressed()
{
    const bool pressed = printModePressed_;
    printModePressed_ = false;
    return pressed;
}

bool InputManager::wasStaSettingsPressed()
{
    const bool pressed = staSettingsPressed_;
    staSettingsPressed_ = false;
    return pressed;
}

bool InputManager::wasPrintApiAuthPressed()
{
    const bool pressed = printApiAuthPressed_;
    printApiAuthPressed_ = false;
    return pressed;
}

bool InputManager::wasClearAuthTokenPressed()
{
    const bool pressed = clearAuthTokenPressed_;
    clearAuthTokenPressed_ = false;
    return pressed;
}

bool InputManager::wasHelpPressed()
{
    const bool pressed = helpPressed_;
    helpPressed_ = false;
    return pressed;
}

bool InputManager::wasConfigPortalPressed()
{
    const bool pressed = configPortalPressed_;
    configPortalPressed_ = false;
    return pressed;
}

bool InputManager::wasExitConfigPortalPressed()
{
    const bool pressed = exitConfigPortalPressed_;
    exitConfigPortalPressed_ = false;
    return pressed;
}

bool InputManager::wasSetupDisplayTogglePressed()
{
    const bool pressed = setupDisplayTogglePressed_;
    setupDisplayTogglePressed_ = false;
    return pressed;
}

bool InputManager::wasRebootPressed()
{
    const bool pressed = rebootPressed_;
    rebootPressed_ = false;
    return pressed;
}
