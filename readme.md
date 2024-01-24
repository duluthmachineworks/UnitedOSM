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