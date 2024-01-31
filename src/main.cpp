/* SPDX-License-Identifier: GPL-3.0-only or GPL-3.0-or-later */
/*
 * Copyright (C) 2024 Christopher E. Lee clee@unitedconsulting.com
 * 
 * This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <Notecard.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include <string>
#include <Preferences.h>

#include "RenogyRover.h"
#include "SparkFun_STTS22H.h"
#include "TimeLib.h"
#include "TimeAlarms.h"

// IO definitions
#define LED_PIN 13;
#define RDX2 16
#define TXD2 17

// Hardware watchdog timeout
#define WDT_TIMEOUT 30 // in seconds

//Init flash storage
Preferences preferences;

// Notecard
#define PRODUCT_UID "com.unitedconsulting.clee:solarmonitor"
#define SEND_INTERVAL 15000
bool send_time_updates = false;
Notecard notecard;
float notecard_temp;

// Firmware_data
int firmware_version_prim = 0;
int firmware_version_sec = 5;
int firmware_version_tert = 4;
int firmware_updated_d = 31;
int firmware_updated_m = 1;
int firmware_updated_y = 2024;

// Changeable Settings
int logging_interval = 5; // in minutes
int outbound_interval = 5;
int inbound_interval = 5;
bool power_on = true;
bool timer_mode = true;
bool wifi_enabled = false;      // default state is off
int time_on_hour = 7;
int time_on_min = 30;
int time_off_hour = 18;
int time_off_min = 0;
int time_reset_hour = 1;
int time_reset_minute = 0;

// Temp sensor
SparkFun_STTS22H tempSensor;
float ext_temp;

// WiFi
const char* ssid = "ESP32_Test";
const char* password = "United625";
WiFiServer server(80);
String header;
const long timeout_time = 2000;

// Timekeeping
unsigned long current_time = millis();
unsigned long previous_time = 0;
unsigned long previous_data_time = 0;
char time_string[10];
AlarmId on_timer;
AlarmId off_timer;
AlarmId reset_timer;

// Charge Controller (Renogy Rover)
RenogyRover rover(255); // Default modbus ID 255
BatteryState battery_state;
ControllerLoadState load_state;
PanelState panel_state;
HistStatistics controller_statistics;
DayStatistics day_statistics;

/********* Function Declarations ********/
void setupNotecard(); //Sets up the notecard
void updateNotecard();  //Updates the notecard
void setupTemp(); //Sets up the temp sensor
void setupWiFi(); //Sets up wifi
void setupController(); //Sets up the connection with the controller
void setupTimer(); //Sets up power on/off timers
void turnOffTimer();  //Deletes all timer objects
void doNotecard();  //Runs notecard update tasks
void doWiFi();  //Runs an ad-hoc web page for status info
void evaluateOutputState(); //Evaluates the output state to the load
void powerOn(); //Turns the load on
void powerOff(); //Turns the load off
void getTempData(); //Gets the current temp data from the optional sensor
time_t getCurrentTimeFromNote();  //Updates the system time from the cellular time
void getCurrentControllerData();  //Polls the controller for current data

void updateSettings();  //Saves new settings to flash
void readSettings();  //Reads settings from flash
void printCurrentSettings();  //Prints the current settings to serial
void sendCurrentSettingsNote();  //Sends a note with the current settings to the cloud

void resetESP();


/********* Default Functions *********/
void setup() {
  delay(2000); //Allow the notecard to boot
  pinMode(LED_BUILTIN, OUTPUT);
  Wire.begin();
  Serial.begin(9600);
  Serial.println("");
  Serial.println("");



  for (int i = 0; i < 5; i++)
  {
    Serial.print(". ");
    delay(1000); //allow the notecard to get started
  }

  //Update settings from flash
  //On first run on a new board, you must run an updateSettings() command to write the flash memory
  //This will be fixed in later versions.
  //updateSettings();
  readSettings();
  printCurrentSettings();

  for (int i = 0; i < 5; i++)
  {
    Serial.print(". ");
    delay(1000); //allow the notecard to get started
  }

  setupNotecard();
  
   // Start time sync services
  setSyncProvider(getCurrentTimeFromNote);

  //Startup other services
  setupTemp();
  setupController();
  setupTimer();
  //setupWiFi();

  // Set up hardware watchdog timer
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  Serial.println("***** Setup Complete *****");
  Serial.println("");
}

void loop() {
  // Poll sensors
  if (tempSensor.dataReady()) {
    tempSensor.getTemperatureC(&ext_temp);
  }

  // do actions
  doNotecard();

  if (wifi_enabled) {
    doWiFi();
  }

  // run output state machine
  evaluateOutputState();

  // reset watchdog timer
  esp_task_wdt_reset();

  //handle alarm scheduling
  Alarm.delay(0);
}

/******** Function Definitions ********/
// ---- Timer ---- //

//Sets up power on/off timers
void setupTimer() {
  
  
  //Erase existing alarm
  Alarm.free(on_timer);
  Alarm.free(off_timer);

  //write new times
  on_timer = Alarm.alarmRepeat(time_on_hour, time_on_min, 0, powerOn);
  off_timer = Alarm.alarmRepeat(time_off_hour, time_off_min, 0, powerOff);

  Serial.print("Timer set to turn on at ");
  Serial.print(hour(Alarm.read(on_timer)));
  Serial.print(":");
  Serial.print(minute(Alarm.read(on_timer)));
  Serial.print(", off at ");
  Serial.print(hour(Alarm.read(off_timer)));
  Serial.print(":");
  Serial.println(minute(Alarm.read(off_timer)));
  
  //Set up global reset timer
  reset_timer = Alarm.alarmRepeat(time_reset_hour, time_reset_minute, 0, resetESP);
  Serial.print("Reset timer set to ");
  Serial.print(hour(Alarm.read(reset_timer)));
  Serial.print(":");
  Serial.println(minute(Alarm.read(reset_timer)));
}

//Deletes all timer objects
void turnOffTimer() {
  Alarm.free(on_timer);
  Alarm.free(off_timer);
}

// ---- Notecard ---- //
//Sets up the notecard
void setupNotecard() {
  Serial.println("Starting Notecard...");
  notecard.begin();
  notecard.setDebugOutputStream(Serial);
  J* req = notecard.newRequest("hub.set");
  JAddStringToObject(req, "product", PRODUCT_UID);
  JAddStringToObject(req, "mode", "periodic");
  JAddNumberToObject(req, "outbound", outbound_interval);
  JAddNumberToObject(req, "inbound", inbound_interval);
  JAddStringToObject(req, "sn", "UC_unit_b");
  notecard.sendRequest(req);

  req = notecard.newRequest("card.location.mode");
  JAddStringToObject(req, "mode", "periodic");
  JAddNumberToObject(req, "seconds", 3600);
  notecard.sendRequest(req);

  req = notecard.newRequest("card.location.track");
  JAddBoolToObject(req, "sync", true);
  JAddBoolToObject(req, "heartbeat", true);
  JAddNumberToObject(req, "hours", 12);
  notecard.sendRequest(req);


}

//updates the notecard
void updateNotecard() {
  J* req = notecard.newRequest("hub.set");
  JAddStringToObject(req, "mode", "periodic");
  JAddNumberToObject(req, "outbound", outbound_interval);
  JAddNumberToObject(req, "inbound", inbound_interval);
  notecard.sendRequest(req);
}

//Runs notecard update tasks
void doNotecard() {
  current_time = millis();
  char power_state_string[4];
  char power_mode_string[10];

  if (current_time > previous_data_time + (logging_interval * 60000)) {
    // Gather data from the controller to send
    getCurrentControllerData();

    // Send data to notecard
    // Sensor data
    /*J *req = notecard.newRequest("note.add");
    if (req != NULL) {
      JAddStringToObject(req, "file", "sensors.qo");
      JAddBoolToObject(req, "sync", true);
      J *body = JAddObjectToObject(req, "body");
      if (body) {
        JAddNumberToObject(body, "temp", temp);
      }
      notecard.sendRequest(req);
    }
    */

    // Controller Data
    //update the time string
    sprintf(time_string, "%02d:%02d:%02d", hour(), minute(), second());

    //Build the controller.qo note
    J* req2 = notecard.newRequest("note.add");
    if (req2 != NULL) {
      JAddStringToObject(req2, "file", "controller.qo");
      //JAddBoolToObject(req2, "sync", true);
      J* body = JAddObjectToObject(req2, "body");
      if (body) {
        J* battery = JAddObjectToObject(body, "battery");
        if (battery) {
          JAddNumberToObject(battery, "StateOfCharge", battery_state.stateOfCharge);
          JAddNumberToObject(battery, "BatteryVoltage", roundf(battery_state.batteryVoltage * 10) / 10);
          JAddNumberToObject(battery, "BatteryTemperature", battery_state.batteryTemperature);
          JAddNumberToObject(battery, "ChargingCurrent", roundf(battery_state.chargingCurrent * 10) / 10);
        }
        J* panel = JAddObjectToObject(body, "panel");
        if (panel) {
          JAddNumberToObject(panel, "PanelVoltage", roundf(panel_state.voltage * 10) / 10);
          JAddNumberToObject(panel, "PanelCurrent", roundf(panel_state.current * 10) / 10);
          JAddNumberToObject(panel, "PanelPower", panel_state.chargingPower);
        }
        J* controller = JAddObjectToObject(body, "controller");
        if (controller) {
          JAddStringToObject(controller, "Controller_Time", time_string);
          JAddNumberToObject(controller, "OSMTemperature", roundf(ext_temp * 10) / 10);
          JAddNumberToObject(controller, "RoverTemperature", battery_state.controllerTemperature);
        }
        J* load = JAddObjectToObject(body, "load");
        if (load) {
          JAddBoolToObject(load, "LoadState", load_state.active);
          JAddNumberToObject(load, "LoadVoltage", roundf(load_state.voltage * 10) / 10);
          JAddNumberToObject(load, "LoadCurrent", roundf(load_state.current * 10) / 10);
          JAddNumberToObject(load, "LoadPower", load_state.power);
        }
        J* statistics = JAddObjectToObject(body, "statistics");
        if (statistics) {
          JAddNumberToObject(statistics, "OperatingDays", controller_statistics.operatingDays);
          JAddNumberToObject(statistics, "OverDischarges", controller_statistics.batOverDischarges);
          JAddNumberToObject(statistics, "FullCharges", controller_statistics.batFullCharges);
          JAddNumberToObject(statistics, "ChargingAH", controller_statistics.batChargingAmpHours);
          JAddNumberToObject(statistics, "DischargingAH", controller_statistics.batDischargingAmpHours);
          JAddNumberToObject(statistics, "PowerGenerated", roundf(controller_statistics.powerGenerated * 10) / 10);
          JAddNumberToObject(statistics, "PowerConsumed", roundf(controller_statistics.powerConsumed * 10) / 10);
        }
        J* day = JAddObjectToObject(body, "dayStatistics");
        if (day) {
          JAddNumberToObject(day, "BattVoltageMin",
            day_statistics.batteryVoltageMinForDay);
          JAddNumberToObject(day, "BattVoltageMax",
            day_statistics.batteryVoltageMaxForDay);
          JAddNumberToObject(day, "MaxChargeCurrent",
            day_statistics.maxChargeCurrentForDay);
          JAddNumberToObject(day, "MaxChargePower",
            day_statistics.maxChargePowerForDay);
          JAddNumberToObject(day, "MaxDischargeCurrent",
            day_statistics.maxDischargeCurrentForDay);
          JAddNumberToObject(day, "MaxDischargePower",
            day_statistics.maxDischargePowerForDay);
          JAddNumberToObject(day, "chargingAH_day",
            day_statistics.chargingAmpHoursForDay);
          JAddNumberToObject(day, "DischargingAH_day",
            day_statistics.dischargingAmpHoursForDay);
          JAddNumberToObject(day, "PowerGenerated_day",
            day_statistics.powerGenerationForDay);
          JAddNumberToObject(day, "PowerConsumed_day",
            day_statistics.powerConsumptionForDay);
        }
      }
      notecard.sendRequest(req2);
    }

    // receive settings data from notecard
    J* req3 = notecard.newRequest("note.get");
    JAddStringToObject(req3, "file", "settingsUpdate.qi");
    JAddBoolToObject(req3, "delete", true);

    J* rsp = notecard.requestAndResponse(req3);
    if (notecard.responseError(rsp)) {
      notecard.logDebug("No notes available");
      Serial.println("");
      // power_state[0] = '\0';
    }
    else {
      J* body = JGetObject(rsp, "body");
      wifi_enabled = JGetBool(body, "WifiEnabled");
      power_on = JGetBool(body, "power_on");
      timer_mode = JGetBool(body, "timer_mode");
      time_on_hour = JGetNumber(body, "time_on_hour");
      time_on_min = JGetNumber(body, "time_on_min");
      time_off_hour = JGetNumber(body, "time_off_hour");
      time_off_min = JGetNumber(body, "time_off_min");
      logging_interval = JGetNumber(body, "logging_interval");
      inbound_interval = JGetNumber(body, "inbound_interval");
      outbound_interval = JGetNumber(body, "outbound_interval");
      wifi_enabled = JGetBool(body, "wifi_enabled");

      if (JGetBool(body, "reset_esp_now")) {
        resetESP();
      }

      if (timer_mode) {
        setupTimer();
      }
      else {
        turnOffTimer();
      }
      updateSettings();
      readSettings();
      updateNotecard();

      Serial.println("Settings updated. Current settings: ");
      printCurrentSettings();
      sendCurrentSettingsNote();
    }
    notecard.deleteResponse(rsp);

    // Update the time
    getCurrentTimeFromNote();

    // Finish the function
    Serial.println("Sensor data transmitted");
    previous_data_time = current_time;
  }
}

void sendCurrentSettingsNote() {
  readSettings();

  //update the time string
  sprintf(time_string, "%02d:%02d:%02d", hour(), minute(), second());

  // build firmware versioning strings
  char firmware_version[10];
  char firmware_date[16];
  sprintf(firmware_version, "%01d.%01d.%01d", firmware_version_prim, firmware_version_sec, firmware_version_tert);
  sprintf(firmware_date, "rev.%02d/%02d/%02d", firmware_updated_d, firmware_updated_m, firmware_updated_y);

  //Read the actual timer settings for the note, don't assume
  char timer_on_string[10];
  char timer_off_string[10];
  sprintf(timer_on_string, "%02d:%02d", hour(Alarm.read(on_timer)), minute(Alarm.read(on_timer)));
  sprintf(timer_off_string, "%02d:%02d", hour(Alarm.read(off_timer)), minute(Alarm.read(off_timer)));

  //Build settings.qo
  J* req4 = notecard.newRequest("note.add");
  if (req4 != NULL) {
    JAddStringToObject(req4, "file", "settings.qo");
    J* body = JAddObjectToObject(req4, "body");
    if (body) {
      JAddStringToObject(body, "firmware_version", firmware_version);
      JAddStringToObject(body, "firmware_date", firmware_date);
      JAddStringToObject(body, "controller_time", time_string);
      JAddBoolToObject(body, "power_on", power_on);
      JAddBoolToObject(body, "timer_mode", timer_mode);
      JAddBoolToObject(body, "wifi_enabled", wifi_enabled);
      JAddNumberToObject(body, "time_on_hour", time_on_hour);
      JAddNumberToObject(body, "time_on_minute", time_on_min);
      JAddNumberToObject(body, "time_off_hour", time_off_hour);
      JAddNumberToObject(body, "time_off_minute", time_off_min);
      JAddStringToObject(body, "timer_on", timer_on_string);
      JAddStringToObject(body, "timer_off", timer_off_string);
      JAddNumberToObject(body, "logging_interval", logging_interval);
      JAddNumberToObject(body, "outbound_interval", outbound_interval);
      JAddNumberToObject(body, "inbound_interval", inbound_interval);
    }
    notecard.sendRequest(req4);
  }


}
//Updates the system time from the cellular time
time_t getCurrentTimeFromNote() {
  // char time_string[12];
  unsigned long current_unix_time = 10;

  int gmt_offset = 0;
  // recieve data from notecard
  J* req3 = notecard.newRequest("card.time");

  J* rsp = notecard.requestAndResponse(req3);
  if (notecard.responseError(rsp)) {
    notecard.logDebug("No time available");
  }
  else {
    current_unix_time = JGetNumber(rsp, "time");
    gmt_offset = JGetNumber(rsp, "minutes");
    current_unix_time = current_unix_time + (gmt_offset * 60);

    setTime(current_unix_time);

    if (send_time_updates) {
      // Send a note indicating the updated time
      J* req = notecard.newRequest("note.add");
      if (req != NULL) {
        JAddStringToObject(req, "file", "time.qo");
        JAddBoolToObject(req, "sync", true);
        J* body = JAddObjectToObject(req, "body");
        if (body) {
          JAddNumberToObject(body, "Updated Unix Time", current_unix_time);
        }
        notecard.sendRequest(req);
      }
    }

    Serial.print("Current time updated: ");
    Serial.print(hour());
    Serial.print(":");
    Serial.println(minute());
  }

  notecard.deleteResponse(rsp);
  return current_unix_time;
}

// ---- Temp Sensor ---- //

//Sets up the temp sensor
void setupTemp() {
  if (!tempSensor.begin()) {
    Serial.println("Temp sensor did not begin.");
    while (1)
      ;
  }

  Serial.println("Temp sensor ready.");

  // Other output data rates can be found in the description
  // above. To change the ODR or mode, the device must first be
  // powered down.
  tempSensor.setDataRate(STTS22H_POWER_DOWN);
  delay(10);
  tempSensor.setDataRate(STTS22H_1Hz);

  // Enables incrementing register behavior for the IC.
  // It is not enabled by default as the datsheet states and
  // is vital for reading the two temperature registers.
  tempSensor.enableAutoIncrement();

  delay(100);
}

//Gets the current temp data from the optional sensor
void getTempData() {
  if (tempSensor.dataReady()) {
    tempSensor.getTemperatureF(&ext_temp);
  }
}

// ---- Output state machine ---- //

//Evaluates the output state to the load
void evaluateOutputState() {
  // Evaluate outputs based on state register
  switch (power_on) {
  case true:
    if (!load_state.active) {
      powerOn();
    }
    break;
  case false:
    if (load_state.active) {
      powerOff();
    }
    break;
  }
}

//Turns the load on
void powerOn() {
  power_on = true;
  Serial.println("Power turned on.");
  digitalWrite(LED_BUILTIN, HIGH);
  if (!load_state.active) {
    rover.setLoadState(1);
    getCurrentControllerData();
  }
}

//Turns the load off
void powerOff() {
  power_on = false;
  Serial.println("Power turned off");
  digitalWrite(LED_BUILTIN, LOW);
  if (load_state.active) {
    rover.setLoadState(0);
    getCurrentControllerData();
  }
}

// ---- Rover Functions ---- //

//Sets up the connection with the controller
void setupController() {
  Serial2.begin(9600, SERIAL_8N1, RDX2, TXD2);
  rover.begin(Serial2);

  // Check to see if data can be pulled
  rover.getBatteryState(&battery_state);
  delay(500);
  if (battery_state.batteryVoltage > 0) {
    Serial.println("RS232 connection to Rover initialized");
  }
  else {
    Serial.println("RS232 connection failed!!!");
  }
}

//Polls the controller for current data
void getCurrentControllerData() {
  rover.getBatteryState(&battery_state);
  rover.getPanelState(&panel_state);
  rover.getControllerLoadState(&load_state);
  rover.getHistoricalStatistics(&controller_statistics);
  rover.getDayStatistics(&day_statistics);
  Serial.println("Controller data updated");
}

// ---- WiFi Functions ---- //

//Sets up wifi
void setupWiFi() {
  // Connect to Wi-Fi network with SSID and password
  Serial.println("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be
  // open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print(" AP IP address: ");
  Serial.println(IP);

  server.begin();
}

//Runs an ad-hoc web page for status info
void doWiFi() {
  WiFiClient client = server.available(); // Listen for incoming clients

  if (client) { // If a new client connects,

    getCurrentControllerData();

    current_time = millis();
    previous_time = current_time;
    Serial.println("New Client.");  // print a message out in the serial port
    String currentLine =
      ""; // make a String to hold incoming data from the client
    while (client.connected() &&
      current_time - previous_time <=
      timeout_time) { // loop while the client's connected
      current_time = millis();
      if (client.available()) { // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n') { // if the byte is a newline character
          // if the current line is blank, you got two newline
          // characters in a row. that's the end of the client HTTP
          // request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g.
            // HTTP/1.1 200 OK) and a content-type so the client
            // knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" "
              "content=\"width=device-width, "
              "initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size
            // attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: "
              "inline-block; "
              "margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; "
              "border: none; color: "
              "white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: "
              "30px; margin: 2px; cursor: "
              "pointer;}");
            client.println(".button2 {background-color: "
              "#555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");

            // Display current state, and ON/OFF buttons for GPIO 26
            client.println("<p>Current Temp: ");
            client.println(String(ext_temp));
            client.println(" Degrees F</p> <br>");

            client.println("<h3>Command States:</h3> <ul>");
            client.println("<li>Power State: ");
            client.println(power_on);
            client.println("</li> <li>Power Mode: ");
            client.println(timer_mode);
            client.println("</li></ul>");

            client.println("<h3>Controller</h3> <ul>");
            client.println("<li> Battery Voltage: ");
            client.println(battery_state.batteryVoltage);
            client.println("</li></ul>");

            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          }
          else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r') { // if you got anything else but a carriage
          // return character,
          currentLine += c;     // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

//Updates the settings saved to flash
void updateSettings() {
  preferences.begin("app_settings", false);

  preferences.putBool("power_on", power_on);
  preferences.putBool("timer_mode", timer_mode);
  preferences.putBool("wifi_enabled", wifi_enabled);
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

void readSettings() {
  preferences.begin("app_settings", false);

  power_on = preferences.getBool("power_on");
  timer_mode = preferences.getBool("timer_mode");
  wifi_enabled = preferences.getBool("wifi_enabled");
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

//Prints the current settings to serial
void printCurrentSettings() {
   // build firmware versioning strings
  char firmware_version[10];
  char firmware_date[16];
  sprintf(firmware_version, "%01d.%01d.%01d", firmware_version_prim, firmware_version_sec, firmware_version_tert);
  sprintf(firmware_date, "rev.%02d/%02d/%02d", firmware_updated_d, firmware_updated_m, firmware_updated_y);

  //Read the actual timer settings for the note, don't assume
  char timer_on_string[10];
  char timer_off_string[10];
  sprintf(timer_on_string, "%02d:%02d", hour(Alarm.read(on_timer)), minute(Alarm.read(on_timer)));
  sprintf(timer_off_string, "%02d:%02d", hour(Alarm.read(off_timer)), minute(Alarm.read(off_timer)));

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

// ---- System Functions ---- //
void resetESP() {
  Serial.println("Restarting ESP");
  ESP.restart();
}