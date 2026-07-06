#pragma once

#include "AppConfig.h"
#include "UserSettings.h"

#ifndef ENABLE_NVS_SETTINGS
#define ENABLE_NVS_SETTINGS 0
#endif

#if ENABLE_NVS_SETTINGS
#include <Preferences.h>
#endif

class SettingsStore
{
public:
    explicit SettingsStore(const AppConfig &config);

    bool begin();
    bool load(UserSettings &settings);
    bool save(const UserSettings &settings);
    void resetToDefault(UserSettings &settings);

private:
    const AppConfig &config_;
    bool nvsReady_ = false;

#if ENABLE_NVS_SETTINGS
    Preferences preferences_;
#endif
};
