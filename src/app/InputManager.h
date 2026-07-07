#pragma once

#include <Arduino.h>

#include "InputEvent.h"
#include "UserSettings.h"

class InputManager
{
public:
    void begin(const UserSettings &settings);
    void update();
    bool popEvent(InputEvent &event);

private:
    static constexpr uint8_t kEventQueueSize = 8;

    int buttonPin_ = -1;
    InputEvent eventQueue_[kEventQueueSize] = {};
    uint8_t eventHead_ = 0;
    uint8_t eventTail_ = 0;
    uint8_t eventCount_ = 0;

    void pushEvent(InputEvent event);
    void handleSerialCommand(char command);
    void updateButtons();
};
