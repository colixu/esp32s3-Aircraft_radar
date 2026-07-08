#include "InputManager.h"

#include "DebugLog.h"

namespace
{
    bool isIgnoredSerialChar(char command)
    {
        return command == '\r' || command == '\n' || command == ' ' || command == '\t';
    }
}

void InputManager::begin(const UserSettings &settings)
{
    buttonPin_ = settings.system.uiButtonPin;
    eventHead_ = 0;
    eventTail_ = 0;
    eventCount_ = 0;

    if (buttonPin_ >= 0)
    {
        pinMode(buttonPin_, INPUT_PULLUP);
    }
}

void InputManager::update()
{
    while (Serial0.available() > 0)
    {
        handleSerialCommand(static_cast<char>(Serial0.read()));
    }

    updateButtons();
}

bool InputManager::popEvent(InputEvent &event)
{
    if (eventCount_ == 0)
    {
        event = InputEvent::None;
        return false;
    }

    event = eventQueue_[eventTail_];
    eventTail_ = (eventTail_ + 1) % kEventQueueSize;
    --eventCount_;
    return true;
}

void InputManager::pushEvent(InputEvent event)
{
    if (event == InputEvent::None)
    {
        return;
    }

    if (eventCount_ >= kEventQueueSize)
    {
        DebugLog::println("InputManager: event queue full, dropping input event.");
        return;
    }

    eventQueue_[eventHead_] = event;
    eventHead_ = (eventHead_ + 1) % kEventQueueSize;
    ++eventCount_;
}

void InputManager::handleSerialCommand(char command)
{
    if (isIgnoredSerialChar(command))
    {
        return;
    }

    switch (command)
    {
        case 'h':
        case 'H':
            pushEvent(InputEvent::ShowHelp);
            break;

        case 'p':
        case 'P':
            pushEvent(InputEvent::PrintSettings);
            break;

        case 'c':
        case 'C':
            pushEvent(InputEvent::EnterApSetup);
            break;

        case 'w':
        case 'W':
            pushEvent(InputEvent::ShowStaSettings);
            break;

        case 'q':
        case 'Q':
            pushEvent(InputEvent::ToggleSettingsDisplay);
            break;

        case 'x':
        case 'X':
            pushEvent(InputEvent::ExitCurrentView);
            break;

        case 'u':
        case 'U':
            pushEvent(InputEvent::NextUiTheme);
            break;

        case 'r':
        case 'R':
            pushEvent(InputEvent::SwitchRange);
            break;

        case 'g':
        case 'G':
            pushEvent(InputEvent::ToggleGroundTraffic);
            break;

        case 'o':
        case 'O':
            pushEvent(InputEvent::CycleScheduleIdleDisplayMode);
            break;

        case 's':
        case 'S':
            pushEvent(InputEvent::SaveSettings);
            break;

        case 'l':
        case 'L':
            pushEvent(InputEvent::LoadSettings);
            break;

        case 'd':
        case 'D':
            pushEvent(InputEvent::ResetDefaults);
            break;

        case 't':
        case 'T':
            pushEvent(InputEvent::PrintTimeStatus);
            break;

        case 'm':
        case 'M':
            pushEvent(InputEvent::PrintDeviceStatus);
            break;

        case 'a':
            pushEvent(InputEvent::PrintApiAuthStatus);
            break;

        case 'A':
            pushEvent(InputEvent::ClearAuthToken);
            break;

        case 'b':
        case 'B':
            pushEvent(InputEvent::Reboot);
            break;

        default:
            DebugLog::printf("Unknown serial command '%c'. Press h for help.\r\n", command);
            break;
    }
}

void InputManager::updateButtons()
{
    // Future hardware mapping:
    // SETUP short press -> ShowStaSettings
    // SETUP long press  -> EnterApSetup
    // MODE short press  -> NextUiTheme
    // BACK short press  -> ExitCurrentView
}
