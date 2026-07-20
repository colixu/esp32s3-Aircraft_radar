#include "InputManager.h"

#include <stdlib.h>
#include <string.h>

#include "DebugLog.h"
#include "FeatureFlags.h"

namespace
{
    constexpr uint32_t kBootDebounceMs = 20;
    constexpr uint32_t kBootDoubleClickWindowMs = 850;
    constexpr uint32_t kBootLongPressMs = 900;
    constexpr uint32_t kBootIgnoreAfterStartupMs = 500;
    constexpr uint32_t kBootPostEventGuardMs = 100;

    bool serialDebugDisabledNotified = false;

    bool isIgnoredSerialChar(char command)
    {
        return command == '\r' || command == '\n' || command == ' ' || command == '\t';
    }

    bool equalsIgnoreCase(const char *a, const char *b)
    {
        if (a == nullptr || b == nullptr)
        {
            return false;
        }

        while (*a != '\0' && *b != '\0')
        {
            char ca = *a;
            char cb = *b;
            if (ca >= 'A' && ca <= 'Z')
            {
                ca = static_cast<char>(ca - 'A' + 'a');
            }
            if (cb >= 'A' && cb <= 'Z')
            {
                cb = static_cast<char>(cb - 'A' + 'a');
            }
            if (ca != cb)
            {
                return false;
            }
            ++a;
            ++b;
        }

        return *a == '\0' && *b == '\0';
    }

    void printVirtualButtonHelp()
    {
        DebugLog::println("Unknown button command. Use: btn up short|long|double or btn down short|long|double");
    }

    void printSerialDebugDisabledOnce()
    {
        if (serialDebugDisabledNotified)
        {
            return;
        }

        serialDebugDisabledNotified = true;
        DebugLog::println("Serial debug commands disabled. Use virtual button commands.");
    }
}

void InputManager::begin(const UserSettings &settings)
{
    buttonPin_ = settings.system.uiButtonPin;
    inputStartedMs_ = millis();
    bootRawChangedMs_ = inputStartedMs_;
    bootPressStartedMs_ = 0;
    bootPendingClickMs_ = 0;
    bootPostEventGuardUntilMs_ = 0;
    eventHead_ = 0;
    eventTail_ = 0;
    eventCount_ = 0;
    lineLength_ = 0;
    uiCommandPending_ = false;
    memset(lineBuffer_, 0, sizeof(lineBuffer_));
    memset(&pendingUiCommand_, 0, sizeof(pendingUiCommand_));
    bootLastRawPressed_ = false;
    bootStablePressed_ = false;
    bootLongFired_ = false;
    bootPendingClick_ = false;
    bootDoublePressArmed_ = false;

    if (buttonPin_ >= 0)
    {
        pinMode(buttonPin_, INPUT_PULLUP);
    }

#if ENABLE_SINGLE_BOOT_BUTTON
    pinMode(USER_BOOT_BUTTON_PIN, INPUT_PULLUP);
#endif
}

void InputManager::update()
{
    while (Serial.available() > 0)
    {
        handleSerialInput(static_cast<char>(Serial.read()), Serial);
    }

    while (Serial0.available() > 0)
    {
        handleSerialInput(static_cast<char>(Serial0.read()), Serial0);
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

void InputManager::handleSerialInput(char command, Stream &serial)
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

    if (command == 's' || command == 'S')
    {
        if (serial.available() == 0)
        {
            lineBuffer_[0] = command;
            lineLength_ = 1;
            return;
        }

        const int next = serial.peek();
        if (next == 'e' || next == 'E')
        {
            lineBuffer_[0] = command;
            lineLength_ = 1;
            return;
        }
    }

    if (command == 'b' || command == 'B')
    {
        if (serial.available() == 0)
        {
            lineBuffer_[0] = command;
            lineLength_ = 1;
            return;
        }

        const int next = serial.peek();
        if (next == 't' || next == 'T' ||
            next == 'u' || next == 'U' ||
            next == 'd' || next == 'D')
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

#if !ENABLE_SERIAL_DEBUG_COMMANDS
    (void)command;
    printSerialDebugDisabledOnce();
    return;
#else
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
#if ENABLE_UI_LAB
            pushEvent(InputEvent::ToggleUiLab);
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case 'f':
        case 'F':
#if ENABLE_UI_LAB
            pushEvent(InputEvent::NextUiLabScene);
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case 'j':
        case 'J':
#if ENABLE_UI_LAB
            pushEvent(InputEvent::PrintUiTuning);
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case 'k':
        case 'K':
#if ENABLE_UI_LAB && ENABLE_UI_LAB_ADVANCED_TUNING
            pushEvent(InputEvent::SaveUiTuning);
#elif ENABLE_UI_LAB
            DebugLog::println("Advanced UI tuning is disabled in this build.");
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
            break;

        case 'n':
        case 'N':
#if ENABLE_UI_LAB
            pushEvent(InputEvent::ResetUiTuning);
#else
            DebugLog::println("UI Lab is disabled in this build.");
#endif
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
#endif
}

void InputManager::handleSerialLine()
{
    if (parseVirtualButtonCommand(lineBuffer_))
    {
        return;
    }

    if (parseUiTuningCommand(lineBuffer_))
    {
        return;
    }

    if (lineLength_ == 1)
    {
        handleSerialCommand(lineBuffer_[0]);
        return;
    }

    DebugLog::printf("Unknown line command: %s\r\n", lineBuffer_);
}

bool InputManager::parseVirtualButtonCommand(char *line)
{
    if (line == nullptr)
    {
        return false;
    }

    char original[kLineBufferSize];
    strncpy(original, line, sizeof(original) - 1);
    original[sizeof(original) - 1] = '\0';

    char *token = strtok(line, " \t");
    if (token == nullptr)
    {
        return false;
    }

#if !ENABLE_SERIAL_VIRTUAL_BUTTONS
    if (equalsIgnoreCase(token, "btn") ||
        equalsIgnoreCase(token, "bu") ||
        equalsIgnoreCase(token, "bd"))
    {
        DebugLog::println("Virtual button commands are disabled in this build.");
        return true;
    }

    strncpy(line, original, kLineBufferSize - 1);
    line[kLineBufferSize - 1] = '\0';
    return false;
#else
    bool up = false;
    bool down = false;
    char *action = nullptr;

    if (equalsIgnoreCase(token, "btn"))
    {
        char *direction = strtok(nullptr, " \t");
        action = strtok(nullptr, " \t");
        if (equalsIgnoreCase(direction, "up"))
        {
            up = true;
        }
        else if (equalsIgnoreCase(direction, "down"))
        {
            down = true;
        }
        else
        {
            printVirtualButtonHelp();
            return true;
        }
    }
    else if (equalsIgnoreCase(token, "bu"))
    {
        up = true;
        action = strtok(nullptr, " \t");
    }
    else if (equalsIgnoreCase(token, "bd"))
    {
        down = true;
        action = strtok(nullptr, " \t");
    }
    else
    {
        strncpy(line, original, kLineBufferSize - 1);
        line[kLineBufferSize - 1] = '\0';
        return false;
    }

    if (action == nullptr)
    {
        printVirtualButtonHelp();
        return true;
    }

    InputEvent event = InputEvent::None;
    if (equalsIgnoreCase(action, "short"))
    {
        event = up ? InputEvent::KeyUpShort : InputEvent::KeyDownShort;
    }
    else if (equalsIgnoreCase(action, "long"))
    {
        event = up ? InputEvent::KeyUpLong : InputEvent::KeyDownLong;
    }
    else if (equalsIgnoreCase(action, "double"))
    {
        event = up ? InputEvent::KeyUpDouble : InputEvent::KeyDownDouble;
    }
    else
    {
        printVirtualButtonHelp();
        return true;
    }

    if (!up && !down)
    {
        printVirtualButtonHelp();
        return true;
    }

    pushEvent(event);
    DebugLog::printf("Virtual button: %s %s\r\n", up ? "up" : "down", action);
    return true;
#endif
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

#if !ENABLE_UI_LAB
    DebugLog::println("UI Lab is disabled in this build.");
    return true;
#elif !ENABLE_UI_LAB_ADVANCED_TUNING
    DebugLog::println("Advanced UI tuning is disabled in this build.");
    return true;
#else
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
#endif
}

void InputManager::updateButtons()
{
#if ENABLE_SINGLE_BOOT_BUTTON
    updateBootButton();
#endif
}

void InputManager::updateBootButton()
{
#if ENABLE_SINGLE_BOOT_BUTTON
    const uint32_t now = millis();
    const bool rawPressed =
#if USER_BOOT_BUTTON_ACTIVE_LOW
        digitalRead(USER_BOOT_BUTTON_PIN) == LOW;
#else
        digitalRead(USER_BOOT_BUTTON_PIN) == HIGH;
#endif

    if (now - inputStartedMs_ < kBootIgnoreAfterStartupMs)
    {
        bootLastRawPressed_ = rawPressed;
        bootStablePressed_ = rawPressed;
        bootRawChangedMs_ = now;
        bootPressStartedMs_ = rawPressed ? now : 0;
        bootPostEventGuardUntilMs_ = 0;
        bootLongFired_ = false;
        bootPendingClick_ = false;
        bootDoublePressArmed_ = false;
        return;
    }

    if (bootPostEventGuardUntilMs_ != 0 && now < bootPostEventGuardUntilMs_)
    {
        if (!rawPressed)
        {
            bootLastRawPressed_ = false;
            bootStablePressed_ = false;
            bootRawChangedMs_ = now;
        }
        bootPressStartedMs_ = 0;
        bootLongFired_ = false;
        bootPendingClick_ = false;
        bootDoublePressArmed_ = false;
        return;
    }

    if (bootPostEventGuardUntilMs_ != 0 && now >= bootPostEventGuardUntilMs_)
    {
        bootPostEventGuardUntilMs_ = 0;
    }

    if (rawPressed != bootLastRawPressed_)
    {
        bootLastRawPressed_ = rawPressed;
        bootRawChangedMs_ = now;
    }

    if (now - bootRawChangedMs_ >= kBootDebounceMs &&
        rawPressed != bootStablePressed_)
    {
        bootStablePressed_ = rawPressed;

        if (bootStablePressed_)
        {
            if (bootPendingClick_)
            {
                if (now - bootPendingClickMs_ <= kBootDoubleClickWindowMs)
                {
                    bootPendingClick_ = false;
                    bootDoublePressArmed_ = true;
                }
                else
                {
                    bootPendingClick_ = false;
                    pushEvent(InputEvent::BootButtonShort);
                }
            }

            bootPressStartedMs_ = now;
            bootLongFired_ = false;
        }
        else
        {
            if (!bootLongFired_)
            {
                if (bootDoublePressArmed_)
                {
                    bootDoublePressArmed_ = false;
                    bootPostEventGuardUntilMs_ = now + kBootPostEventGuardMs;
                    pushEvent(InputEvent::BootButtonDouble);
                }
                else
                {
                    bootPendingClick_ = true;
                    bootPendingClickMs_ = now;
                }
            }

            bootPressStartedMs_ = 0;
            bootLongFired_ = false;
        }
    }

    if (bootStablePressed_ &&
        !bootLongFired_ &&
        bootPressStartedMs_ != 0 &&
        now - bootPressStartedMs_ >= kBootLongPressMs)
    {
        bootLongFired_ = true;
        bootPendingClick_ = false;
        bootDoublePressArmed_ = false;
        bootPostEventGuardUntilMs_ = now + kBootPostEventGuardMs;
        pushEvent(InputEvent::BootButtonLong);
    }

    if (bootPendingClick_ &&
        !bootStablePressed_ &&
        now - bootPendingClickMs_ > kBootDoubleClickWindowMs)
    {
        bootPendingClick_ = false;
        pushEvent(InputEvent::BootButtonShort);
    }
#endif
    // Future hardware mapping:
    // SETUP short press -> ShowStaSettings
    // SETUP long press  -> EnterApSetup
    // MODE short press  -> NextUiTheme
    // BACK short press  -> ExitCurrentView
}
