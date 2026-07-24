#pragma once

#include <Arduino.h>

class WifiManagerSimple
{
public:
    void begin();
    void begin(const char *ssid, const char *password, uint8_t txPowerQuarterDbm = 60);
    void update(uint32_t now, uint32_t reconnectIntervalMs);
    void stop();

    bool isConnected() const;
    const char *statusText() const;
    int rssi() const;

private:
    uint32_t lastConnectAttemptMs_ = 0;
    bool started_ = false;
    bool wasConnected_ = false;
    char ssid_[32] = "";
    char password_[64] = "";
    uint8_t txPowerQuarterDbm_ = 60;

    void startConnect(uint32_t now);
};
