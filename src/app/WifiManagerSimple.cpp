#include "WifiManagerSimple.h"

#include <WiFi.h>

#include "DebugLog.h"
#include "WifiConfig.h"

void WifiManagerSimple::begin()
{
    DebugLog::println("Starting WiFi for API test...");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    startConnect(millis());
}

void WifiManagerSimple::update(uint32_t now, uint32_t reconnectIntervalMs)
{
    if (isConnected())
    {
        if (!wasConnected_)
        {
            wasConnected_ = true;
            DebugLog::printf("WiFi connected. IP: %s RSSI: %d dBm\r\n",
                             WiFi.localIP().toString().c_str(),
                             WiFi.RSSI());
        }
        return;
    }

    if (wasConnected_)
    {
        wasConnected_ = false;
        DebugLog::println("WiFi disconnected.");
    }

    if (!started_ || now - lastConnectAttemptMs_ >= reconnectIntervalMs)
    {
        startConnect(now);
    }
}

bool WifiManagerSimple::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

const char *WifiManagerSimple::statusText() const
{
    if (isConnected())
    {
        return "OK";
    }

    switch (WiFi.status())
    {
        case WL_IDLE_STATUS:
            return "IDLE";
        case WL_NO_SSID_AVAIL:
            return "NO SSID";
        case WL_CONNECT_FAILED:
            return "FAIL";
        case WL_CONNECTION_LOST:
            return "LOST";
        case WL_DISCONNECTED:
            return "DISC";
        default:
            return "WAIT";
    }
}

int WifiManagerSimple::rssi() const
{
    if (!isConnected())
    {
        return 0;
    }

    return WiFi.RSSI();
}

void WifiManagerSimple::startConnect(uint32_t now)
{
    started_ = true;
    lastConnectAttemptMs_ = now;

    DebugLog::printf("WiFi connecting to SSID: %s\r\n", WifiConfig::kSsid);
    WiFi.disconnect(false);
    WiFi.begin(WifiConfig::kSsid, WifiConfig::kPassword);
}
