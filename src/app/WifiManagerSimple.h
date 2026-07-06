#pragma once

#include <Arduino.h>

class WifiManagerSimple
{
public:
    void begin();
    void update(uint32_t now, uint32_t reconnectIntervalMs);

    bool isConnected() const;
    const char *statusText() const;
    int rssi() const;

private:
    uint32_t lastConnectAttemptMs_ = 0;
    bool started_ = false;
    bool wasConnected_ = false;

    void startConnect(uint32_t now);
};
