# United OSM
### An open-source solar power monitor

## Project Background
This project is intended to provide a IOT-style monitoring system for deployed solar power systems, via LTE. This project leverages the Blues Wireless notecard as the primary method of cellular communication, controlled by an ESP32. The ESP32 interfaces with compatible solar charge controllers to log data to the cloud and control attached loads, if supported by the controller.

This project was developed for United Consulting by Christopher Lee. 

## Project Requirements
- This project is written for use with ESP32 hardware only.
- This project is compatible with Renogy solar charge controllers which utilize modbus communication via either RS232 or RS485. Currently, communicating via CAN is not supported.
- Communication is provided by a Blues Wireless Notecard, with their Notehub cloud service handing event routing and data logging. Support for routing data to InitialState, andlong with the appropriate JSONata expresson, is provided.

## V1.0 Hardware
Version 1.0 is built using entirely off-the-shelf hardware, utilizing Sparkfun's Qwiic ecosystem. Exact hardware used in V1.0 is as follows:
- Sparkfun Thing Plus ESP32, with U.FL connector
- Sparkfun Qwiic Cellular notecard carrier
- Blues Wireless cellular notecard
- Sparkfun STTS22H Qwiic temperature sensor breakout
- Sparkfun AP63357DV 5v buck regulator breakout
- Off-brand RS232 transciever based on the Max SP3232E chipset

Simple, off-the-shelf prototyping boards were selected for this version due to ease of hardware development. Future versions of this project will include a custom PCB to reduce the number of finicky interconencts between boards, and to reduce the overall device footprint.

## Controller interconnect cable design
To simplify construction, an "active cable" containing the RS232 tranciever was constructed. This connects to a RJ-12 jack on the Renogy Rover series of solar charge controllers, and to a RJ-45 jack on the UnitedOSM module. This provides both a 12v power supply to the UnitedOSM module, and data connection and conversion from TTL to RS232.

### Cable pinout
- **RJ-12 Connector**
1. TX - Blue
2. RX - White/Blue
3. Power/Signal GND - Brown
4. Power/Signal GND - White/Brown
5. +12V - Orange
6. +12V - White/Orange
7. Not used - Green
8. Not used - White/green 

- **RJ-45 Connector**
1. TX - Blue
2. RX - White/Blue
3. Power GND - Brown
4. Signal GND - White/Brown
5. +12V - Orange
6. +12V - White/Orange
7. VCC (3.3v) - Green
8. Not used - White/Green

    ***Note: Signal GND (White/brown) is terminated at the RS232 transciever, this cannot be used for any other purposes.***