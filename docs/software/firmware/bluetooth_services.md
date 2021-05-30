---
layout: default
title: Bluetooth Services
parent: Firmware
grand_parent: Software
---

# Overview

Note: Bluetooth support is currently disabled because of flash size constraints in ESP32. Contributions as way around this are welcome. 


| Service     | UUID                                   | Description                                                        |
| ----------- | -------------------------------------- | ------------------------------------------------------------------ |
| Device Info | `0000180A-0000-1000-8000-00805F9B34FB` | General information about the bluetooth device                     |
| OBS         | `1FE7FAF9-CE63-4236-0004-000000000000` | Reports distance sensor readings and confirmed close passes        |
| Heart Rate  | `0000180D-0000-1000-8000-00805F9B34FB` | Transmits the current distance by acting like a heart rate monitor |
| Distance    | `1FE7FAF9-CE63-4236-0001-000000000000` | Transmits the current distance every 50ms                          |
| Close Pass  | `1FE7FAF9-CE63-4236-0003-000000000000` | Detects and transmits possible close passes                        |


## Device Info Service
- *Description:* General information about the bluetooth device
- *UUID:* `0000180A-0000-1000-8000-00805F9B34FB`

| Characteristic    | UUID                                   | Property | Value              |
| ----------------- | -------------------------------------- | -------- | ------------------ |
| Firmware Revision | `00002a26-0000-1000-8000-00805f9b34fb` | `READ`   | `vX.Y.ZZZZ`        |
| Manufacturer Name | `00002a29-0000-1000-8000-00805f9b34fb` | `READ`   | `openbikesensor.org` |


## OBS Service
- *Description:* Transmits current sensor readings
- *UUID:* `1FE7FAF9-CE63-4236-0004-000000000000`

| Characteristic      | UUID                                   | Property        | Value                                                                               |
| ------------------- | -------------------------------------- | --------------- | ----------------------------------------------------------------------------------- |
| Time                | `1FE7FAF9-CE63-4236-0004-000000000001` | `READ`          | reports the value of the ms timer of the OBS unit, can be used to synchronize time  |
| Sensor Distance     | `1FE7FAF9-CE63-4236-0004-000000000002` | `NOTIFY`        | Gives sensor reading of the left and right sensor.                                  |
| Close Pass          | `1FE7FAF9-CE63-4236-0004-000000000003` | `NOTIFY`        | Notifies of button confirmed close pass events.                                     |
| Offset              | `1FE7FAF9-CE63-4236-0004-000000000004` | `READ`          | Configured handle bar offset values in cm.                                          |
| Track Id            | `1FE7FAF9-CE63-4236-0004-000000000005` | `READ`          | UUID as text to uniquely identify the current recorded track.                       |

This service uses binary format to transfer time counter as unit32 and unt16
for distance in cm.

*Time* is sent as uint32. The time counts linear milliseconds from the start
of the obs. The value returned is the timer value at the time of the read.

*Sensor Distance* delivers the time of the left measurement with the current values of
the left and right sensor. Content is time uint32, left uint16, right uint16. Left
and right values are in cm, a value of 0xfff means no measurement. The message is
sent immediately after the measure, so you can assume that the reported time
is current at the ESP.

*Close Pass* events are triggered when a pass was confirmed by button press. The
values are same as with the *Sensor Distance* characteristic. Note that this
information is sent after confirmation, so the timer information must be used
to match the event to the correct time and so location.

*Offset* reports the configured handle bar offsets in cm on the OBS side. This
is purely to ease the user configuration. The offset is not considered in any
of the other reported values. The service uses 2 uint16 values to report the
left and right offset in cm in that order.

*Tack Id* holds a UUID as String representation that can be used to uniquely
identify the recorded track. If the value changes, a new track is recorded, and
the millisecond counter on the OBS likely is restarted. 

## Heart Rate Service
- *Description:* Transmits the current distance by acting like a heart rate monitor
- *UUID:* `0000180D-0000-1000-8000-00805F9B34FB`

| Characteristic  | UUID                                   | Property         | Value                                     |
| --------------- | -------------------------------------- | ---------------- | ----------------------------------------- |
| Heart Rate      | `00002a37-0000-1000-8000-00805f9b34fb` | `NOTIFY`         | Minimum distance measured over 1/2 second |


## Distance Service
- *Description:* Transmits the current distance every 50ms
- *UUID:* `1FE7FAF9-CE63-4236-0001-000000000000`

Might get removed soon, prefer the "OBS Service".

| Characteristic | UUID                                   | Property         | Value                                            |
| -------------- | -------------------------------------- | ---------------- | ------------------------------------------------ |
| Distance 50 ms | `1FE7FAF9-CE63-4236-0001-000000000001` | `READ`, `NOTIFY` | Current distance of all sensors with a timestamp |

The format of the transmitted string is `"timestamp;[leftSensor1, leftSensor2, ...];[rightSensor1, rightSensor2, ...]"`, e.g. `"43567893;100,30;400"` or `"43567893;100;"`.
The list of sensor values for one side might be empty but the entire transmitted string can be safely split on `";"` and each sensor value list safely on `","`.


## Close Pass Service
- *Description:* Detects and transmits possible close passes
- *UUID:* `1FE7FAF9-CE63-4236-0003-000000000000`

Might get removed soon, prefer the "OBS Service".

| Characteristic      | UUID                                   | Property        | Value                                                                               |
| ------------------- | -------------------------------------- | --------------- | ----------------------------------------------------------------------------------- |
| Close Pass Events   | `1FE7FAF9-CE63-4236-0003-000000000002` | `READ`,`NOTIFY` | Notifies of new close pass events                                                   |

The format of the transmitted string for the *distance characteristic* is `"timestamp;[leftSensor1, leftSensor2, ...];[rightSensor1, rightSensor2, ...]"`, e.g. `"43567893;100,30;400"` or `"43567893;100;"`.
The list of sensor values for one side might be empty, but the entire transmitted string can be safely split on `";"` and each sensor value list safely on `","`.

The format of the transmitted string for the *event characteristic* is `"timestamp;eventName;[payload1, payload2, ...]"`, e.g. `"43567893;button;"` or `"43567893;avg2s;142,83"`.
The following events are defined:
* `button`: Triggered using a physical button
  * Payload: last distance value
