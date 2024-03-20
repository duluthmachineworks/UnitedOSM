
#ifndef AppSettings_h
#define AppSettings_h

#include <Arduino.h>
#include <Preferences.h>

/**
 * @class AppSettings
 * @brief Represents the settings for the application.
 *
 * This class provides methods to save and load settings from Preferences.
 * It also contains member variables to store firmware version and various changeable settings.
 */
class AppSettings {
public:

    AppSettings();

    void saveSettings();
    void loadSettings();
    bool settingsEmpty();

    void printCurrentSettings();

    //Getters
    int loggingInterval() const;
    int outboundInterval() const;
    int inboundInterval() const;
    bool powerOn() const;
    bool timerMode() const;
    int timeOnHour() const;
    int timeOnMin() const;
    int timeOffHour() const;
    int timeOffMin() const;
    int timeResetHour() const;
    int timeResetMinute() const;
    bool enableRenogy() const;
    bool enableSTTS22H() const;
    bool enableSen5x() const;
    bool enableWifi() const;
    bool enableBms() const;

    const char* firmwareVersionString() const;
    const char* firmwareDateString() const;
    const char* timeOnString() const;
    const char* timeOffString() const;

    //Setters
    void setLoggingInterval(int interval);
    void setOutboundInterval(int interval);
    void setInboundInterval(int interval);
    void setPowerOn(bool power);
    void setTimerMode(bool mode);
    void setTimeOn(int hour, int min);
    void setTimeOff(int hour, int min);
    void setTimeReset(int hour, int minute);
    void setEnableRenogy(bool enable);
    void setEnableSTTS22H(bool enable);
    void setEnableSen5x(bool enable);
    void setEnableWifi(bool enable);
    void setEnableBms(bool enable);


private:
    Preferences preferences;
    // Add private member variables as needed
    // Firmware_data
    int firmware_version_prim = 0;
    int firmware_version_sec = 5;
    int firmware_version_tert = 5;
    int firmware_updated_d = 31;
    int firmware_updated_m = 1;
    int firmware_updated_y = 2024;

    // Changeable Settings
    int logging_interval = 1; // in minutes
    int status_interval = 10;
    int outbound_interval = 1;
    int inbound_interval = 1;
    bool power_on = true;
    bool timer_mode = true;
    int time_on_hour = 7;
    int time_on_min = 30;
    int time_off_hour = 18;
    int time_off_min = 0;
    int time_reset_hour = 1;
    int time_reset_minute = 0;
    bool enable_renogy = false;
    bool enable_STTS22H = false;
    bool enable_sen5x = false;
    bool enable_wifi = false;
    bool enable_bms = false;

};

#endif // APP_SETTINGS_H