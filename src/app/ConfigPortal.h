#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "SettingsStore.h"
#include "UserSettings.h"

enum class ConfigPortalMode
{
    ApSetup,
    StaSettings
};

class ConfigPortal
{
public:
    bool begin(UserSettings *settings, SettingsStore *settingsStore);
    bool beginApSetup(UserSettings *settings, SettingsStore *settingsStore);
    bool beginStaSettings(UserSettings *settings, SettingsStore *settingsStore);
    void update();
    void stop();

    bool isRunning() const;
    ConfigPortalMode mode() const;
    bool shouldRestart() const;
    const char *apSsid() const;
    const char *apPassword() const;
    const char *ipAddress() const;
    const char *staIpAddress() const;

private:
    enum class PageLanguage
    {
        English,
        Chinese
    };

    static constexpr const char *kApPassword = "12345678";
    static constexpr byte kDnsPort = 53;

    WebServer server_{80};
    DNSServer dnsServer_;
    UserSettings *settings_ = nullptr;
    SettingsStore *settingsStore_ = nullptr;
    bool running_ = false;
    bool restartRequested_ = false;
    bool dnsRunning_ = false;
    ConfigPortalMode mode_ = ConfigPortalMode::ApSetup;
    PageLanguage pageLanguage_ = PageLanguage::Chinese;
    char apSsid_[32] = "";
    char ipAddress_[16] = "192.168.4.1";
    char staIpAddress_[16] = "0.0.0.0";
    String page_;

    void beginServer();
    void handleRoot();
    void handleAdvanced();
    void handleSave();
    void handleSaveSimple();
    void handleSaveAdvanced();
    void handleStatus();
    void handleRestart();
    void handleNotFound();

    void renderSimplePage();
    void renderAdvancedPage();
    void renderSavedPage(bool saved);
    void applySimpleFormToSettings();
    void applyAdvancedFormToSettings();
    void updateLanguageFromRequest();
    const char *text(const char *english, const char *chinese) const;
    const char *languageCode() const;
    const char *toggleLanguageCode() const;
    const char *toggleLanguageLabel() const;
    void sendPageHeader(const char *title);
    void sendPageFooter();
    void write(const char *content);
    void sendLanguageSwitch(const char *path);
    void sendTextInput(const char *label, const char *name, const char *value, bool password);
    void sendNumberInput(const char *label, const char *name, const char *value, const char *step);
    void sendKmInput(const char *label, const char *name, const char *value, const char *step);
    void sendCheckbox(const char *label, const char *name, bool checked);
    void sendSelectOption(const char *value, const char *label, bool selected);
    void sendHiddenLanguage();
    void sendStatusPanel(uint32_t intervalMs);
    bool hasCheckedArg(const char *name);
    int argToInt(const char *name, int fallback);
    uint32_t argToUInt(const char *name, uint32_t fallback);
    float argToFloat(const char *name, float fallback);
    void copyArgToBuffer(const char *name, char *buffer, size_t bufferSize, bool keepOldIfEmpty);
};
