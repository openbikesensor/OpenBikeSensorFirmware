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

The update mechanism consists of 2 parts. A small firmware called Flash App
is used to read the new firmware from the SD card and flash it to the obs.

So as a 1st step you need to make sure that the Flash App part is installed
on you ESP, you can do so by using "Update Flash App" button and start the 
update b pressing "Update to vX.Y.ZZZ". The data is directly downloaded from
GitHub, so the ESP needs to have access to the internet via your WiFi. You
can also download the `flash.bin` directly from the 
[GitHub releases](https://github.com/openbikesensor/OpenBikeSensorFlash/releases) 
page. The direct update from GitHub has currently no progress bar so be
patient.

Now you need to download the OBS Firmware and place it as `/sdflash/app.bin`
on the SD Card. This is also fully automated behind the "Update Firmware"
button, There you can again directly download the latest version and start
the internal update process with the "Update to vX.Y.ZZZ" button. Make sure 
it is a version newer v0.6.x. At the time of writing there is no such version 
released yet. Do not downgrade to a version prior v0.6.x, once the Flash App 
was installed. A hint on the "Update Firmware" page will show the version of
the Flash App you have installed or if the Flash App is not installed hint 
you to install the Flash App 1st. With the "File Upload" option you can 
also update to locally built versions. Make sure they are pos v0.6 versions!
The 1st update includes a repartitioning of the ESPs flash where you
do not see any indication of progress. Be patient all does not take more
than one or two minutes after the upload was completed.

The About page gives you some insight of the current partitioning a more 
detailed documentation will come. 



### from v0.5.x or earlier

With version v0.6 a new update path was introduced. To allow updating, 
to versions of v0.6 or newer you at least hav to have version v0.6 installed.

The update process is as usual, download the latest v0.6.x from GitHub
and use the "Update Firmware" functionality in server mode.

### from v0.2.x or earlier

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


## License

    Copyright (C) 2020-2021 OpenBikeSensor Contributors
    Contact: https://openbikesensor.org

    This file is part of the OpenBikeSensor firmware.

    The OpenBikeSensor firmware is free software: you can
    redistribute it and/or modify it under the terms of the GNU Lesser General
    Public License as published by the Free Software Foundation, either version
    3 of the License, or (at your option) any later version.

    The OpenBikeSensor Website and Documentation is distributed in the hope
    that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with the OpenBikeSensor Website and Documentation. If not, see
    <http://www.gnu.org/licenses/>.