/**************************************************************

This file is a part of
https://github.com/atiderko/espwebconfig

Copyright [2020] Alexander Tiderko

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

**************************************************************/

#include "ewcTime.h"
#include "ewcConfigServer.h"
#include <ArduinoJSON.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include "generated/timeSetupHTML.h"

using namespace EWC;

Time::Time() : ConfigInterface("time")
{
    _ntpAvailable = false;
    _manuallOffset = 0;
}

Time::~Time()
{
}

void Time::setup(JsonDocument& config, bool resetConfig)
{
    settimeofday_cb(std::bind(&Time::_callbackTimeSet, this));
    I::get().logger() << F("[EWC Time] setup") << endl;
    _initParams();
    _fromJson(config);
    EWC::I::get().server().insertMenuG("Time", "/time/setup", "menu_time", FPSTR(PROGMEM_CONFIG_TEXT_HTML), HTML_TIME_SETUP_GZIP, sizeof(HTML_TIME_SETUP_GZIP), true, 0);
    EWC::I::get().server().webserver().on("/time/config.json", std::bind(&Time::_onTimeConfig, this, &EWC::I::get().server().webserver()));
    EWC::I::get().server().webserver().on("/time/config/save", std::bind(&Time::_onTimeSave, this, &EWC::I::get().server().webserver()));
    I::get().logger() << F("[EWC Time] current time: ") << str() << endl;
    if (!_paramManually) {
        // Sync our clock to NTP
        I::get().logger() << F("[EWC Time] sync to ntp server...") << endl;
        configTime(TZMAP[_paramTimezone-1][1] * 3600, TZMAP[_paramTimezone-1][0] * 3600, "0.europe.pool.ntp.org", "pool.ntp.org", "time.nist.gov");
    }
}

void Time::fillJson(JsonDocument& config)
{
    config["time"]["timezone"] = _paramTimezone;
    config["time"]["manually"] = _paramManually;
    config["time"]["mdate"] = _paramDate;
    config["time"]["mtime"] = _paramTime;
    config["time"]["dnd_enabled"] = _paramDndEnabled;
    config["time"]["dnd_from"] = _paramDndFrom;
    config["time"]["dnd_to"] = _paramDndTo;
    config["time"]["current"] = str();
}

void Time::_initParams()
{
    _paramTimezone = 31;
    _paramManually = false;
    _paramDate = "21.10.2020";
    _paramTime = "21:00";
    _paramDndEnabled = false;
    _paramDndFrom = "22:00";
    _paramDndTo = "06:00";
}

void Time::_fromJson(JsonDocument& config)
{
    JsonVariant jv = config["time"]["timezone"];
    if (!jv.isNull()) {
        int tz = jv.as<int>();
        if (tz > 0 && tz <= 82) {
            if (_paramTimezone != tz) {
                _paramTimezone = tz;
            }
        }
    }
    jv = config["time"]["mdate"];
    if (!jv.isNull()) {
        _paramDate = jv.as<String>();
    }
    jv = config["time"]["mtime"];
    if (!jv.isNull()) {
        _paramTime = jv.as<String>();
    }
    jv = config["time"]["manually"];
    if (!jv.isNull()) {
        _paramManually = jv.as<bool>();
        // set time
        setLocalTime(_paramDate, _paramTime);
    }
    jv = config["time"]["dnd_enabled"];
    if (!jv.isNull()) {
        _paramDndEnabled = jv.as<bool>();
    }
    jv = config["time"]["dnd_from"];
    if (!jv.isNull()) {
        _paramDndFrom = jv.as<String>();
    }
    jv = config["time"]["dnd_to"];
    if (!jv.isNull()) {
        _paramDndTo = jv.as<String>();
    }
    if (!_paramManually) {
        // Sync our clock to NTP
        I::get().logger() << "[EWC Time] sync to ntp server..." << endl;
        configTime(TZMAP[_paramTimezone-1][1] * 3600, TZMAP[_paramTimezone-1][0] * 3600, "0.europe.pool.ntp.org", "pool.ntp.org", "time.nist.gov");
    }
}

void Time::_callbackTimeSet(void)
{
    I::get().logger() << "[EWC Time] ntp time set to " << str() << endl;
    _ntpAvailable = true;
}

void Time::_onTimeConfig(WebServer* request)
{
    if (!I::get().server().isAuthenticated(request)) {
        return request->requestAuthentication();
    }
    DynamicJsonDocument jsonDoc(1024);
    fillJson(jsonDoc);
    String output;
    serializeJson(jsonDoc, output);
    request->send(200, FPSTR(PROGMEM_CONFIG_APPLICATION_JSON), output);
}

void Time::_onTimeSave(WebServer* request)
{
    if (!I::get().server().isAuthenticated(request)) {
        return request->requestAuthentication();
    }
    DynamicJsonDocument config(1024);
    if (request->hasArg("timezone") && !request->arg("timezone").isEmpty()) {
        config["time"]["timezone"] = request->arg("timezone").toInt();
    }

    if (request->hasArg("manually")) {
        bool manually = request->arg("manually").equals("true");
        config["time"]["manually"] = manually;
        if (manually) {
            if (request->hasArg("mdate")) {
                config["time"]["mdate"] = request->arg("mdate");
            }
            if (request->hasArg("mtime")) {
                config["time"]["mtime"] = request->arg("mtime");
            }
        }
    }
    if (request->hasArg("dnd_enabled")) {
        bool dnd = request->arg("dnd_enabled").equals("true");
        config["time"]["dnd_enabled"] = dnd;
        if (dnd) {
            if (request->hasArg("dnd_from")) {
                config["time"]["dnd_from"] = request->arg("dnd_from");
            }
            if (request->hasArg("dnd_to")) {
                config["time"]["dnd_to"]  = request->arg("dnd_to");
            }
        }
    }
    _fromJson(config);
    I::get().configFS().save();
    String details;
    serializeJsonPretty(config["time"], details);
    I::get().server().sendPageSuccess(request, "EWC Time save", "Save successful!", "/time/setup", "<pre id=\"json\">" + details + "</pre>");
}

void Time::setLocalTime(String& date, String& time) {
    struct tm ti;
    sscanf(date.c_str(), "%d.%d.%d", &ti.tm_mday, &ti.tm_mon, &ti.tm_year);
    sscanf(time.c_str(), "%d:%d", &ti.tm_hour, &ti.tm_min);
    time_t t = mktime(&ti);
    _manuallOffset = t - millis() / 1000;
}

time_t Time::currentTime()
{
    if (_paramManually) {
        return millis() / 1000 - _manuallOffset;
    }
    time_t rawtime;
    time(&rawtime);
    rawtime += TZMAP[_paramTimezone-1][1] * 3600 + TZMAP[_paramTimezone-1][0] * 3600;
    return rawtime;
}

String Time::str(time_t offsetSeconds)
{
    time_t rawtime = currentTime();
    rawtime += offsetSeconds;
    char buffer [80];
    strftime(buffer, 80, "%FT%T", gmtime(&rawtime));
    return String(buffer);
}

bool Time::dndEnabled()
{
    return _paramDndEnabled;
}

bool Time::isDisturb(time_t offsetSeconds)
{
    if (_paramDndEnabled) {
        time_t r = shiftDisturb(offsetSeconds);
        return r > offsetSeconds;
    }
    return false;
}

time_t Time::shiftDisturb(time_t offsetSeconds)
{
    if (_paramDndEnabled) {
        time_t m24 =  24 * 60;
        time_t ctime = currentTime();
        struct tm * timeinfo;
        timeinfo = gmtime(&ctime);
        time_t cmin = timeinfo->tm_hour * 60 + timeinfo->tm_min;
        time_t minFrom = _dndToMin(_paramDndFrom);
        time_t minTo = _dndToMin(_paramDndTo);
        cmin += offsetSeconds / 60;
        if (cmin > m24) {
            cmin -= m24;
        }
        time_t result = 0;
        if (minFrom > minTo) {
            // overflow on 24:00
            if (cmin >= minFrom) {
                result = m24 - cmin + minTo;
            } else if (cmin <= minTo) {
                result = minTo - cmin;
            }
        } else {
            result = minTo - cmin;
        }
        return result * 60 + offsetSeconds;  // back to seconds
    }
    return offsetSeconds;
}

const String& Time::dndTo()
{
    return _paramDndTo;
}

time_t Time::_dndToMin(String& hmTime)
{
    int hours, minutes;
    sscanf(hmTime.c_str(), "%d:%d", &hours, &minutes);
    return hours * 60 + minutes;
}
