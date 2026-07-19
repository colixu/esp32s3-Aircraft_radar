#include "ConfigPortal.h"

#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DebugLog.h"

namespace
{
    const char *nvsStateText()
    {
#if ENABLE_NVS_SETTINGS
        return "enabled";
#else
        return "disabled, settings are volatile";
#endif
    }

    const char *nvsJsonText()
    {
#if ENABLE_NVS_SETTINGS
        return "true";
#else
        return "false";
#endif
    }
}

bool ConfigPortal::begin(UserSettings *settings, SettingsStore *settingsStore)
{
    return beginApSetup(settings, settingsStore);
}

bool ConfigPortal::beginApSetup(UserSettings *settings, SettingsStore *settingsStore)
{
    settings_ = settings;
    settingsStore_ = settingsStore;
    restartRequested_ = false;
    mode_ = ConfigPortalMode::ApSetup;

    const uint64_t mac = ESP.getEfuseMac();
    snprintf(apSsid_, sizeof(apSsid_), "AircraftRadar-%04X", static_cast<uint16_t>(mac & 0xFFFF));
    snprintf(staIpAddress_, sizeof(staIpAddress_), "%s", WiFi.localIP().toString().c_str());

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(150);
    WiFi.mode(WIFI_AP);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    DebugLog::printf("ConfigPortal AP TX power set to 15 dBm (%d)\r\n", static_cast<int>(WiFi.getTxPower()));
    delay(50);
    const bool apStarted = WiFi.softAP(apSsid_, kApPassword);
    snprintf(ipAddress_, sizeof(ipAddress_), "%s", WiFi.softAPIP().toString().c_str());

    dnsRunning_ = dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());

    beginServer();
    DebugLog::printf("ConfigPortal started. mode=ap_setup AP=%s IP=%s NVS=%s started=%u\r\n",
                     apSsid_,
                     ipAddress_,
                     nvsStateText(),
                     apStarted ? 1 : 0);
    return true;
}

bool ConfigPortal::beginStaSettings(UserSettings *settings, SettingsStore *settingsStore)
{
    settings_ = settings;
    settingsStore_ = settingsStore;
    restartRequested_ = false;
    mode_ = ConfigPortalMode::StaSettings;

    const uint64_t mac = ESP.getEfuseMac();
    snprintf(apSsid_, sizeof(apSsid_), "AircraftRadar-%04X", static_cast<uint16_t>(mac & 0xFFFF));
    snprintf(ipAddress_, sizeof(ipAddress_), "192.168.4.1");
    snprintf(staIpAddress_, sizeof(staIpAddress_), "%s", WiFi.localIP().toString().c_str());

    if (dnsRunning_)
    {
        dnsServer_.stop();
        dnsRunning_ = false;
    }
    WiFi.softAPdisconnect(true);

    beginServer();
    DebugLog::printf("ConfigPortal started. mode=sta_settings STA=%s NVS=%s\r\n",
                     staIpAddress_,
                     nvsStateText());
    return true;
}

void ConfigPortal::beginServer()
{
    server_.stop();
    server_.on("/", HTTP_GET, [this]()
    {
        handleRoot();
    });
    server_.on("/advanced", HTTP_GET, [this]()
    {
        handleAdvanced();
    });
    server_.on("/save", HTTP_POST, [this]()
    {
        handleSave();
    });
    server_.on("/saveSimple", HTTP_POST, [this]()
    {
        handleSaveSimple();
    });
    server_.on("/saveAdvanced", HTTP_POST, [this]()
    {
        handleSaveAdvanced();
    });
    server_.on("/status", HTTP_GET, [this]()
    {
        handleStatus();
    });
    server_.on("/restart", HTTP_GET, [this]()
    {
        handleRestart();
    });
    server_.onNotFound([this]()
    {
        handleNotFound();
    });
    server_.begin();

    running_ = true;
}

void ConfigPortal::update()
{
    if (!running_)
    {
        return;
    }

    if (dnsRunning_)
    {
        dnsServer_.processNextRequest();
    }
    server_.handleClient();
}

void ConfigPortal::stop()
{
    if (!running_)
    {
        return;
    }

    server_.stop();
    if (dnsRunning_)
    {
        dnsServer_.stop();
        dnsRunning_ = false;
    }
    if (mode_ == ConfigPortalMode::ApSetup)
    {
        WiFi.softAPdisconnect(true);
    }
    running_ = false;
    DebugLog::println("ConfigPortal stopped.");
}

bool ConfigPortal::isRunning() const
{
    return running_;
}

ConfigPortalMode ConfigPortal::mode() const
{
    return mode_;
}

bool ConfigPortal::shouldRestart() const
{
    return restartRequested_;
}

const char *ConfigPortal::apSsid() const
{
    return apSsid_;
}

const char *ConfigPortal::apPassword() const
{
    return kApPassword;
}

const char *ConfigPortal::ipAddress() const
{
    return mode_ == ConfigPortalMode::StaSettings ? staIpAddress_ : ipAddress_;
}

const char *ConfigPortal::staIpAddress() const
{
    return staIpAddress_;
}

void ConfigPortal::handleRoot()
{
    updateLanguageFromRequest();
    renderSimplePage();
}

void ConfigPortal::handleAdvanced()
{
    updateLanguageFromRequest();
    renderAdvancedPage();
}

void ConfigPortal::handleSave()
{
    handleSaveSimple();
}

void ConfigPortal::handleSaveSimple()
{
    updateLanguageFromRequest();
    if (settings_ == nullptr || settingsStore_ == nullptr)
    {
        renderSavedPage(false);
        return;
    }

    applySimpleFormToSettings();
    sanitizeUserSettings(*settings_);
    const bool saved = settingsStore_->save(*settings_);
    DebugLog::printf("ConfigPortal simple save complete. saved=%u NVS=%s\r\n",
                     saved ? 1 : 0,
                     nvsStateText());
    renderSavedPage(saved);
}

void ConfigPortal::handleSaveAdvanced()
{
    updateLanguageFromRequest();
    if (settings_ == nullptr || settingsStore_ == nullptr)
    {
        renderSavedPage(false);
        return;
    }

    applyAdvancedFormToSettings();
    sanitizeUserSettings(*settings_);
    const bool saved = settingsStore_->save(*settings_);
    DebugLog::printf("ConfigPortal advanced save complete. saved=%u NVS=%s\r\n",
                     saved ? 1 : 0,
                     nvsStateText());
    renderSavedPage(saved);
}

void ConfigPortal::handleStatus()
{
    if (settings_ == nullptr)
    {
        server_.send(500, "application/json", "{\"error\":\"settings unavailable\"}");
        return;
    }

    char body[512];
    snprintf(body,
             sizeof(body),
             "{\"portalMode\":\"%s\",\"apSsid\":\"%s\",\"apIp\":\"%s\",\"staIp\":\"%s\","
             "\"wifiConfigured\":%s,\"wifiConnected\":%s,\"currentIP\":\"%s\","
             "\"apiProvider\":\"%s\",\"apiMode\":\"%s\",\"centerLat\":%.6f,\"centerLon\":%.6f,"
             "\"maxRangeKm\":%.1f,\"scheduleEnabled\":%s,\"computedRequestIntervalMs\":%lu,"
             "\"activeRequestIntervalMs\":%lu,\"idleDisplayMode\":\"%s\","
             "\"nvsEnabled\":%s}",
             mode_ == ConfigPortalMode::ApSetup ? "ap_setup" : "sta_settings",
             apSsid_,
             ipAddress_,
             staIpAddress_,
             settings_->wifi.configured ? "true" : "false",
             WiFi.status() == WL_CONNECTED ? "true" : "false",
             mode_ == ConfigPortalMode::StaSettings ? staIpAddress_ : ipAddress_,
             apiProviderName(settings_->api.provider),
             apiAccountModeName(settings_->api.accountMode),
             settings_->location.centerLat,
             settings_->location.centerLon,
             settings_->location.maxRangeKm,
             settings_->schedule.enabled ? "true" : "false",
             static_cast<unsigned long>(settings_->api.computedRequestIntervalMs),
             static_cast<unsigned long>(activeRequestIntervalMs(*settings_)),
             scheduleIdleDisplayModeName(settings_->schedule.idleDisplayMode),
             nvsJsonText());
    server_.send(200, "application/json", body);
}

void ConfigPortal::handleRestart()
{
    updateLanguageFromRequest();
    restartRequested_ = true;
    sendPageHeader(text("Restart requested", "已请求重启"));
    write("<h1>");
    write(text("Restart requested", "已请求重启"));
    write("</h1><p>");
    write(text("The device will restart shortly.", "设备即将重启。"));
    write("</p>");
    sendPageFooter();
}

void ConfigPortal::handleNotFound()
{
    server_.sendHeader("Location", "/", true);
    server_.send(302, "text/plain", "");
}

void ConfigPortal::sendStatusPanel(uint32_t intervalMs)
{
    if (settings_ == nullptr)
    {
        return;
    }

    char value[40];
    const UserSettings &settings = *settings_;
    write("<fieldset><legend>");
    write(text("Current Status", "当前状态"));
    write("</legend><p>");
    write(text("Mode", "模式"));
    write(": ");
    write(mode_ == ConfigPortalMode::ApSetup ? text("AP Setup", "AP 配置") : text("STA Settings", "局域网设置"));
    write(" / WiFi: ");
    write(WiFi.status() == WL_CONNECTED ? text("Connected", "已连接") :
                                           (settings.wifi.configured ? text("Configured", "已配置") : text("Not configured", "未配置")));
    write("</p><p>IP: ");
    write(mode_ == ConfigPortalMode::StaSettings ? staIpAddress_ : ipAddress_);
    write("</p><p>");
    write(text("Data Source", "数据源"));
    write(": ");
    write(apiProviderName(settings.api.provider));
    write(" / ");
    write(text("Refresh", "刷新"));
    write(": ");
    snprintf(value, sizeof(value), "%.1fs", static_cast<float>(intervalMs) / 1000.0f);
    write(value);
    write("</p></fieldset>");
}

void ConfigPortal::renderSimplePage()
{
    if (settings_ == nullptr)
    {
        server_.send(500, "text/plain", "settings not available");
        return;
    }

    char value[40];
    const UserSettings &settings = *settings_;
    const uint32_t activeSeconds = computeActiveSecondsPerDay(settings.schedule);
    const uint32_t intervalMs = activeRequestIntervalMs(settings);
    const uint32_t estimatedRequests = intervalMs > 0 ? (activeSeconds * 1000UL) / intervalMs : 0;
    uint8_t selectedRangePreset = 0;
    float selectedRangeDelta = fabsf(settings.location.maxRangeKm - settings.location.rangePresetsKm[0]);
    for (uint8_t i = 1; i < 3; ++i)
    {
        const float delta = fabsf(settings.location.maxRangeKm - settings.location.rangePresetsKm[i]);
        if (delta < selectedRangeDelta)
        {
            selectedRangePreset = i;
            selectedRangeDelta = delta;
        }
    }

    sendPageHeader(text("Aircraft Radar Setup", "航班雷达设置"));
    sendLanguageSwitch("/");
    write("<h1>");
    write(text("Aircraft Radar Setup", "航班雷达设置"));
    write("</h1><p>");
    if (mode_ == ConfigPortalMode::ApSetup)
    {
        if (!settings.wifi.configured)
        {
            write(text("First setup: enter your WiFi, radar location, data source, and display settings. Save settings, then restart the device so it can connect to your WiFi.",
                       "首次配置：请填写 WiFi、雷达位置、数据源和显示设置。保存后请重启设备，设备会尝试连接你的 WiFi。"));
        }
        else
        {
            write(text("Setup portal is active. Save changes, then restart the device to fully apply WiFi and startup settings.",
                       "当前处于配置页面。修改后请保存设置，并重启设备以完整应用 WiFi 和启动相关设置。"));
        }
    }
    else
    {
        write(text("Update settings here. Display and filter values are saved immediately; WiFi and startup behavior should be verified after restart.",
                   "可在这里修改设备设置。显示和过滤参数会立即保存；WiFi 和启动相关配置建议重启后确认。"));
    }
    write("</p>");
    sendStatusPanel(intervalMs);
    write("<form method=\"POST\" action=\"/saveSimple\">");
    sendHiddenLanguage();

    write("<fieldset><legend>WiFi</legend>");
    write("<p>");
    write(settings.wifi.configured ? text("WiFi is configured.", "WiFi 已配置。") :
                                     text("WiFi is not configured yet.", "WiFi 尚未配置。"));
    write("</p>");
    sendTextInput(text("WiFi SSID", "WiFi 名称"), "wifi_ssid", settings.wifi.ssid, false);
    write("<label>");
    write(text("WiFi Password", "WiFi 密码"));
    write("<span class=\"passwordRow\"><input id=\"wifiPassword\" name=\"wifi_password\" type=\"password\" value=\"");
    write(settings.wifi.configured ? settings.wifi.password : "");
    write("\" placeholder=\"");
    write(settings.wifi.configured ? text("Password hidden", "密码已隐藏") : text("WiFi password", "WiFi 密码"));
    write("\"><button type=\"button\" onclick=\"toggleWifiPassword()\" id=\"wifiPasswordToggle\">");
    write(text("Show", "查看"));
    write("</button></span></label><p class=\"hint\">");
    write(settings.wifi.configured ? text("Saved password is hidden by default. Tap Show to view or edit it.",
                                          "已保存的密码默认隐藏。点击查看可以显示或修改。") :
                                     text("Enter a WiFi password before saving.",
                                          "保存前请输入 WiFi 密码。"));
    write("</p>");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Radar Location", "雷达位置"));
    write("</legend>");
    snprintf(value, sizeof(value), "%.6f", settings.location.centerLat);
    sendNumberInput(text("Latitude", "纬度"), "centerLat", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.centerLon);
    sendNumberInput(text("Longitude", "经度"), "centerLon", value, "0.000001");
    write("<button type=\"button\" onclick=\"usePhoneLocation()\">");
    write(text("Use this phone location", "使用手机当前位置"));
    write("</button><p id=\"geoStatus\" class=\"hint\"></p>");
    write("<label>");
    write(text("Display range", "显示范围"));
    write("<select name=\"displayRangePreset\">");
    snprintf(value, sizeof(value), text("Preset 1 - %.0f km", "档位 1 - %.0f km"), settings.location.rangePresetsKm[0]);
    sendSelectOption("0", value, selectedRangePreset == 0);
    snprintf(value, sizeof(value), text("Preset 2 - %.0f km", "档位 2 - %.0f km"), settings.location.rangePresetsKm[1]);
    sendSelectOption("1", value, selectedRangePreset == 1);
    snprintf(value, sizeof(value), text("Preset 3 - %.0f km", "档位 3 - %.0f km"), settings.location.rangePresetsKm[2]);
    sendSelectOption("2", value, selectedRangePreset == 2);
    write("</select></label>");
    snprintf(value, sizeof(value), "%.1f", settings.location.rangePresetsKm[0]);
    sendKmInput(text("Range preset 1", "档位 1"), "rangePreset1Km", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.location.rangePresetsKm[1]);
    sendKmInput(text("Range preset 2", "档位 2"), "rangePreset2Km", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.location.rangePresetsKm[2]);
    sendKmInput(text("Range preset 3", "档位 3"), "rangePreset3Km", value, "0.1");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Data Account", "数据账号"));
    write("</legend>");
    write("<p>");
    write(text("Choose adsb.fi Open Data for no-login public data, or OpenSky if you want to use the OpenSky backup source.",
               "选择 adsb.fi Open Data 可无需登录使用公共数据；如需备用数据源，可切换到 OpenSky。"));
    write("</p><label>Data Source<select name=\"dataSource\" onchange=\"toggleDataSource(this.value)\">");
    sendSelectOption("adsbfi", "adsb.fi Open Data", settings.api.provider == ApiProvider::AdsbFi);
    sendSelectOption("opensky", "OpenSky", settings.api.provider == ApiProvider::OpenSky);
    write("</select></label>");
    snprintf(value, sizeof(value), "%.1f", static_cast<float>(activeRequestIntervalMs(settings)) / 1000.0f);
    write("<label>");
    write(text("Refresh interval seconds", "刷新间隔秒数"));
    write("<input name=\"apiIntervalSec\" type=\"number\" min=\"1\" max=\"3600\" step=\"0.1\" value=\"");
    write(value);
    write("\"></label>");
    write("<p class=\"hint\" id=\"dataSourceHint\"></p><div id=\"openSkyAccountFields\"><label>API mode");
    write("<select name=\"apiMode\" onchange=\"toggleClientFields(this.value)\">");
    sendSelectOption("anonymous", text("Anonymous Free Mode", "免费匿名模式"), settings.api.accountMode == ApiAccountMode::Anonymous);
    sendSelectOption("client", text("Use My OpenSky API Client", "使用我的 OpenSky API Client"), settings.api.accountMode != ApiAccountMode::Anonymous);
    write("</select></label><div id=\"clientFields\">");
    sendTextInput("OpenSky Client ID", "openSkyClientId", settings.api.openSkyClientId, false);
    sendTextInput("OpenSky Client Secret", "openSkyClientSecret", "", true);
    write("</div></div></fieldset>");

    write("<fieldset><legend>");
    write(text("Run Schedule", "运行时段"));
    write("</legend><label>");
    write(text("Schedule mode", "运行模式"));
    write("<select name=\"scheduleMode\">");
    sendSelectOption("always", text("Always run", "全天运行"), !settings.schedule.enabled);
    sendSelectOption("window", text("Run only during selected hours", "仅在指定时段运行"), settings.schedule.enabled);
    write("</select></label>");
    write("<div id=\"scheduleFields\">");
    snprintf(value, sizeof(value), "%d", settings.schedule.startMinutesOfDay / 60);
    sendNumberInput(text("Start hour", "开始小时"), "startHour", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.startMinutesOfDay % 60);
    sendNumberInput(text("Start minute", "开始分钟"), "startMinute", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.endMinutesOfDay / 60);
    sendNumberInput(text("End hour", "结束小时"), "endHour", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.endMinutesOfDay % 60);
    sendNumberInput(text("End minute", "结束分钟"), "endMinute", value, "1");
    write("</div>");
    write("<label>");
    write(text("Timezone", "时区"));
    write("<select name=\"timezoneOffsetMinutes\" id=\"timezoneOffsetMinutes\">");
    sendSelectOption("0", "UTC+0", settings.schedule.timezoneOffsetMinutes == 0);
    sendSelectOption("480", "UTC+8", settings.schedule.timezoneOffsetMinutes == 480);
    sendSelectOption("540", "UTC+9", settings.schedule.timezoneOffsetMinutes == 540);
    snprintf(value, sizeof(value), "%d", settings.schedule.timezoneOffsetMinutes);
    sendSelectOption(value, "Custom", settings.schedule.timezoneOffsetMinutes != 0 &&
                                    settings.schedule.timezoneOffsetMinutes != 480 &&
                                    settings.schedule.timezoneOffsetMinutes != 540);
    write("</select></label><button type=\"button\" onclick=\"setBrowserTimezone()\">");
    write(text("Use browser timezone", "使用浏览器时区"));
    write("</button><label>");
    write(text("Outside schedule display", "超出时段显示"));
    write("<select name=\"idleDisplayMode\">");
    sendSelectOption("0",
                     text("Paused status", "暂停状态"),
                     settings.schedule.idleDisplayMode == ScheduleIdleDisplayMode::PausedStatus);
    sendSelectOption("1",
                     text("Clock", "时钟"),
                     settings.schedule.idleDisplayMode == ScheduleIdleDisplayMode::Clock);
    sendSelectOption("2",
                     text("Screen off", "黑屏"),
                     settings.schedule.idleDisplayMode == ScheduleIdleDisplayMode::DisplayOff);
    write("</select></label></fieldset>");

    write("<fieldset><legend>");
    write(text("Display", "显示"));
    write("</legend><label>");
    write(text("UI Theme", "界面风格"));
    write("<select name=\"uiTheme\">");
    sendSelectOption("0", text("Classic Radar", "经典雷达"), settings.display.uiTheme == UiTheme::ClassicRadar);
    sendSelectOption("1", text("Modern Display", "现代显示"), settings.display.uiTheme == UiTheme::ModernRadar);
    sendSelectOption("2", "Cyberpunk", settings.display.uiTheme == UiTheme::CyberpunkRadar);
    write("</select></label><label>");
    write(text("Brightness", "亮度"));
    write("<select name=\"brightness\">");
    sendSelectOption("64", text("Low", "低"), settings.display.brightness <= 100);
    sendSelectOption("160", text("Medium", "中"), settings.display.brightness > 100 && settings.display.brightness < 220);
    sendSelectOption("255", text("High", "高"), settings.display.brightness >= 220);
    write("</select></label></fieldset>");

    write("<fieldset><legend>");
    write(text("Usage Estimate", "用量估算"));
    write("</legend>");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.dailyCreditBudget));
    write("<input id=\"currentDailyCreditBudget\" type=\"hidden\" value=\"");
    write(value);
    write("\">");
    write("<input id=\"currentApiMode\" type=\"hidden\" value=\"");
    write(settings.api.accountMode == ApiAccountMode::Anonymous ? "anonymous" : "client");
    write("\">");
    snprintf(value, sizeof(value), "%.2f", settings.api.creditReserveRatio);
    write("<input id=\"creditReserveRatio\" type=\"hidden\" value=\"");
    write(value);
    write("\">");
    snprintf(value, sizeof(value), "%.1f", settings.api.requestCostCredits);
    write("<input id=\"requestCostCredits\" type=\"hidden\" value=\"");
    write(value);
    write("\">");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.minUsefulIntervalMs));
    write("<input id=\"currentMinUsefulIntervalMs\" type=\"hidden\" value=\"");
    write(value);
    write("\">");
    write("<input id=\"anonymousDailyCreditBudget\" type=\"hidden\" value=\"400\">");
    write("<input id=\"clientDailyCreditBudget\" type=\"hidden\" value=\"4000\">");
    write("<input id=\"anonymousMinIntervalMs\" type=\"hidden\" value=\"10000\">");
    write("<input id=\"clientMinIntervalMs\" type=\"hidden\" value=\"5000\">");
    write("<p>");
    write(text("API mode", "API 模式"));
    write(": <span id=\"usageApiMode\">");
    write(apiAccountModeName(settings.api.accountMode));
    write("</span></p><p>");
    write(text("Daily credit budget", "每日额度"));
    write(": <span id=\"usageDailyBudget\">");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.dailyCreditBudget));
    write(value);
    write("</span></p><p>");
    write(text("Active hours per day", "每日运行小时"));
    write(": <span id=\"usageActiveHours\">");
    snprintf(value, sizeof(value), "%.1f", static_cast<float>(activeSeconds) / 3600.0f);
    write(value);
    write(" h</span></p><p>");
    write(text("Recommended refresh interval", "推荐刷新间隔"));
    write(": <span id=\"usageRefreshInterval\">");
    snprintf(value, sizeof(value), "%lus", static_cast<unsigned long>(settings.api.computedRequestIntervalMs / 1000UL));
    write(value);
    write("</span></p><p>");
    write(text("Estimated daily requests", "预计每日请求数"));
    write(": <span id=\"usageDailyRequests\">");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(estimatedRequests));
    write(value);
    write("</span></p><p>NVS: <span id=\"usageNvs\">");
    write(nvsStateText());
    write("</span></p></fieldset>");

    write("<p><button type=\"submit\">");
    write(text("Save Settings", "保存设置"));
    write("</button> <a href=\"/restart?lang=");
    write(languageCode());
    write("\">");
    write(text("Restart Now", "立即重启"));
    write("</a></p><p><a href=\"/advanced?lang=");
    write(languageCode());
    write("\">");
    write(text("Advanced settings", "高级设置"));
    write("</a></p></form>");

    write("<script>");
    write("function toggleWifiPassword(){var p=document.getElementById('wifiPassword');var b=document.getElementById('wifiPasswordToggle');if(!p||!b){return;}var show=p.type=='password';p.type=show?'text':'password';b.innerText=show?'");
    write(text("Hide", "隐藏"));
    write("':'");
    write(text("Show", "查看"));
    write("';}");
    write("function byName(n){return document.querySelector('[name='+n+']');}");
    write("function numberValue(n,f){var e=byName(n);var v=e?parseFloat(e.value):NaN;return isNaN(v)?f:v;}");
    write("function hiddenNumber(id,f){var e=document.getElementById(id);var v=e?parseFloat(e.value):NaN;return isNaN(v)?f:v;}");
    write("function activeSecondsPerDay(){var m=byName('scheduleMode');if(!m||m.value!='window'){return 86400;}var s=numberValue('startHour',0)*60+numberValue('startMinute',0);var e=numberValue('endHour',0)*60+numberValue('endMinute',0);s=((Math.floor(s)%1440)+1440)%1440;e=((Math.floor(e)%1440)+1440)%1440;if(s==e){return 86400;}if(e>s){return (e-s)*60;}return (1440-s+e)*60;}");
    write("function updateScheduleVisibility(){var m=byName('scheduleMode');var f=document.getElementById('scheduleFields');if(f&&m){f.style.display=(m.value=='window')?'block':'none';}}");
    write("function updateUsageEstimate(){var ds=byName('dataSource');var adsb=!ds||ds.value=='adsbfi';var api=byName('apiMode');var client=api&&api.value=='client';var active=activeSecondsPerDay();var reserve=hiddenNumber('creditReserveRatio',0.90);var cost=hiddenNumber('requestCostCredits',1.0);var budget=client?hiddenNumber('clientDailyCreditBudget',4000):hiddenNumber('anonymousDailyCreditBudget',400);var minMs=client?hiddenNumber('clientMinIntervalMs',5000):hiddenNumber('anonymousMinIntervalMs',10000);if(adsb){budget=0;minMs=1000;}var requestedMs=numberValue('apiIntervalSec',adsb?10:(minMs/1000))*1000;var currentMode=document.getElementById('currentApiMode');currentMode=currentMode?currentMode.value:'';var sameMode=(client&&currentMode=='client')||(!client&&currentMode=='anonymous');if(!adsb&&sameMode){var currentBudget=hiddenNumber('currentDailyCreditBudget',0);var currentMin=hiddenNumber('currentMinUsefulIntervalMs',0);if(currentBudget>0){budget=currentBudget;}if(currentMin>minMs){minMs=currentMin;}}var intervalMs=requestedMs;if(intervalMs<minMs){intervalMs=minMs;}if(intervalMs>3600000){intervalMs=3600000;}var requests=Math.floor((active*1000)/intervalMs);document.getElementById('usageApiMode').innerText=adsb?'adsb.fi Open Data':(client?'OpenSkyClient':'OpenSky Anonymous');document.getElementById('usageDailyBudget').innerText=adsb?'N/A':String(Math.round(budget));document.getElementById('usageActiveHours').innerText=(active/3600).toFixed(1)+' h';document.getElementById('usageRefreshInterval').innerText=(intervalMs/1000).toFixed(intervalMs%1000?1:0)+'s';document.getElementById('usageDailyRequests').innerText=String(requests);updateScheduleVisibility();}");
    write("function bindUsageEstimate(){['scheduleMode','startHour','startMinute','endHour','endMinute','apiMode','dataSource','apiIntervalSec'].forEach(function(n){var e=byName(n);if(e){e.addEventListener('change',updateUsageEstimate);e.addEventListener('input',updateUsageEstimate);}});['creditReserveRatio','requestCostCredits','currentDailyCreditBudget','currentApiMode','anonymousDailyCreditBudget','clientDailyCreditBudget','anonymousMinIntervalMs','clientMinIntervalMs','currentMinUsefulIntervalMs'].forEach(function(id){var e=document.getElementById(id);if(e){e.addEventListener('change',updateUsageEstimate);e.addEventListener('input',updateUsageEstimate);}});}");
    write("var geoSecureMsg=\"");
    write(text("Browser blocks GPS on HTTP IP pages. Enter latitude/longitude manually.",
               "浏览器会阻止 HTTP IP 页面获取定位，请手动输入经纬度。"));
    write("\";var geoUnavailableMsg=\"");
    write(text("Geolocation is not available in this browser.",
               "当前浏览器不支持定位。"));
    write("\";var geoRequestMsg=\"");
    write(text("Requesting location permission...",
               "正在请求定位权限..."));
    write("\";var geoFilledMsg=\"");
    write(text("Location filled. Accuracy: ",
               "已填入当前位置，精度约 "));
    write("\";var geoFailedMsg=\"");
    write(text("Location failed",
               "定位失败"));
    write("\";var geoDeniedMsg=\"");
    write(text("Location permission denied",
               "定位权限被拒绝"));
    write("\";var geoUnavailable2Msg=\"");
    write(text("Location unavailable",
               "无法获取当前位置"));
    write("\";var geoTimeoutMsg=\"");
    write(text("Location timeout",
               "定位超时"));
    write("\";var geoManualMsg=\"");
    write(text(". Enter latitude/longitude manually.",
               "，请手动输入经纬度。"));
    write("\";var hintOpenSky=\"");
    write(text("OpenSky can use anonymous mode or your API Client.",
               "OpenSky 可以使用匿名模式，也可以使用你的 API Client。"));
    write("\";var hintAdsb=\"");
    write(text("adsb.fi public endpoint does not require login.",
               "adsb.fi 公共接口不需要登录。"));
    write("\";");
    write("function usePhoneLocation(){var s=document.getElementById('geoStatus');if(!window.isSecureContext){s.innerText=geoSecureMsg;return;}if(!navigator.geolocation){s.innerText=geoUnavailableMsg;return;}s.innerText=geoRequestMsg;navigator.geolocation.getCurrentPosition(function(p){document.querySelector('[name=centerLat]').value=p.coords.latitude.toFixed(6);document.querySelector('[name=centerLon]').value=p.coords.longitude.toFixed(6);s.innerText=geoFilledMsg+Math.round(p.coords.accuracy)+' m';},function(e){var m=geoFailedMsg;if(e.code==1){m=geoDeniedMsg;}else if(e.code==2){m=geoUnavailable2Msg;}else if(e.code==3){m=geoTimeoutMsg;}s.innerText=m+geoManualMsg;},{enableHighAccuracy:true,timeout:12000,maximumAge:30000});}");
    write("function setBrowserTimezone(){var o=-new Date().getTimezoneOffset();var e=document.getElementById('timezoneOffsetMinutes');var found=false;for(var i=0;i<e.options.length;i++){if(e.options[i].value==String(o)){e.selectedIndex=i;found=true;}}if(!found){var opt=document.createElement('option');opt.value=String(o);opt.text='UTC'+(o>=0?'+':'')+(o/60);opt.selected=true;e.add(opt);}}");
    write("function toggleClientFields(v){var c=document.getElementById('clientFields');if(c){c.style.display=(v=='client')?'block':'none';}updateUsageEstimate();}");
    write("function toggleDataSource(v){var open=v=='opensky';var f=document.getElementById('openSkyAccountFields');var h=document.getElementById('dataSourceHint');if(f){f.style.display=open?'block':'none';}if(h){h.innerText=open?hintOpenSky:hintAdsb;}updateUsageEstimate();}");
    write("bindUsageEstimate();toggleClientFields(document.querySelector('[name=apiMode]').value);toggleDataSource(document.querySelector('[name=dataSource]').value);updateUsageEstimate();");
    write("</script>");
    sendPageFooter();
}

void ConfigPortal::renderAdvancedPage()
{
    if (settings_ == nullptr)
    {
        server_.send(500, "text/plain", "settings not available");
        return;
    }

    char value[40];
    const UserSettings &settings = *settings_;
    sendPageHeader(text("Advanced settings", "高级设置"));
    sendLanguageSwitch("/advanced");
    write("<p><a href=\"/?lang=");
    write(languageCode());
    write("\">");
    write(text("Back to simple setup", "返回简单设置"));
    write("</a></p><h1>");
    write(text("Advanced settings", "高级设置"));
    write("</h1><form method=\"POST\" action=\"/saveAdvanced\">");
    sendHiddenLanguage();

    write("<fieldset><legend>");
    write(text("Display / Filter", "显示 / 过滤"));
    write("</legend>");
    snprintf(value, sizeof(value), "%u", settings.display.maxAircraftToDisplay);
    sendNumberInput(text("Max aircraft to display", "最大显示飞机数"), "maxAircraftToDisplay", value, "1");
    sendCheckbox(text("Show labels", "显示标签"), "showLabels", settings.display.showLabels);
    write("<p class=\"hint\">");
    write(text("Labels show aircraft callsign near the target. Selected aircraft may also show altitude, speed, or distance depending on the current UI theme.",
               "标签会在飞机目标附近显示航班号。被选中的飞机可能还会根据当前界面风格显示高度、速度或距离。"));
    write("</p>");
    const uint8_t brightnessPercent = static_cast<uint8_t>((static_cast<uint16_t>(settings.display.brightness) * 100U + 127U) / 255U);
    snprintf(value, sizeof(value), "%u", brightnessPercent);
    write("<label>");
    write(text("Brightness", "显示亮度"));
    write("<span class=\"rangeLine\"><input name=\"brightnessPercent\" type=\"range\" min=\"0\" max=\"100\" step=\"1\" value=\"");
    write(value);
    write("\" oninput=\"document.getElementById('brightnessPercentValue').innerText=this.value+'%'\"> <span id=\"brightnessPercentValue\">");
    write(value);
    write("%</span></span></label>");
    sendCheckbox(text("Show ground traffic", "显示地面目标"), "showGroundTraffic", settings.filter.showGroundTraffic);
    snprintf(value, sizeof(value), "%.1f", settings.filter.minAirborneAltitudeM);
    sendNumberInput(text("Minimum airborne altitude m", "最小空中高度 m"), "minAirborneAltitudeM", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.filter.minAirborneSpeedMs);
    sendNumberInput(text("Minimum airborne speed m/s", "最小空中速度 m/s"), "minAirborneSpeedMs", value, "0.1");
    write("<p class=\"hint\">");
    write(text("Filtering removes aircraft below the minimum altitude or below the minimum speed. This helps hide ground, parked, or very slow airport targets.",
               "过滤会隐藏低于最小高度或低于最小速度的目标，用于减少机场地面、停机或低速目标堆在雷达上的情况。"));
    write("</p>");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Prediction", "位置预测"));
    write("</legend>");
    sendCheckbox(text("Prediction enabled", "启用位置预测"), "predictionEnabled", settings.prediction.enabled);
    snprintf(value, sizeof(value), "%.2f", settings.prediction.followAlpha);
    sendNumberInput(text("Follow alpha", "跟随平滑系数"), "followAlpha", value, "0.01");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.predictionMaxMs));
    sendNumberInput(text("Prediction max ms", "最大预测时间 ms"), "predictionMaxMs", value, "1000");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.jumpResetDistanceKm);
    sendKmInput(text("Jump reset distance", "跳变重置距离"), "jumpResetDistanceKm", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.lowSpeedThresholdMs);
    sendNumberInput(text("Low speed threshold m/s", "低速阈值 m/s"), "lowSpeedThresholdMs", value, "0.1");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.staleTimeoutMs));
    sendNumberInput(text("Stale timeout ms", "数据过期时间 ms"), "staleTimeoutMs", value, "1000");
    sendCheckbox(text("Correction enabled", "启用修正动画"), "correctionEnabled", settings.prediction.correctionEnabled);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.correctionMinApiIntervalMs));
    sendNumberInput(text("Correction min API interval ms", "修正最小 API 间隔 ms"), "correctionMinApiIntervalMs", value, "1000");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.correctionDurationMs));
    sendNumberInput(text("Correction duration ms", "修正动画时长 ms"), "correctionDurationMs", value, "1000");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.correctionStartDistanceKm);
    sendKmInput(text("Correction start distance", "修正起始距离"), "correctionStartDistanceKm", value, "0.1");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("System", "系统"));
    write("</legend>");
    snprintf(value, sizeof(value), "%d", settings.system.uiButtonPin);
    sendNumberInput(text("UI button pin", "UI 按键引脚"), "uiButtonPin", value, "1");
    sendCheckbox(text("Serial debug", "串口调试"), "serialDebug", settings.system.serialDebug);
    write("</fieldset><p><button type=\"submit\">");
    write(text("Save advanced settings", "保存高级设置"));
    write("</button></p></form>");
    sendPageFooter();
}

void ConfigPortal::renderSavedPage(bool saved)
{
    sendPageHeader(saved ? text("Saved", "已保存") : text("Save failed", "保存失败"));
    write(saved ? text("<h1>Saved</h1>", "<h1>已保存</h1>") : text("<h1>Save failed</h1>", "<h1>保存失败</h1>"));
    write("<p>NVS: ");
    write(nvsStateText());
    write("</p><p>");
    if (!saved)
    {
        write(text("Please check the serial log for details.", "请查看串口日志了解详情。"));
    }
    else
    {
        write(text("Settings were saved. Restart the device to fully apply WiFi, startup, and connection-related changes.",
                   "设置已保存。WiFi、启动和连接相关改动需要重启设备后才能完整生效。"));
    }
    if (saved && strcmp(nvsStateText(), "enabled") != 0)
    {
        write("</p><p>");
        write(text("NVS is disabled. Settings are only valid until reboot.", "NVS 未启用，设置只在本次运行期间有效。"));
    }
    write("</p><p><a href=\"/?lang=");
    write(languageCode());
    write("\">");
    write(text("Back", "返回"));
    write("</a> <a href=\"/restart?lang=");
    write(languageCode());
    write("\">");
    write(text("Restart Now", "立即重启"));
    write("</a></p>");
    sendPageFooter();
}

void ConfigPortal::applySimpleFormToSettings()
{
    UserSettings &settings = *settings_;
    copyArgToBuffer("wifi_ssid", settings.wifi.ssid, sizeof(settings.wifi.ssid), false);
    copyArgToBuffer("wifi_password", settings.wifi.password, sizeof(settings.wifi.password), true);
    settings.wifi.configured = settings.wifi.ssid[0] != '\0';

    settings.location.centerLat = argToFloat("centerLat", settings.location.centerLat);
    settings.location.centerLon = argToFloat("centerLon", settings.location.centerLon);
    settings.location.rangePresetsKm[0] = argToFloat("rangePreset1Km", settings.location.rangePresetsKm[0]);
    settings.location.rangePresetsKm[1] = argToFloat("rangePreset2Km", settings.location.rangePresetsKm[1]);
    settings.location.rangePresetsKm[2] = argToFloat("rangePreset3Km", settings.location.rangePresetsKm[2]);
    const uint8_t selectedRangePreset = static_cast<uint8_t>(argToInt("displayRangePreset", 0));
    if (selectedRangePreset < 3)
    {
        settings.location.maxRangeKm = settings.location.rangePresetsKm[selectedRangePreset];
    }
    updateQueryBoxFromCenterRange(settings);

    const String dataSource = server_.hasArg("dataSource") ? server_.arg("dataSource") : "adsbfi";
    settings.api.provider = dataSource == "opensky" ? ApiProvider::OpenSky : ApiProvider::AdsbFi;
    const String apiMode = server_.hasArg("apiMode") ? server_.arg("apiMode") : "anonymous";
    uint32_t requestedIntervalMs = static_cast<uint32_t>(argToFloat("apiIntervalSec",
                                                                    static_cast<float>(activeRequestIntervalMs(settings)) / 1000.0f) *
                                                         1000.0f);
    requestedIntervalMs = max<uint32_t>(requestedIntervalMs, 1000);
    requestedIntervalMs = min<uint32_t>(requestedIntervalMs, 3600000);
    if (settings.api.provider == ApiProvider::AdsbFi)
    {
        settings.api.accountMode = ApiAccountMode::Anonymous;
        settings.api.refreshPolicy = RefreshPolicy::ManualInterval;
        settings.api.dailyCreditBudget = 400;
        settings.api.minUsefulIntervalMs = 1000;
        settings.api.manualRequestIntervalMs = requestedIntervalMs;
    }
    else if (apiMode == "client")
    {
        settings.api.refreshPolicy = RefreshPolicy::ManualInterval;
        settings.api.accountMode = ApiAccountMode::OpenSkyClient;
        settings.api.dailyCreditBudget = 4000;
        settings.api.minUsefulIntervalMs = 5000;
        settings.api.manualRequestIntervalMs = requestedIntervalMs;
        copyArgToBuffer("openSkyClientId", settings.api.openSkyClientId, sizeof(settings.api.openSkyClientId), false);
        copyArgToBuffer("openSkyClientSecret", settings.api.openSkyClientSecret, sizeof(settings.api.openSkyClientSecret), true);
    }
    else
    {
        settings.api.refreshPolicy = RefreshPolicy::ManualInterval;
        settings.api.accountMode = ApiAccountMode::Anonymous;
        settings.api.dailyCreditBudget = 400;
        settings.api.minUsefulIntervalMs = 10000;
        settings.api.manualRequestIntervalMs = requestedIntervalMs;
    }

    const String scheduleMode = server_.hasArg("scheduleMode") ? server_.arg("scheduleMode") : "always";
    settings.schedule.enabled = scheduleMode == "window";
    settings.schedule.startMinutesOfDay = argToInt("startHour", settings.schedule.startMinutesOfDay / 60) * 60 +
                                          argToInt("startMinute", settings.schedule.startMinutesOfDay % 60);
    settings.schedule.endMinutesOfDay = argToInt("endHour", settings.schedule.endMinutesOfDay / 60) * 60 +
                                        argToInt("endMinute", settings.schedule.endMinutesOfDay % 60);
    settings.schedule.timezoneOffsetMinutes = argToInt("timezoneOffsetMinutes", settings.schedule.timezoneOffsetMinutes);
    settings.schedule.idleDisplayMode = static_cast<ScheduleIdleDisplayMode>(
        argToInt("idleDisplayMode", static_cast<int>(settings.schedule.idleDisplayMode)));

    settings.display.uiTheme = static_cast<UiTheme>(argToInt("uiTheme", static_cast<int>(settings.display.uiTheme)));
    settings.display.brightness = static_cast<uint8_t>(argToInt("brightness", settings.display.brightness));
}

void ConfigPortal::applyAdvancedFormToSettings()
{
    UserSettings &settings = *settings_;
    settings.location.centerLat = argToFloat("centerLat", settings.location.centerLat);
    settings.location.centerLon = argToFloat("centerLon", settings.location.centerLon);
    settings.location.maxRangeKm = argToFloat("maxRangeKm", settings.location.maxRangeKm);
    settings.location.rangePresetsKm[0] = argToFloat("rangePreset1Km", settings.location.rangePresetsKm[0]);
    settings.location.rangePresetsKm[1] = argToFloat("rangePreset2Km", settings.location.rangePresetsKm[1]);
    settings.location.rangePresetsKm[2] = argToFloat("rangePreset3Km", settings.location.rangePresetsKm[2]);
    settings.location.queryLatMin = argToFloat("queryLatMin", settings.location.queryLatMin);
    settings.location.queryLonMin = argToFloat("queryLonMin", settings.location.queryLonMin);
    settings.location.queryLatMax = argToFloat("queryLatMax", settings.location.queryLatMax);
    settings.location.queryLonMax = argToFloat("queryLonMax", settings.location.queryLonMax);

    settings.api.provider = static_cast<ApiProvider>(argToInt("apiProvider", static_cast<int>(settings.api.provider)));
    settings.api.accountMode = static_cast<ApiAccountMode>(argToInt("accountMode", static_cast<int>(settings.api.accountMode)));
    settings.api.refreshPolicy = static_cast<RefreshPolicy>(argToInt("refreshPolicy", static_cast<int>(settings.api.refreshPolicy)));
    copyArgToBuffer("openSkyClientId", settings.api.openSkyClientId, sizeof(settings.api.openSkyClientId), false);
    copyArgToBuffer("openSkyClientSecret", settings.api.openSkyClientSecret, sizeof(settings.api.openSkyClientSecret), true);
    settings.api.dailyCreditBudget = argToUInt("dailyCreditBudget", settings.api.dailyCreditBudget);
    settings.api.creditReserveRatio = argToFloat("creditReserveRatio", settings.api.creditReserveRatio);
    settings.api.requestCostCredits = argToFloat("requestCostCredits", settings.api.requestCostCredits);
    settings.api.manualRequestIntervalMs = argToUInt("manualRequestIntervalMs", settings.api.manualRequestIntervalMs);
    settings.api.minUsefulIntervalMs = argToUInt("minUsefulIntervalMs", settings.api.minUsefulIntervalMs);

    settings.display.uiTheme = static_cast<UiTheme>(argToInt("uiTheme", static_cast<int>(settings.display.uiTheme)));
    settings.display.maxAircraftToDisplay = static_cast<uint8_t>(argToInt("maxAircraftToDisplay", settings.display.maxAircraftToDisplay));
    settings.display.showLabels = hasCheckedArg("showLabels");
    if (server_.hasArg("brightnessPercent"))
    {
        const int brightnessPercent = constrain(argToInt("brightnessPercent", 100), 0, 100);
        settings.display.brightness = static_cast<uint8_t>((brightnessPercent * 255 + 50) / 100);
    }
    else
    {
        settings.display.brightness = static_cast<uint8_t>(argToInt("brightness", settings.display.brightness));
    }
    settings.filter.showGroundTraffic = hasCheckedArg("showGroundTraffic");
    settings.filter.minAirborneAltitudeM = argToFloat("minAirborneAltitudeM", settings.filter.minAirborneAltitudeM);
    settings.filter.minAirborneSpeedMs = argToFloat("minAirborneSpeedMs", settings.filter.minAirborneSpeedMs);

    settings.prediction.enabled = hasCheckedArg("predictionEnabled");
    settings.prediction.followAlpha = argToFloat("followAlpha", settings.prediction.followAlpha);
    settings.prediction.predictionMaxMs = argToUInt("predictionMaxMs", settings.prediction.predictionMaxMs);
    settings.prediction.jumpResetDistanceKm = argToFloat("jumpResetDistanceKm", settings.prediction.jumpResetDistanceKm);
    settings.prediction.lowSpeedThresholdMs = argToFloat("lowSpeedThresholdMs", settings.prediction.lowSpeedThresholdMs);
    settings.prediction.staleTimeoutMs = argToUInt("staleTimeoutMs", settings.prediction.staleTimeoutMs);
    settings.prediction.correctionEnabled = hasCheckedArg("correctionEnabled");
    settings.prediction.correctionMinApiIntervalMs = argToUInt("correctionMinApiIntervalMs", settings.prediction.correctionMinApiIntervalMs);
    settings.prediction.correctionDurationMs = argToUInt("correctionDurationMs", settings.prediction.correctionDurationMs);
    settings.prediction.correctionStartDistanceKm = argToFloat("correctionStartDistanceKm", settings.prediction.correctionStartDistanceKm);

    settings.system.uiButtonPin = argToInt("uiButtonPin", settings.system.uiButtonPin);
    settings.system.serialDebug = hasCheckedArg("serialDebug");
}

void ConfigPortal::updateLanguageFromRequest()
{
    if (!server_.hasArg("lang"))
    {
        return;
    }

    const String lang = server_.arg("lang");
    pageLanguage_ = lang == "zh" ? PageLanguage::Chinese : PageLanguage::English;
}

const char *ConfigPortal::text(const char *english, const char *chinese) const
{
    return pageLanguage_ == PageLanguage::Chinese ? chinese : english;
}

const char *ConfigPortal::languageCode() const
{
    return pageLanguage_ == PageLanguage::Chinese ? "zh" : "en";
}

const char *ConfigPortal::toggleLanguageCode() const
{
    return pageLanguage_ == PageLanguage::Chinese ? "en" : "zh";
}

const char *ConfigPortal::toggleLanguageLabel() const
{
    return pageLanguage_ == PageLanguage::Chinese ? "English" : "中文";
}

void ConfigPortal::sendPageHeader(const char *title)
{
    page_ = "";
    page_.reserve(32000);
    write("<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    write("<title>");
    write(title);
    write("</title><style>");
    write(":root{color-scheme:dark;--bg:#050b14;--panel:#0d1b2b;--panel2:#102438;--line:#244865;--text:#e8f6ff;--muted:#8fb3c8;--hint:#789aae;--accent:#28d8ff;--accent2:#35f0b4;--danger:#ff7a59}");
    write("*{box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;margin:0 auto;padding:14px 12px 26px;max-width:760px;background:radial-gradient(circle at 50% -16%,rgba(40,216,255,.18),transparent 360px),linear-gradient(180deg,#071426,#04080f);color:var(--text);font-size:15px;line-height:1.45}h1{font-size:24px;margin:8px 0 12px;letter-spacing:.02em}p{margin-top:0}form{display:grid;gap:12px}");
    write("fieldset{margin:0;padding:14px;border:1px solid var(--line);border-radius:12px;background:linear-gradient(180deg,rgba(16,36,56,.94),rgba(9,20,32,.96));box-shadow:0 10px 28px rgba(0,0,0,.26);display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:10px 14px}legend{font-weight:800;padding:0 6px;color:var(--accent)}label{display:block;color:var(--muted);font-size:13px;font-weight:650}input,select{width:100%;box-sizing:border-box;margin-top:5px;padding:10px;border:1px solid #2c5a78;border-radius:9px;background:#071522;color:var(--text);font-size:15px;outline:none}input:focus,select:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(40,216,255,.14)}");
    write("button{padding:10px 14px;border:0;border-radius:9px;background:linear-gradient(135deg,var(--accent),var(--accent2));color:#031018;font-weight:800}button[type=button]{background:#17304a;color:var(--text);border:1px solid #2e5f7e}a{display:inline-block;margin:4px 8px 4px 0;color:var(--accent);text-decoration:none;font-weight:700}.hint,fieldset>p,fieldset>div{grid-column:1/-1}.hint{font-size:13px;color:var(--hint)}.unit,.passwordRow,.rangeLine{display:flex;align-items:center;gap:8px}.unit input,.passwordRow input,.rangeLine input{flex:1}.unit span,.rangeLine span{margin-top:5px;color:var(--muted);font-size:14px;font-weight:800}.passwordRow button{min-width:70px;margin-top:5px;padding:10px 12px}input[type=range]{padding:0;accent-color:var(--accent)}@media(max-width:560px){body{padding:10px 10px 24px}fieldset{grid-template-columns:1fr}}</style></head><body>");
}

void ConfigPortal::sendPageFooter()
{
    write("</body></html>");
    server_.send(200, "text/html", page_);
    page_ = "";
}

void ConfigPortal::write(const char *content)
{
    if (content == nullptr)
    {
        return;
    }
    page_ += content;
}

void ConfigPortal::sendLanguageSwitch(const char *path)
{
    write("<p style=\"text-align:right\"><a href=\"");
    write(path);
    write("?lang=");
    write(toggleLanguageCode());
    write("\">");
    write(toggleLanguageLabel());
    write("</a></p>");
}

void ConfigPortal::sendHiddenLanguage()
{
    write("<input type=\"hidden\" name=\"lang\" value=\"");
    write(languageCode());
    write("\">");
}

void ConfigPortal::sendTextInput(const char *label, const char *name, const char *value, bool password)
{
    write("<label>");
    write(label);
    write("<input name=\"");
    write(name);
    write("\" type=\"");
    write(password ? "password" : "text");
    write("\" value=\"");
    write(value != nullptr ? value : "");
    write("\"></label>");
}

void ConfigPortal::sendNumberInput(const char *label, const char *name, const char *value, const char *step)
{
    write("<label>");
    write(label);
    write("<input name=\"");
    write(name);
    write("\" type=\"number\" step=\"");
    write(step);
    write("\" value=\"");
    write(value);
    write("\"></label>");
}

void ConfigPortal::sendKmInput(const char *label, const char *name, const char *value, const char *step)
{
    write("<label>");
    write(label);
    write("<span class=\"unit\"><input name=\"");
    write(name);
    write("\" type=\"number\" step=\"");
    write(step);
    write("\" value=\"");
    write(value);
    write("\"><span>km</span></span></label>");
}

void ConfigPortal::sendCheckbox(const char *label, const char *name, bool checked)
{
    write("<label><input style=\"width:auto\" name=\"");
    write(name);
    write("\" type=\"checkbox\"");
    if (checked)
    {
        write(" checked");
    }
    write("> ");
    write(label);
    write("</label>");
}

void ConfigPortal::sendSelectOption(const char *value, const char *label, bool selected)
{
    write("<option value=\"");
    write(value);
    write("\"");
    if (selected)
    {
        write(" selected");
    }
    write(">");
    write(label);
    write("</option>");
}

bool ConfigPortal::hasCheckedArg(const char *name)
{
    return server_.hasArg(name);
}

int ConfigPortal::argToInt(const char *name, int fallback)
{
    if (!server_.hasArg(name))
    {
        return fallback;
    }
    return server_.arg(name).toInt();
}

uint32_t ConfigPortal::argToUInt(const char *name, uint32_t fallback)
{
    const int value = argToInt(name, static_cast<int>(fallback));
    return value > 0 ? static_cast<uint32_t>(value) : fallback;
}

float ConfigPortal::argToFloat(const char *name, float fallback)
{
    if (!server_.hasArg(name))
    {
        return fallback;
    }
    return server_.arg(name).toFloat();
}

void ConfigPortal::copyArgToBuffer(const char *name, char *buffer, size_t bufferSize, bool keepOldIfEmpty)
{
    if (buffer == nullptr || bufferSize == 0 || !server_.hasArg(name))
    {
        return;
    }

    const String value = server_.arg(name);
    if (keepOldIfEmpty && value.length() == 0)
    {
        return;
    }

    strncpy(buffer, value.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
}
