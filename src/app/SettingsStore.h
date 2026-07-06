#pragma once

#include "AppConfig.h"
#include "UserSettings.h"

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
};
