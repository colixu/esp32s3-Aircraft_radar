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
