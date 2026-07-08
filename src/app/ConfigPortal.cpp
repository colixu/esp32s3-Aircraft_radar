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
             "\"apiMode\":\"%s\",\"centerLat\":%.6f,\"centerLon\":%.6f,"
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

    sendPageHeader(text("Aircraft Radar Setup", "航班雷达设置"));
    sendLanguageSwitch("/");
    write("<h1>");
    write(text("Aircraft Radar Setup", "航班雷达设置"));
    write("</h1><p>");
    if (mode_ == ConfigPortalMode::ApSetup)
    {
        write(text("You are connected to the device setup WiFi. Save WiFi settings and restart.",
                   "请连接设备配置 WiFi，保存 WiFi 后重启设备。"));
        write("</p><p>AP: ");
        write(apSsid_);
        write(" / IP: ");
        write(ipAddress_);
    }
    else
    {
        write(text("You are connected through your home WiFi. You can update settings here.",
                   "当前通过家庭 WiFi 访问，可以在这里修改设置。"));
        write("</p><p>IP: ");
        write(staIpAddress_);
    }
    write("</p><form method=\"POST\" action=\"/saveSimple\">");
    sendHiddenLanguage();

    write("<fieldset><legend>WiFi</legend>");
    write("<p>");
    write(settings.wifi.configured ? text("WiFi is configured.", "WiFi 已配置。") :
                                     text("WiFi is not configured yet.", "WiFi 尚未配置。"));
    write("</p>");
    sendTextInput(text("WiFi SSID", "WiFi 名称"), "wifi_ssid", settings.wifi.ssid, false);
    sendTextInput(text("WiFi Password", "WiFi 密码"), "wifi_password", "", true);
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
    write("</button><p id=\"geoStatus\"></p>");
    write("<label>");
    write(text("Display range", "显示范围"));
    write("<select name=\"displayRangeKm\">");
    sendSelectOption("30", "30 km", fabsf(settings.location.maxRangeKm - 30.0f) < 0.5f);
    sendSelectOption("60", "60 km", fabsf(settings.location.maxRangeKm - 60.0f) < 0.5f);
    sendSelectOption("100", "100 km", fabsf(settings.location.maxRangeKm - 100.0f) < 0.5f);
    if (fabsf(settings.location.maxRangeKm - 30.0f) >= 0.5f &&
        fabsf(settings.location.maxRangeKm - 60.0f) >= 0.5f &&
        fabsf(settings.location.maxRangeKm - 100.0f) >= 0.5f)
    {
        snprintf(value, sizeof(value), "%.0f", settings.location.maxRangeKm);
        sendSelectOption(value, "Custom", true);
    }
    write("</select></label></fieldset>");

    write("<fieldset><legend>");
    write(text("Data Account", "数据账号"));
    write("</legend>");
    write("<p>");
    write(text("Authenticated mode requires an OpenSky API Client. Create a client in your OpenSky account, then paste the Client ID and Client Secret here. Do not enter your OpenSky website password.",
               "认证模式需要 OpenSky API Client。请在 OpenSky 账号中创建 API Client，然后填写 Client ID 和 Client Secret。不要填写 OpenSky 网站登录密码。"));
    write("</p><label>");
    write(text("API mode", "API 模式"));
    write("<select name=\"apiMode\" onchange=\"toggleClientFields(this.value)\">");
    sendSelectOption("anonymous", text("Anonymous Free Mode", "免费匿名模式"), settings.api.accountMode == ApiAccountMode::Anonymous);
    sendSelectOption("client", text("Use My OpenSky API Client", "使用我的 OpenSky API Client"), settings.api.accountMode != ApiAccountMode::Anonymous);
    write("</select></label><div id=\"clientFields\">");
    sendTextInput("OpenSky Client ID", "openSkyClientId", settings.api.openSkyClientId, false);
    sendTextInput("OpenSky Client Secret", "openSkyClientSecret", "", true);
    write("</div></fieldset>");

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
    write("function byName(n){return document.querySelector('[name='+n+']');}");
    write("function numberValue(n,f){var e=byName(n);var v=e?parseFloat(e.value):NaN;return isNaN(v)?f:v;}");
    write("function hiddenNumber(id,f){var e=document.getElementById(id);var v=e?parseFloat(e.value):NaN;return isNaN(v)?f:v;}");
    write("function activeSecondsPerDay(){var m=byName('scheduleMode');if(!m||m.value!='window'){return 86400;}var s=numberValue('startHour',0)*60+numberValue('startMinute',0);var e=numberValue('endHour',0)*60+numberValue('endMinute',0);s=((Math.floor(s)%1440)+1440)%1440;e=((Math.floor(e)%1440)+1440)%1440;if(s==e){return 86400;}if(e>s){return (e-s)*60;}return (1440-s+e)*60;}");
    write("function updateScheduleVisibility(){var m=byName('scheduleMode');var f=document.getElementById('scheduleFields');if(f&&m){f.style.display=(m.value=='window')?'block':'none';}}");
    write("function updateUsageEstimate(){var api=byName('apiMode');var client=api&&api.value=='client';var active=activeSecondsPerDay();var reserve=hiddenNumber('creditReserveRatio',0.90);var cost=hiddenNumber('requestCostCredits',1.0);var budget=client?hiddenNumber('clientDailyCreditBudget',4000):hiddenNumber('anonymousDailyCreditBudget',400);var minMs=client?hiddenNumber('clientMinIntervalMs',5000):hiddenNumber('anonymousMinIntervalMs',10000);var currentMode=document.getElementById('currentApiMode');currentMode=currentMode?currentMode.value:'';var sameMode=(client&&currentMode=='client')||(!client&&currentMode=='anonymous');if(sameMode){var currentBudget=hiddenNumber('currentDailyCreditBudget',0);var currentMin=hiddenNumber('currentMinUsefulIntervalMs',0);if(currentBudget>0){budget=currentBudget;}if(currentMin>minMs){minMs=currentMin;}}var usable=budget*reserve;var effective=usable/cost;var intervalMs=effective>0?(active/effective)*1000:minMs;if(intervalMs<minMs){intervalMs=minMs;}var requests=Math.floor((active*1000)/intervalMs);document.getElementById('usageApiMode').innerText=client?'OpenSkyClient':'Anonymous';document.getElementById('usageDailyBudget').innerText=String(Math.round(budget));document.getElementById('usageActiveHours').innerText=(active/3600).toFixed(1)+' h';document.getElementById('usageRefreshInterval').innerText=Math.round(intervalMs/1000)+'s';document.getElementById('usageDailyRequests').innerText=String(requests);updateScheduleVisibility();}");
    write("function bindUsageEstimate(){['scheduleMode','startHour','startMinute','endHour','endMinute','apiMode'].forEach(function(n){var e=byName(n);if(e){e.addEventListener('change',updateUsageEstimate);e.addEventListener('input',updateUsageEstimate);}});['creditReserveRatio','requestCostCredits','currentDailyCreditBudget','currentApiMode','anonymousDailyCreditBudget','clientDailyCreditBudget','anonymousMinIntervalMs','clientMinIntervalMs','currentMinUsefulIntervalMs'].forEach(function(id){var e=document.getElementById(id);if(e){e.addEventListener('change',updateUsageEstimate);e.addEventListener('input',updateUsageEstimate);}});}");
    write("function usePhoneLocation(){var s=document.getElementById('geoStatus');if(!navigator.geolocation){s.innerText='Geolocation not available';return;}navigator.geolocation.getCurrentPosition(function(p){document.querySelector('[name=centerLat]').value=p.coords.latitude.toFixed(6);document.querySelector('[name=centerLon]').value=p.coords.longitude.toFixed(6);s.innerText='Location filled';},function(){s.innerText='Location denied, enter manually';});}");
    write("function setBrowserTimezone(){var o=-new Date().getTimezoneOffset();var e=document.getElementById('timezoneOffsetMinutes');var found=false;for(var i=0;i<e.options.length;i++){if(e.options[i].value==String(o)){e.selectedIndex=i;found=true;}}if(!found){var opt=document.createElement('option');opt.value=String(o);opt.text='UTC'+(o>=0?'+':'')+(o/60);opt.selected=true;e.add(opt);}}");
    write("function toggleClientFields(v){document.getElementById('clientFields').style.display=(v=='client')?'block':'none';updateUsageEstimate();}");
    write("bindUsageEstimate();toggleClientFields(document.querySelector('[name=apiMode]').value);updateUsageEstimate();");
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

    write("<fieldset><legend>Location / Query Box</legend>");
    snprintf(value, sizeof(value), "%.6f", settings.location.centerLat);
    sendNumberInput("centerLat", "centerLat", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.centerLon);
    sendNumberInput("centerLon", "centerLon", value, "0.000001");
    snprintf(value, sizeof(value), "%.1f", settings.location.maxRangeKm);
    sendNumberInput("maxRangeKm", "maxRangeKm", value, "0.1");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLatMin);
    sendNumberInput("queryLatMin", "queryLatMin", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLonMin);
    sendNumberInput("queryLonMin", "queryLonMin", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLatMax);
    sendNumberInput("queryLatMax", "queryLatMax", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLonMax);
    sendNumberInput("queryLonMax", "queryLonMax", value, "0.000001");
    write("</fieldset>");

    write("<fieldset><legend>API / Refresh</legend>");
    write("<label>Account<select name=\"accountMode\">");
    sendSelectOption("0", "Anonymous", settings.api.accountMode == ApiAccountMode::Anonymous);
    sendSelectOption("1", "StandardUser", settings.api.accountMode == ApiAccountMode::StandardUser);
    sendSelectOption("2", "ActiveFeeder", settings.api.accountMode == ApiAccountMode::ActiveFeeder);
    sendSelectOption("3", "CustomBudget", settings.api.accountMode == ApiAccountMode::CustomBudget);
    sendSelectOption("4", "OpenSkyClient", settings.api.accountMode == ApiAccountMode::OpenSkyClient);
    write("</select></label><label>Refresh<select name=\"refreshPolicy\">");
    sendSelectOption("0", "AutoByDailyBudget", settings.api.refreshPolicy == RefreshPolicy::AutoByDailyBudget);
    sendSelectOption("1", "ManualInterval", settings.api.refreshPolicy == RefreshPolicy::ManualInterval);
    write("</select></label>");
    sendTextInput("OpenSky Client ID", "openSkyClientId", settings.api.openSkyClientId, false);
    sendTextInput("OpenSky Client Secret", "openSkyClientSecret", "", true);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.dailyCreditBudget));
    sendNumberInput("dailyCreditBudget", "dailyCreditBudget", value, "1");
    snprintf(value, sizeof(value), "%.2f", settings.api.creditReserveRatio);
    sendNumberInput("creditReserveRatio", "creditReserveRatio", value, "0.01");
    snprintf(value, sizeof(value), "%.1f", settings.api.requestCostCredits);
    sendNumberInput("requestCostCredits", "requestCostCredits", value, "0.1");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.manualRequestIntervalMs));
    sendNumberInput("manualRequestIntervalMs", "manualRequestIntervalMs", value, "1000");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.minUsefulIntervalMs));
    sendNumberInput("minUsefulIntervalMs", "minUsefulIntervalMs", value, "1000");
    write("</fieldset>");

    write("<fieldset><legend>Display / Filter</legend>");
    write("<label>UI Theme<select name=\"uiTheme\">");
    sendSelectOption("0", "ClassicRadar", settings.display.uiTheme == UiTheme::ClassicRadar);
    sendSelectOption("1", "ModernRadar", settings.display.uiTheme == UiTheme::ModernRadar);
    sendSelectOption("2", "CyberpunkRadar", settings.display.uiTheme == UiTheme::CyberpunkRadar);
    write("</select></label>");
    snprintf(value, sizeof(value), "%u", settings.display.maxAircraftToDisplay);
    sendNumberInput("maxAircraftToDisplay", "maxAircraftToDisplay", value, "1");
    sendCheckbox("showLabels", "showLabels", settings.display.showLabels);
    snprintf(value, sizeof(value), "%u", settings.display.brightness);
    sendNumberInput("brightness", "brightness", value, "1");
    sendCheckbox("showGroundTraffic", "showGroundTraffic", settings.filter.showGroundTraffic);
    snprintf(value, sizeof(value), "%.1f", settings.filter.minAirborneAltitudeM);
    sendNumberInput("minAirborneAltitudeM", "minAirborneAltitudeM", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.filter.minAirborneSpeedMs);
    sendNumberInput("minAirborneSpeedMs", "minAirborneSpeedMs", value, "0.1");
    write("</fieldset>");

    write("<fieldset><legend>Schedule</legend>");
    sendCheckbox("scheduleEnabled", "scheduleEnabled", settings.schedule.enabled);
    snprintf(value, sizeof(value), "%d", settings.schedule.startMinutesOfDay / 60);
    sendNumberInput("startHour", "startHour", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.startMinutesOfDay % 60);
    sendNumberInput("startMinute", "startMinute", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.endMinutesOfDay / 60);
    sendNumberInput("endHour", "endHour", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.endMinutesOfDay % 60);
    sendNumberInput("endMinute", "endMinute", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.timezoneOffsetMinutes);
    sendNumberInput("timezoneOffsetMinutes", "timezoneOffsetMinutes", value, "1");
    write("<label>idleDisplayMode<select name=\"idleDisplayMode\">");
    sendSelectOption("0", "PausedStatus", settings.schedule.idleDisplayMode == ScheduleIdleDisplayMode::PausedStatus);
    sendSelectOption("1", "Clock", settings.schedule.idleDisplayMode == ScheduleIdleDisplayMode::Clock);
    sendSelectOption("2", "DisplayOff", settings.schedule.idleDisplayMode == ScheduleIdleDisplayMode::DisplayOff);
    write("</select></label>");
    write("</fieldset>");

    write("<fieldset><legend>Prediction</legend>");
    sendCheckbox("predictionEnabled", "predictionEnabled", settings.prediction.enabled);
    snprintf(value, sizeof(value), "%.2f", settings.prediction.followAlpha);
    sendNumberInput("followAlpha", "followAlpha", value, "0.01");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.predictionMaxMs));
    sendNumberInput("predictionMaxMs", "predictionMaxMs", value, "1000");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.jumpResetDistanceKm);
    sendNumberInput("jumpResetDistanceKm", "jumpResetDistanceKm", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.lowSpeedThresholdMs);
    sendNumberInput("lowSpeedThresholdMs", "lowSpeedThresholdMs", value, "0.1");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.staleTimeoutMs));
    sendNumberInput("staleTimeoutMs", "staleTimeoutMs", value, "1000");
    sendCheckbox("correctionEnabled", "correctionEnabled", settings.prediction.correctionEnabled);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.correctionMinApiIntervalMs));
    sendNumberInput("correctionMinApiIntervalMs", "correctionMinApiIntervalMs", value, "1000");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.correctionDurationMs));
    sendNumberInput("correctionDurationMs", "correctionDurationMs", value, "1000");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.correctionStartDistanceKm);
    sendNumberInput("correctionStartDistanceKm", "correctionStartDistanceKm", value, "0.1");
    write("</fieldset>");

    write("<fieldset><legend>System</legend>");
    snprintf(value, sizeof(value), "%d", settings.system.uiButtonPin);
    sendNumberInput("uiButtonPin", "uiButtonPin", value, "1");
    sendCheckbox("serialDebug", "serialDebug", settings.system.serialDebug);
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
    else if (strcmp(nvsStateText(), "enabled") != 0)
    {
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
    settings.location.maxRangeKm = argToFloat("displayRangeKm", settings.location.maxRangeKm);
    updateQueryBoxFromCenterRange(settings);

    settings.api.provider = ApiProvider::OpenSky;
    const String apiMode = server_.hasArg("apiMode") ? server_.arg("apiMode") : "anonymous";
    settings.api.refreshPolicy = RefreshPolicy::AutoByDailyBudget;
    if (apiMode == "client")
    {
        settings.api.accountMode = ApiAccountMode::OpenSkyClient;
        settings.api.dailyCreditBudget = 4000;
        settings.api.minUsefulIntervalMs = 5000;
        copyArgToBuffer("openSkyClientId", settings.api.openSkyClientId, sizeof(settings.api.openSkyClientId), false);
        copyArgToBuffer("openSkyClientSecret", settings.api.openSkyClientSecret, sizeof(settings.api.openSkyClientSecret), true);
    }
    else
    {
        settings.api.accountMode = ApiAccountMode::Anonymous;
        settings.api.dailyCreditBudget = 400;
        settings.api.minUsefulIntervalMs = 10000;
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
    settings.location.queryLatMin = argToFloat("queryLatMin", settings.location.queryLatMin);
    settings.location.queryLonMin = argToFloat("queryLonMin", settings.location.queryLonMin);
    settings.location.queryLatMax = argToFloat("queryLatMax", settings.location.queryLatMax);
    settings.location.queryLonMax = argToFloat("queryLonMax", settings.location.queryLonMax);

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
    settings.display.brightness = static_cast<uint8_t>(argToInt("brightness", settings.display.brightness));
    settings.filter.showGroundTraffic = hasCheckedArg("showGroundTraffic");
    settings.filter.minAirborneAltitudeM = argToFloat("minAirborneAltitudeM", settings.filter.minAirborneAltitudeM);
    settings.filter.minAirborneSpeedMs = argToFloat("minAirborneSpeedMs", settings.filter.minAirborneSpeedMs);

    settings.schedule.enabled = hasCheckedArg("scheduleEnabled");
    settings.schedule.startMinutesOfDay = argToInt("startHour", settings.schedule.startMinutesOfDay / 60) * 60 +
                                          argToInt("startMinute", settings.schedule.startMinutesOfDay % 60);
    settings.schedule.endMinutesOfDay = argToInt("endHour", settings.schedule.endMinutesOfDay / 60) * 60 +
                                        argToInt("endMinute", settings.schedule.endMinutesOfDay % 60);
    settings.schedule.timezoneOffsetMinutes = argToInt("timezoneOffsetMinutes", settings.schedule.timezoneOffsetMinutes);
    settings.schedule.idleDisplayMode = static_cast<ScheduleIdleDisplayMode>(
        argToInt("idleDisplayMode", static_cast<int>(settings.schedule.idleDisplayMode)));

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
    page_.reserve(28000);
    write("<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    write("<title>");
    write(title);
    write("</title><style>body{font-family:sans-serif;margin:20px;max-width:760px}fieldset{margin:12px 0;padding:10px;border:1px solid #bbb}label{display:block;margin:8px 0}input,select{width:100%;box-sizing:border-box;padding:6px}button{padding:8px 14px}a{display:inline-block;margin:4px 8px 4px 0}.hint{font-size:.9em;color:#555}</style></head><body>");
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
