/**
 * @file AppSettings.h
 * @author Christopher Lee (clee@unitedconsulting.com)
 * @brief Handles management of settings for UnitedOSM
 * @version 0.1
 * @date 2024-02-28
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <Arduino.h>
#include <Notecard.h>
#include <Preferences.h>

#ifndef AppSettings_h
#define Appsettings_h

class AppSettings {
private:
    // Firmware_data
    int firmware_version_prim = 0;
    int firmware_version_sec = 5;
    int firmware_version_tert = 5;
    int firmware_updated_d = 31;
    int firmware_updated_m = 1;
    int firmware_updated_y = 2024;

    // Changeable Settings
    int logging_interval = 1; // in minutes
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
    bool enable_sen5x = true;
    bool enable_wifi = false;
public:
    AppSettings();

    /**
     * @brief Set the Firmware Version object
     * 
     * @param prim 
     * @param sec 
     * @param tert 
     */
    void setFirmwareVersion(int prim, int sec, int tert);
};


#endif