#include <Arduino.h>
#include <Preferences.h>
#include "AppSettings.h"


// Constructor
AppSettings::AppSettings() {
    if (settingsEmpty()) {
        saveSettings();
    }
    else {
        loadSettings();
    }

}

void AppSettings::saveSettings() {
    preferences.begin("app_settings", false);

    preferences.putBool("power_on", power_on);
    preferences.putBool("timer_mode", timer_mode);
    preferences.putBool("wifi_enabled", enable_wifi);
    preferences.putInt("time_on_hour", time_on_hour);
    preferences.putInt("time_on_minute", time_on_min);
    preferences.putInt("time_off_hour", time_off_hour);
    preferences.putInt("time_off_minute", time_off_min);
    preferences.putInt("logging_int", logging_interval);
    preferences.putInt("outbound_int", outbound_interval);
    preferences.putInt("inbound_int", inbound_interval);

    preferences.end();

    Serial.println("Settings written to flash");
}

void AppSettings::loadSettings() {
    preferences.begin("app_settings", false);

    power_on = preferences.getBool("power_on");
    timer_mode = preferences.getBool("timer_mode");
    enable_wifi = preferences.getBool("wifi_enabled");
    time_on_hour = preferences.getInt("time_on_hour");
    time_on_min = preferences.getInt("time_on_minute");
    time_off_hour = preferences.getInt("time_off_hour");
    time_off_min = preferences.getInt("time_off_minute");
    logging_interval = preferences.getInt("logging_int");
    outbound_interval = preferences.getInt("outbound_int");
    inbound_interval = preferences.getInt("inbound_int");

    preferences.end();

    Serial.println("Settings read from flash");
}

bool AppSettings::settingsEmpty() {
    preferences.begin("app_settings", false);

    if (preferences.isKey("power_on")) {
        return false;
    }
    else {
        return true;
    }
}

void AppSettings::printCurrentSettings() {
    // build firmware versioning strings
    char firmware_version[10];
    char firmware_date[16];
    sprintf(firmware_version, "%01d.%01d.%01d", firmware_version_prim, firmware_version_sec, firmware_version_tert);
    sprintf(firmware_date, "rev.%02d/%02d/%02d", firmware_updated_d, firmware_updated_m, firmware_updated_y);

    // Read the actual timer settings for the note, don't assume
    char timer_on_string[10];
    char timer_off_string[10];
    sprintf(timer_on_string, "%02d:%02d", time_on_hour, time_on_min);
    sprintf(timer_off_string, "%02d:%02d", time_off_hour, time_off_min);

    Serial.print("Firmware version: ");
    Serial.println(firmware_version);
    Serial.print("Firmware date: ");
    Serial.println(firmware_date);
    Serial.print("Power State: ");
    Serial.println(power_on);
    Serial.print("Timer Mode: ");
    Serial.println(timer_mode);
    Serial.print("Time on: ");
    Serial.print(time_on_hour);
    Serial.print(":");
    Serial.println(time_on_min);
    Serial.print("Time off: ");
    Serial.print(time_off_hour);
    Serial.print(":");
    Serial.println(time_off_min);
    Serial.print("Timer on time: ");
    Serial.println(timer_on_string);
    Serial.print("Timer off time: ");
    Serial.println(timer_off_string);
    Serial.print("Logging interval: ");
    Serial.println(logging_interval);
    Serial.print("Inbound interval: ");
    Serial.println(inbound_interval);
    Serial.print("Outbound interval: ");
    Serial.println(outbound_interval);
}

//Getters
int AppSettings::loggingInterval() const {
    return logging_interval;
}

int AppSettings::outboundInterval() const {
    return outbound_interval;
}

int AppSettings::inboundInterval() const {
    return inbound_interval;
}

bool AppSettings::powerOn() const {
    return power_on;
}

bool AppSettings::timerMode() const {
    return timer_mode;
}

int AppSettings::timeOnHour() const {
    return time_on_hour;
}

int AppSettings::timeOnMin() const {
    return time_on_min;
}

int AppSettings::timeOffHour() const {
    return time_off_hour;
}

int AppSettings::timeOffMin() const {
    return time_off_min;
}

int AppSettings::timeResetHour() const {
    return time_reset_hour;
}

int AppSettings::timeResetMinute() const {
    return time_reset_minute;
}

bool AppSettings::enableRenogy() const {
    return enable_renogy;
}

bool AppSettings::enableSTTS22H() const {
    return enable_STTS22H;
}

bool AppSettings::enableSen5x() const {
    return enable_sen5x;
}

bool AppSettings::enableWifi() const {
    return enable_wifi;
}

bool AppSettings::enableBms() const {
    return enable_bms;
}

const char* AppSettings::firmwareVersionString() const {
    static char buffer[10];
    sprintf(buffer, "%01d.%01d.%01d", firmware_version_prim, firmware_version_sec, firmware_version_tert);
    return buffer;
}

const char* AppSettings::firmwareDateString() const {
    static char buffer[16];
    sprintf(buffer, "rev.%02d/%02d/%02d", firmware_updated_d, firmware_updated_m, firmware_updated_y);
    return buffer;
}

const char* AppSettings::timeOnString() const {
    static char buffer[6];
    sprintf(buffer, "%02d:%02d", time_on_hour, time_on_min);
    return buffer;
}

const char* AppSettings::timeOffString() const {
    static char buffer[6];
    sprintf(buffer, "%02d:%02d", time_off_hour, time_off_min);
    return buffer;
}



//Setters
void AppSettings::setLoggingInterval(int interval) {
    logging_interval = interval;
    saveSettings();
}

void AppSettings::setOutboundInterval(int interval) {
    outbound_interval = interval;
    saveSettings();
}

void AppSettings::setInboundInterval(int interval) {
    inbound_interval = interval;
    saveSettings();
}

void AppSettings::setPowerOn(bool power) {
    power_on = power;
    saveSettings();
}

void AppSettings::setTimerMode(bool mode) {
    timer_mode = mode;
    saveSettings();
}

void AppSettings::setTimeOn(int hour, int min) {
    time_on_hour = hour;
    time_on_min = min;
    saveSettings();
}

void AppSettings::setTimeOff(int hour, int min) {
    time_off_hour = hour;
    time_off_min = min;
    saveSettings();
}

void AppSettings::setTimeReset(int hour, int minute) {
    time_reset_hour = hour;
    time_reset_minute = minute;
    saveSettings();
}

void AppSettings::setEnableRenogy(bool enable) {
    enable_renogy = enable;
    saveSettings();
}

void AppSettings::setEnableSTTS22H(bool enable) {
    enable_STTS22H = enable;
    saveSettings();
}

void AppSettings::setEnableSen5x(bool enable) {
    enable_sen5x = enable;
    saveSettings();
}

void AppSettings::setEnableWifi(bool enable) {
    enable_wifi = enable;
    saveSettings();
}

void AppSettings::setEnableBms(bool enable) {
    enable_bms = enable;
    saveSettings();
}
