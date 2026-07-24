#include "WifiManagerSimple.h"

#include <WiFi.h>
#include <string.h>

#include "DebugLog.h"
#include "WifiConfig.h"

void WifiManagerSimple::begin()
{
    begin(WifiConfig::kSsid, WifiConfig::kPassword);
}

void WifiManagerSimple::begin(const char *ssid, const char *password, uint8_t txPowerQuarterDbm)
{
    strncpy(ssid_, ssid != nullptr ? ssid : "", sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';
    strncpy(password_, password != nullptr ? password : "", sizeof(password_) - 1);
    password_[sizeof(password_) - 1] = '\0';
    txPowerQuarterDbm_ = txPowerQuarterDbm;

    DebugLog::println("Starting WiFi...");
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(static_cast<wifi_power_t>(txPowerQuarterDbm_));
    WiFi.setSleep(false);
    DebugLog::printf("WiFi TX power set to %.1f dBm (%d)\r\n",
                     static_cast<float>(txPowerQuarterDbm_) / 4.0f,
                     static_cast<int>(WiFi.getTxPower()));
    started_ = false;
    wasConnected_ = false;
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

void WifiManagerSimple::stop()
{
    if (!started_ && !wasConnected_)
    {
        return;
    }

    DebugLog::println("Stopping WiFi STA connection.");
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    started_ = false;
    wasConnected_ = false;
    lastConnectAttemptMs_ = 0;
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

    DebugLog::printf("WiFi connecting to SSID: %s status=%d pass_set=%u\r\n",
                     ssid_,
                     static_cast<int>(WiFi.status()),
                     password_[0] != '\0' ? 1 : 0);
    WiFi.disconnect(false);
    delay(50);
    WiFi.begin(ssid_, password_);
}
