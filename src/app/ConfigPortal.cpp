#include "ConfigPortal.h"

#include <WiFi.h>
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
}

bool ConfigPortal::begin(UserSettings *settings, SettingsStore *settingsStore)
{
    settings_ = settings;
    settingsStore_ = settingsStore;
    restartRequested_ = false;

    const uint64_t mac = ESP.getEfuseMac();
    snprintf(apSsid_, sizeof(apSsid_), "AircraftRadar-%04X", static_cast<uint16_t>(mac & 0xFFFF));

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid_, kApPassword);
    snprintf(ipAddress_, sizeof(ipAddress_), "%s", WiFi.softAPIP().toString().c_str());

    dnsRunning_ = dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());

    server_.on("/", HTTP_GET, [this]()
    {
        handleRoot();
    });
    server_.on("/save", HTTP_POST, [this]()
    {
        handleSave();
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
    DebugLog::printf("ConfigPortal started. AP=%s IP=%s NVS=%s\r\n",
                     apSsid_,
                     ipAddress_,
                     nvsStateText());
    return true;
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
    WiFi.softAPdisconnect(true);
    running_ = false;
    DebugLog::println("ConfigPortal stopped.");
}

bool ConfigPortal::isRunning() const
{
    return running_;
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
    return ipAddress_;
}

void ConfigPortal::handleRoot()
{
    updateLanguageFromRequest();
    renderSettingsPage();
}

void ConfigPortal::handleSave()
{
    updateLanguageFromRequest();
    if (settings_ == nullptr || settingsStore_ == nullptr)
    {
        renderSavedPage(false);
        return;
    }

    applyFormToSettings();
    sanitizeUserSettings(*settings_);
    const bool saved = settingsStore_->save(*settings_);
    DebugLog::printf("ConfigPortal save complete. saved=%u NVS=%s\r\n",
                     saved ? 1 : 0,
                     nvsStateText());
    renderSavedPage(saved);
}

void ConfigPortal::handleStatus()
{
    char body[192];
    snprintf(body,
             sizeof(body),
             "{\"mode\":\"setup\",\"ap\":\"%s\",\"ip\":\"%s\",\"nvs\":\"%s\",\"lang\":\"%s\",\"restart\":%u}",
             apSsid_,
             ipAddress_,
             nvsStateText(),
             languageCode(),
             restartRequested_ ? 1 : 0);
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

void ConfigPortal::renderSettingsPage()
{
    if (settings_ == nullptr)
    {
        server_.send(500, "text/plain", "settings not available");
        return;
    }

    char value[32];
    UserSettings &settings = *settings_;

    sendPageHeader(text("Aircraft Radar Setup", "航班雷达设置"));
    write("<p style=\"text-align:right\"><a href=\"/?lang=");
    write(toggleLanguageCode());
    write("\">");
    write(toggleLanguageLabel());
    write("</a></p>");
    write("<h1>");
    write(text("Aircraft Radar Setup", "航班雷达设置"));
    write("</h1>");
    write("<p>AP: ");
    write(apSsid_);
    write(" / IP: ");
    write(ipAddress_);
    write("</p><form method=\"POST\" action=\"/save\"><input type=\"hidden\" name=\"lang\" value=\"");
    write(languageCode());
    write("\">");

    write("<fieldset><legend>WiFi</legend>");
    sendCheckbox(text("Configured", "已配置"), "wifi_configured", settings.wifi.configured);
    sendTextInput("SSID", "wifi_ssid", settings.wifi.ssid, false);
    sendTextInput(text("Password", "密码"), "wifi_password", "", true);
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Location", "位置"));
    write("</legend>");
    snprintf(value, sizeof(value), "%.6f", settings.location.centerLat);
    sendNumberInput(text("Center Lat", "中心纬度"), "centerLat", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.centerLon);
    sendNumberInput(text("Center Lon", "中心经度"), "centerLon", value, "0.000001");
    snprintf(value, sizeof(value), "%.1f", settings.location.maxRangeKm);
    sendNumberInput(text("Max Range Km", "最大范围 km"), "maxRangeKm", value, "0.1");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLatMin);
    sendNumberInput(text("Query Lat Min", "查询最小纬度"), "queryLatMin", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLonMin);
    sendNumberInput(text("Query Lon Min", "查询最小经度"), "queryLonMin", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLatMax);
    sendNumberInput(text("Query Lat Max", "查询最大纬度"), "queryLatMax", value, "0.000001");
    snprintf(value, sizeof(value), "%.6f", settings.location.queryLonMax);
    sendNumberInput(text("Query Lon Max", "查询最大经度"), "queryLonMax", value, "0.000001");
    write("</fieldset>");

    write("<fieldset><legend>API</legend>");
    write("<label>");
    write(text("Provider", "数据源"));
    write("<select name=\"provider\">");
    sendSelectOption("0", "OpenSky", settings.api.provider == ApiProvider::OpenSky);
    sendSelectOption("1", "adsb.fi", settings.api.provider == ApiProvider::AdsbFi);
    sendSelectOption("2", "Airplanes.live", settings.api.provider == ApiProvider::AirplanesLive);
    sendSelectOption("3", "adsb.lol", settings.api.provider == ApiProvider::AdsbLol);
    sendSelectOption("4", "Custom", settings.api.provider == ApiProvider::Custom);
    write("</select></label>");
    write("<label>");
    write(text("Account", "账号类型"));
    write("<select name=\"accountMode\">");
    sendSelectOption("0", text("Anonymous", "匿名"), settings.api.accountMode == ApiAccountMode::Anonymous);
    sendSelectOption("1", text("Standard", "标准用户"), settings.api.accountMode == ApiAccountMode::StandardUser);
    sendSelectOption("2", text("Feeder", "馈送用户"), settings.api.accountMode == ApiAccountMode::ActiveFeeder);
    sendSelectOption("3", text("Custom Budget", "自定义额度"), settings.api.accountMode == ApiAccountMode::CustomBudget);
    write("</select></label>");
    write("<label>");
    write(text("Refresh", "刷新策略"));
    write("<select name=\"refreshPolicy\">");
    sendSelectOption("0", text("Auto Budget", "按额度自动"), settings.api.refreshPolicy == RefreshPolicy::AutoByDailyBudget);
    sendSelectOption("1", text("Manual", "手动间隔"), settings.api.refreshPolicy == RefreshPolicy::ManualInterval);
    write("</select></label>");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.dailyCreditBudget));
    sendNumberInput(text("Daily Credit Budget", "每日额度"), "dailyCreditBudget", value, "1");
    snprintf(value, sizeof(value), "%.2f", settings.api.creditReserveRatio);
    sendNumberInput(text("Credit Reserve Ratio", "额度保留比例"), "creditReserveRatio", value, "0.01");
    snprintf(value, sizeof(value), "%.1f", settings.api.requestCostCredits);
    sendNumberInput(text("Request Cost", "单次请求成本"), "requestCostCredits", value, "0.1");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.manualRequestIntervalMs));
    sendNumberInput(text("Manual Interval Ms", "手动间隔 ms"), "manualRequestIntervalMs", value, "1000");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.minUsefulIntervalMs));
    sendNumberInput(text("Min Useful Interval Ms", "最小有效间隔 ms"), "minUsefulIntervalMs", value, "1000");
    sendTextInput(text("OpenSky Username", "OpenSky 用户名"), "openSkyUsername", settings.api.openSkyUsername, false);
    sendTextInput(text("OpenSky Password", "OpenSky 密码"), "openSkyPassword", "", true);
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Schedule", "运行时间"));
    write("</legend>");
    sendCheckbox(text("Schedule Enabled", "启用时间表"), "scheduleEnabled", settings.schedule.enabled);
    snprintf(value, sizeof(value), "%d", settings.schedule.startMinutesOfDay / 60);
    sendNumberInput(text("Start Hour", "开始小时"), "startHour", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.startMinutesOfDay % 60);
    sendNumberInput(text("Start Minute", "开始分钟"), "startMinute", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.endMinutesOfDay / 60);
    sendNumberInput(text("End Hour", "结束小时"), "endHour", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.endMinutesOfDay % 60);
    sendNumberInput(text("End Minute", "结束分钟"), "endMinute", value, "1");
    snprintf(value, sizeof(value), "%d", settings.schedule.timezoneOffsetMinutes);
    sendNumberInput(text("Timezone Offset Minutes", "时区偏移分钟"), "timezoneOffsetMinutes", value, "1");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Display", "显示"));
    write("</legend>");
    write("<label>");
    write(text("UI Theme", "界面风格"));
    write("<select name=\"uiTheme\">");
    sendSelectOption("0", text("Classic", "经典雷达"), settings.display.uiTheme == UiTheme::ClassicRadar);
    sendSelectOption("1", text("Modern", "现代"), settings.display.uiTheme == UiTheme::ModernRadar);
    sendSelectOption("2", text("Cyberpunk", "赛博"), settings.display.uiTheme == UiTheme::CyberpunkRadar);
    write("</select></label>");
    snprintf(value, sizeof(value), "%u", settings.display.maxAircraftToDisplay);
    sendNumberInput(text("Max Aircraft", "最大飞机数"), "maxAircraftToDisplay", value, "1");
    sendCheckbox(text("Show Labels", "显示标签"), "showLabels", settings.display.showLabels);
    snprintf(value, sizeof(value), "%u", settings.display.brightness);
    sendNumberInput(text("Brightness", "亮度"), "brightness", value, "1");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Filter", "过滤"));
    write("</legend>");
    sendCheckbox(text("Show Ground Traffic", "显示地面目标"), "showGroundTraffic", settings.filter.showGroundTraffic);
    snprintf(value, sizeof(value), "%.1f", settings.filter.minAirborneAltitudeM);
    sendNumberInput(text("Min Airborne Altitude M", "最低空中高度 m"), "minAirborneAltitudeM", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.filter.minAirborneSpeedMs);
    sendNumberInput(text("Min Airborne Speed m/s", "最低空中速度 m/s"), "minAirborneSpeedMs", value, "0.1");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("Prediction", "预测"));
    write("</legend>");
    sendCheckbox(text("Prediction Enabled", "启用预测"), "predictionEnabled", settings.prediction.enabled);
    snprintf(value, sizeof(value), "%.2f", settings.prediction.followAlpha);
    sendNumberInput(text("Follow Alpha", "跟随系数"), "followAlpha", value, "0.01");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.predictionMaxMs));
    sendNumberInput(text("Prediction Max Ms", "最大预测时间 ms"), "predictionMaxMs", value, "1000");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.jumpResetDistanceKm);
    sendNumberInput(text("Jump Reset Distance Km", "跳变重置距离 km"), "jumpResetDistanceKm", value, "0.1");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.lowSpeedThresholdMs);
    sendNumberInput(text("Low Speed Threshold m/s", "低速阈值 m/s"), "lowSpeedThresholdMs", value, "0.1");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.staleTimeoutMs));
    sendNumberInput(text("Stale Timeout Ms", "过期时间 ms"), "staleTimeoutMs", value, "1000");
    sendCheckbox(text("Correction Enabled", "启用平滑校正"), "correctionEnabled", settings.prediction.correctionEnabled);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.correctionMinApiIntervalMs));
    sendNumberInput(text("Correction Min API Interval Ms", "校正最小 API 间隔 ms"), "correctionMinApiIntervalMs", value, "1000");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.prediction.correctionDurationMs));
    sendNumberInput(text("Correction Duration Ms", "校正持续时间 ms"), "correctionDurationMs", value, "1000");
    snprintf(value, sizeof(value), "%.1f", settings.prediction.correctionStartDistanceKm);
    sendNumberInput(text("Correction Start Distance Km", "校正启动距离 km"), "correctionStartDistanceKm", value, "0.1");
    write("</fieldset>");

    write("<fieldset><legend>");
    write(text("System", "系统"));
    write("</legend>");
    snprintf(value, sizeof(value), "%d", settings.system.uiButtonPin);
    sendNumberInput(text("UI Button Pin", "界面按键引脚"), "uiButtonPin", value, "1");
    sendCheckbox(text("Serial Debug", "串口调试"), "serialDebug", settings.system.serialDebug);
    write("</fieldset>");

    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(settings.api.computedRequestIntervalMs));
    write("<p>computedRequestIntervalMs: ");
    write(value);
    write("</p><p>NVS: ");
    write(nvsStateText());
    write("</p><p><button type=\"submit\">");
    write(text("Save Settings", "保存设置"));
    write("</button> ");
    write("<a href=\"/restart?lang=");
    write(languageCode());
    write("\">");
    write(text("Restart device", "重启设备"));
    write("</a></p></form>");
    sendPageFooter();
}

void ConfigPortal::renderSavedPage(bool saved)
{
    sendPageHeader(saved ? text("Saved", "已保存") : text("Save failed", "保存失败"));
    write(saved ? text("<h1>Saved</h1>", "<h1>已保存</h1>") : text("<h1>Save failed</h1>", "<h1>保存失败</h1>"));
    write("<p>NVS: ");
    write(nvsStateText());
    write("</p><p><a href=\"/?lang=");
    write(languageCode());
    write("\">");
    write(text("Back", "返回"));
    write("</a> <a href=\"/restart?lang=");
    write(languageCode());
    write("\">");
    write(text("Restart device", "重启设备"));
    write("</a></p>");
    sendPageFooter();
}

void ConfigPortal::applyFormToSettings()
{
    UserSettings &settings = *settings_;
    settings.wifi.configured = hasCheckedArg("wifi_configured");
    copyArgToBuffer("wifi_ssid", settings.wifi.ssid, sizeof(settings.wifi.ssid), false);
    copyArgToBuffer("wifi_password", settings.wifi.password, sizeof(settings.wifi.password), true);

    settings.location.centerLat = argToFloat("centerLat", settings.location.centerLat);
    settings.location.centerLon = argToFloat("centerLon", settings.location.centerLon);
    settings.location.maxRangeKm = argToFloat("maxRangeKm", settings.location.maxRangeKm);
    settings.location.queryLatMin = argToFloat("queryLatMin", settings.location.queryLatMin);
    settings.location.queryLonMin = argToFloat("queryLonMin", settings.location.queryLonMin);
    settings.location.queryLatMax = argToFloat("queryLatMax", settings.location.queryLatMax);
    settings.location.queryLonMax = argToFloat("queryLonMax", settings.location.queryLonMax);

    settings.api.provider = static_cast<ApiProvider>(argToInt("provider", static_cast<int>(settings.api.provider)));
    settings.api.accountMode = static_cast<ApiAccountMode>(argToInt("accountMode", static_cast<int>(settings.api.accountMode)));
    settings.api.refreshPolicy = static_cast<RefreshPolicy>(argToInt("refreshPolicy", static_cast<int>(settings.api.refreshPolicy)));
    settings.api.dailyCreditBudget = argToUInt("dailyCreditBudget", settings.api.dailyCreditBudget);
    settings.api.creditReserveRatio = argToFloat("creditReserveRatio", settings.api.creditReserveRatio);
    settings.api.requestCostCredits = argToFloat("requestCostCredits", settings.api.requestCostCredits);
    settings.api.manualRequestIntervalMs = argToUInt("manualRequestIntervalMs", settings.api.manualRequestIntervalMs);
    settings.api.minUsefulIntervalMs = argToUInt("minUsefulIntervalMs", settings.api.minUsefulIntervalMs);
    copyArgToBuffer("openSkyUsername", settings.api.openSkyUsername, sizeof(settings.api.openSkyUsername), false);
    copyArgToBuffer("openSkyPassword", settings.api.openSkyPassword, sizeof(settings.api.openSkyPassword), true);

    settings.schedule.enabled = hasCheckedArg("scheduleEnabled");
    settings.schedule.startMinutesOfDay = argToInt("startHour", settings.schedule.startMinutesOfDay / 60) * 60 +
                                          argToInt("startMinute", settings.schedule.startMinutesOfDay % 60);
    settings.schedule.endMinutesOfDay = argToInt("endHour", settings.schedule.endMinutesOfDay / 60) * 60 +
                                        argToInt("endMinute", settings.schedule.endMinutesOfDay % 60);
    settings.schedule.timezoneOffsetMinutes = argToInt("timezoneOffsetMinutes", settings.schedule.timezoneOffsetMinutes);

    settings.display.uiTheme = static_cast<UiTheme>(argToInt("uiTheme", static_cast<int>(settings.display.uiTheme)));
    settings.display.maxAircraftToDisplay = static_cast<uint8_t>(argToInt("maxAircraftToDisplay", settings.display.maxAircraftToDisplay));
    settings.display.showLabels = hasCheckedArg("showLabels");
    settings.display.brightness = static_cast<uint8_t>(argToInt("brightness", settings.display.brightness));

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
    if (lang == "zh")
    {
        pageLanguage_ = PageLanguage::Chinese;
    }
    else
    {
        pageLanguage_ = PageLanguage::English;
    }
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
    page_.reserve(18000);
    write("<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    write("<title>");
    write(title);
    write("</title><style>body{font-family:sans-serif;margin:20px;max-width:760px}fieldset{margin:12px 0;padding:10px}label{display:block;margin:8px 0}input,select{width:100%;box-sizing:border-box;padding:6px}button{padding:8px 14px}</style></head><body>");
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
