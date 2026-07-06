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
    }

    // Hardware button support is intentionally reserved for a later pass.
}

bool InputManager::wasUiSwitchPressed()
{
    const bool pressed = uiSwitchPressed_;
    uiSwitchPressed_ = false;
    return pressed;
}
