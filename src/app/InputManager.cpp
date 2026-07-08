#include "InputManager.h"

#include <stdlib.h>
#include <string.h>

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
    lineLength_ = 0;
    uiCommandPending_ = false;
    memset(lineBuffer_, 0, sizeof(lineBuffer_));
    memset(&pendingUiCommand_, 0, sizeof(pendingUiCommand_));

    if (buttonPin_ >= 0)
    {
        pinMode(buttonPin_, INPUT_PULLUP);
    }
}

void InputManager::update()
{
    while (Serial0.available() > 0)
    {
        handleSerialInput(static_cast<char>(Serial0.read()));
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

bool InputManager::popUiTuningCommand(UiTuningCommand &command)
{
    if (!uiCommandPending_)
    {
        memset(&command, 0, sizeof(command));
        return false;
    }

    command = pendingUiCommand_;
    pendingUiCommand_.pending = false;
    uiCommandPending_ = false;
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

void InputManager::handleSerialInput(char command)
{
    if (lineLength_ > 0)
    {
        if (command == '\r' || command == '\n')
        {
            lineBuffer_[lineLength_] = '\0';
            handleSerialLine();
            lineLength_ = 0;
            lineBuffer_[0] = '\0';
            return;
        }

        if (lineLength_ < kLineBufferSize - 1)
        {
            lineBuffer_[lineLength_++] = command;
        }
        else
        {
            lineLength_ = 0;
            lineBuffer_[0] = '\0';
            DebugLog::println("Serial line too long, dropped.");
        }
        return;
    }

    if ((command == 's' || command == 'S') && Serial0.available() > 0)
    {
        const int next = Serial0.peek();
        if (next == 'e' || next == 'E')
        {
            lineBuffer_[0] = command;
            lineLength_ = 1;
            return;
        }
    }

    handleSerialCommand(command);
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

        case 'y':
        case 'Y':
            pushEvent(InputEvent::ToggleUiLab);
            break;

        case 'f':
        case 'F':
            pushEvent(InputEvent::NextUiLabScene);
            break;

        case 'j':
        case 'J':
            pushEvent(InputEvent::PrintUiTuning);
            break;

        case 'k':
        case 'K':
            pushEvent(InputEvent::SaveUiTuning);
            break;

        case 'n':
        case 'N':
            pushEvent(InputEvent::ResetUiTuning);
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

void InputManager::handleSerialLine()
{
    if (parseUiTuningCommand(lineBuffer_))
    {
        return;
    }

    DebugLog::printf("Unknown line command: %s\r\n", lineBuffer_);
}

bool InputManager::parseUiTuningCommand(char *line)
{
    if (line == nullptr)
    {
        return false;
    }

    char *token = strtok(line, " \t");
    if (token == nullptr ||
        (strcmp(token, "set") != 0 && strcmp(token, "Set") != 0 && strcmp(token, "SET") != 0))
    {
        return false;
    }

    token = strtok(nullptr, " \t");
    if (token == nullptr)
    {
        DebugLog::println("set command missing key.");
        return true;
    }

    memset(&pendingUiCommand_, 0, sizeof(pendingUiCommand_));
    strncpy(pendingUiCommand_.key, token, sizeof(pendingUiCommand_.key) - 1);
    pendingUiCommand_.key[sizeof(pendingUiCommand_.key) - 1] = '\0';

    while (pendingUiCommand_.valueCount < 3)
    {
        token = strtok(nullptr, " \t");
        if (token == nullptr)
        {
            break;
        }
        pendingUiCommand_.values[pendingUiCommand_.valueCount++] = static_cast<float>(atof(token));
    }

    pendingUiCommand_.pending = true;
    uiCommandPending_ = true;
    return true;
}

void InputManager::updateButtons()
{
    // Future hardware mapping:
    // SETUP short press -> ShowStaSettings
    // SETUP long press  -> EnterApSetup
    // MODE short press  -> NextUiTheme
    // BACK short press  -> ExitCurrentView
}
