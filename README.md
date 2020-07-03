# OpenBikeSensor

Platform for measuring data on a bicyle and collecting it.
Currently we measure the distance with HC-SR04 ultrasonic sensors connected to an ESP32.

## Description

Inspired by the Berlin project Radmesser. This version uses a simple push button at the handle bar to confirm distance-measures were actually overtaking vehicles. It has its own GPS and a SD card for logging, so it does not require any additional hardware (like a smartphone).

## Getting Started

1. You need a OpenBikeSensor in order to try work on the Firmware. [Head over to our Hardware Guide to build one.](/docs/guides/01_hardware.md)
2. Clone the repo: `git clone https://github.com/Friends-of-OpenBikeSensor/OpenBikeSensorFirmware.git` and `cd` into it.
2. Choose between developing with recommended [VSCode](/docs/guides/02_setup.md#vscode) (easiest setup) or [CLion](/docs/guides/02_setup.md#clion) (license required), respectively [Arduino IDE](/docs/guides/02_setup.md#arduino) (discouraged).
3. Happy Coding! 🎉

## Acknowledgments

Inspiration, code snippets, etc.
* [Use of ArduinoJson Library](https://arduinojson.org/v6/example/config/)
* [Interface for OTA Web Updates by LastMinuteEngineers.com](https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/)
