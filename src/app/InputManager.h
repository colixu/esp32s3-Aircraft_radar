#pragma once

#include <Arduino.h>

#include "InputEvent.h"
#include "UserSettings.h"

struct UiTuningCommand
{
    char key[32];
    float values[3];
    uint8_t valueCount;
    bool pending;
};

class InputManager
{
public:
    void begin(const UserSettings &settings);
    void update();
    bool popEvent(InputEvent &event);
    bool popUiTuningCommand(UiTuningCommand &command);

private:
    static constexpr uint8_t kEventQueueSize = 8;
    static constexpr uint8_t kLineBufferSize = 128;

    int buttonPin_ = -1;
    InputEvent eventQueue_[kEventQueueSize] = {};
    uint8_t eventHead_ = 0;
    uint8_t eventTail_ = 0;
    uint8_t eventCount_ = 0;
    char lineBuffer_[kLineBufferSize] = {};
    uint8_t lineLength_ = 0;
    UiTuningCommand pendingUiCommand_ = {};
    bool uiCommandPending_ = false;

    void pushEvent(InputEvent event);
    void handleSerialInput(char command, Stream &serial);
    void handleSerialCommand(char command);
    void handleSerialLine();
    bool parseVirtualButtonCommand(char *line);
    bool parseUiTuningCommand(char *line);
    void updateButtons();
};
