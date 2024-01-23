#include <Arduino.h>
#include <HardwareSerial.h>
#include <Notecard.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#include "RenogyRover.h"
#include "SparkFun_STTS22H.h"
#include "TimeLib.h"

// IO definitions
#define LED_PIN 13;
#define RDX2 16
#define TXD2 17

// Hardware watchdog timeout
#define WDT_TIMEOUT 5 // in seconds

// Notecard
#define PRODUCT_UID "com.unitedconsulting.clee:solarmonitor"
#define SEND_INTERVAL 15000
bool send_time_updates = false;
Notecard notecard;
float notecard_temp;

// Changeable Settings
int logging_interval = 1; // in minutes
int outbound_interval = 5;
int inbound_interval = 5;
enum PowerStates { on, off };
enum PowerModes { manual, timer };
bool wifi_enabled = false;      // default state is off
PowerStates power_state = on;   // default state is on
PowerModes power_mode = manual; // default state is manual
int time_on_hour;
int time_on_min;
int time_off_hour;
int time_off_min;

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

// Charge Controller (Renogy Rover)
RenogyRover rover(255); // Default modbus ID 255
BatteryState battery_state;
ControllerLoadState load_state;
PanelState panel_state;
HistStatistics controller_statistics;
DayStatistics day_statistics;

/********* Function Declarations ********/
void setupNotecard();
void setupTemp();
void setupWiFi();
void setupController();

void doNotecard();
void doWiFi();

void evaluateOutputState();

void getTempData();
time_t getCurrentTimeFromNote();
void getCurrentControllerData();

void printCurrentSettings();

/********* Default Functions *********/
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Wire.begin();
  Serial.begin(9600);

  setupNotecard();
  setupTemp();
  setupWiFi();
  setupController();

  // Start time sync services
  setSyncProvider(getCurrentTimeFromNote);

  // Set up hardware watchdog timer
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
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
}

/******** Function Definitions ********/

// ---- Notecard ---- //
void setupNotecard() {
  notecard.begin();
  notecard.setDebugOutputStream(Serial);
  J* req = notecard.newRequest("hub.set");
  JAddStringToObject(req, "product", PRODUCT_UID);
  JAddStringToObject(req, "mode", "periodic");
  JAddNumberToObject(req, "outbound", outbound_interval);
  JAddNumberToObject(req, "inbound", inbound_interval);
  JAddStringToObject(req, "sn", "UC_unit_b");
  notecard.sendRequest(req);
}

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

    // receive data from notecard
    J* req3 = notecard.newRequest("note.get");
    JAddStringToObject(req3, "file", "data.qi");
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
      strncpy(power_state_string, JGetString(body, "power_state"),
        sizeof(power_state_string));
      strncpy(power_mode_string, JGetString(body, "power_mode"),
        sizeof(power_mode_string));
      time_on_hour = JGetNumber(body, "time_on_hour");
      time_on_min = JGetNumber(body, "time_on_min");
      time_off_hour = JGetNumber(body, "time_off_hour");
      time_off_min = JGetNumber(body, "time_off_min");
      Serial.println("Settings updated. Current settings: ");
      printCurrentSettings();

      if (!strncmp(power_state_string, "on", sizeof("on"))) {
        power_state = on;
      }
      else {
        power_state = off;
      }

      if (!strncmp(power_mode_string, "timer", sizeof("timer"))) {
        power_mode = timer;
      }
      else {
        power_mode = manual;
      }
    }

    notecard.deleteResponse(rsp);

    // Update the time
    getCurrentTimeFromNote();

    // Finish the function
    Serial.println("Sensor data transmitted");
    previous_data_time = current_time;
  }
}

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

void getTempData() {
  if (tempSensor.dataReady()) {
    tempSensor.getTemperatureF(&ext_temp);
  }
}

// ---- Output state machine ---- //
void evaluateOutputState() {
  // Evaluate outputs based on state register
  switch (power_state) {
  case on:
    digitalWrite(LED_BUILTIN, HIGH);
    if (!load_state.active) {
      rover.setLoadState(1);
    }

    break;
  case off:
    digitalWrite(LED_BUILTIN, LOW);
    if (load_state.active) {
      rover.setLoadState(0);
    }
    break;

  default:
    break;
  }

  // Set state register based on mode/timer
  switch (power_mode) {
  case timer:
    if (hour() == time_on_hour) {
      if (minute() >= time_on_min) {
        power_state = on;
      }
      else {
        power_state = off;
      }
    }
    else if (hour() == time_off_hour) {
      if (minute() >= time_off_min) {
        power_state = off;
      }
      else {
        power_state = on;
      }
    }
    else if (hour() > time_on_hour && hour() < time_off_hour) {
      power_state = on;
    }
    else {
      power_state = off;
    }
    break;

  default:
    break;
  }
}

// ---- Rover Functions ---- //
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

void getCurrentControllerData() {
  rover.getBatteryState(&battery_state);
  rover.getPanelState(&panel_state);
  rover.getControllerLoadState(&load_state);
  rover.getHistoricalStatistics(&controller_statistics);
  rover.getDayStatistics(&day_statistics);
}

// ---- WiFi Functions ---- //
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
            client.println(power_state);
            client.println("</li> <li>Power Mode: ");
            client.println(power_mode);
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

void printCurrentSettings() {
  Serial.print("Power State: ");
  Serial.println(power_state);
  Serial.print("Power Mode: ");
  Serial.println(power_mode);
  Serial.print("Time on: ");
  Serial.print(time_on_hour);
  Serial.print(":");
  Serial.println(time_on_min);
  Serial.print("Time off: ");
  Serial.print(time_off_hour);
  Serial.print(":");
  Serial.println(time_off_min);
}
