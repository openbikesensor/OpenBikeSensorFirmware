# OpenBikeSensor

[![GitHub version](https://img.shields.io/github/v/release/openbikesensor/OpenBikeSensorFirmware)](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases/latest)
[![GitHub version](https://img.shields.io/github/release-date/openbikesensor/OpenBikeSensorFirmware)](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases/latest)
[![GitHub downloads](https://img.shields.io/github/downloads/openbikesensor/OpenBikeSensorFirmware/latest/total)](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases/latest)
![GitHub commits since latest release (by date)](https://img.shields.io/github/commits-since/openbikesensor/OpenBikeSensorFirmware/latest)


[![GitHub version](https://img.shields.io/github/v/release/openbikesensor/OpenBikeSensorFirmware?include_prereleases&label=pre-release)](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases)
[![GitHub version](https://img.shields.io/github/release-date-pre/openbikesensor/OpenBikeSensorFirmware?label=pre-release+date)](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases/latest)
[![GitHub last commit](https://img.shields.io/github/last-commit/openbikesensor/OpenBikeSensorFirmware)](https://github.com/openbikesensor/OpenBikeSensorFirmware/commits/master)
[![GitHub downloads](https://img.shields.io/github/downloads-pre/openbikesensor/OpenBikeSensorFirmware/latest/total)](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases/latest)


[![GitHub downloads](https://img.shields.io/github/downloads/openbikesensor/OpenBikeSensorFirmware/total)](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases/latest)
[![OpenBikeSensor - CI](https://github.com/openbikesensor/OpenBikeSensorFirmware/workflows/OpenBikeSensor%20-%20CI/badge.svg)](https://github.com/openbikesensor/OpenBikeSensorFirmware/actions?query=workflow%3A%22OpenBikeSensor+-+CI%22)
[![Open Issues](https://img.shields.io/github/issues/openbikesensor/OpenBikeSensorFirmware)](https://github.com/openbikesensor/OpenBikeSensorFirmware/issues)
[![Contributors](https://img.shields.io/github/contributors/openbikesensor/OpenBikeSensorFirmware)](https://github.com/openbikesensor/OpenBikeSensorFirmware/contributors)
[![quality gate](https://sonarcloud.io/api/project_badges/measure?project=openbikesensor_OpenBikeSensorFirmware&metric=alert_status)](https://sonarcloud.io/dashboard?id=openbikesensor_OpenBikeSensorFirmware)
[![bugs](https://sonarcloud.io/api/project_badges/measure?project=openbikesensor_OpenBikeSensorFirmware&metric=bugs)](https://sonarcloud.io/project/issues?id=openbikesensor_OpenBikeSensorFirmware&resolved=false&types=BUG)
[![Slack](https://img.shields.io/badge/Slack-chat-success)](https://www.openbikesensor.org/slack/)


Platform for measuring data on a bicycle and collecting it.
Currently, we measure the distance with either HC-SR04 or JSN-SR04T 
ultrasonic sensors connected to an ESP32.


## Description

Inspired by the Berlin project Radmesser. This version uses a simple push 
button at the handle bar to confirm distance-measures were actually overtaking 
vehicles. It has its own GPS, and a SD card for logging, so it does not 
require any additional hardware (like a smartphone).

Starting with version v0.3 the firmware also exposes the measured data via 
[BLE bluetooth](https://github.com/openbikesensor/OpenBikeSensorFirmware/blob/master/docs/software/firmware/bluetooth_services.md).
You can use this to feed data to the
[SimRa](https://www.mcc.tu-berlin.de/menue/forschung/projekte/simra/) App or
any other app collecting heart-rate BLE data. 

Unfortunately we had to temporarily remove the bluetooth code in v0.5.x because
of resource limitations. Plan is to enable this again as soon as we manage
the resource limitations.


## Updating

update functionality. For the 1st flashing of the firmware or the update
from a version prior v0.3.x you have to plug in USB.
See [flash documentation](https://github.com/openbikesensor/OpenBikeSensorFirmware/blob/master/docs/software/firmware/initial_flash.md)   
for a documentation how to do a manual flash it if you do not want to 
set up an IDE as described below.


## Find the documentation

You can find the OpenBikeSensor documentation under:
* [openbikesensor.org](https://www.openbikesensor.org/)


## Getting Started

1. You need a OpenBikeSensor in order to try work on the Firmware. [Head over to our Hardware Guide to assemble one](https://www.openbikesensor.org/hardware/).
2. Clone this repo: `git clone https://github.com/openbikesensor/OpenBikeSensorFirmware.git` and `cd` into it.
3. Choose between developing with recommended [VSCode](https://www.openbikesensor.org/software/firmware/setup.html#vscode), 
   or [CLion](https://www.openbikesensor.org/software/firmware/setup.html#clion) (license required), 
   respectively [Arduino IDE](https://www.openbikesensor.org/software/firmware/setup.html#clion) (discouraged).
4. Happy Coding! :tada: We are open for your pull request.
