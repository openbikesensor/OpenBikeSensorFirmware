---
layout: default
title: Bluetooth Services
parent: Firmware
grand_parent: Software
---

# Overview

| Service     | UUID                                   | Description                                                        |
| ----------- | -------------------------------------- | ------------------------------------------------------------------ |
| Device Info | `0000180A-0000-1000-8000-00805F9B34FB` | General information about the bluetooth device                     |
| Heart Rate  | `0000180D-0000-1000-8000-00805F9B34FB` | Transmits the current distance by acting like a heart rate monitor |
| Distance    | `1FE7FAF9-CE63-4236-0001-000000000000` | Transmits the current distance every 50ms                          |
| Connection  | `1FE7FAF9-CE63-4236-0002-000000000000` | Connection status for interactive pairing                          |
| Close Pass  | `1FE7FAF9-CE63-4236-0003-000000000000` | Detects and transmits possible close passes                        |
| OBS         | `1FE7FAF9-CE63-4236-0004-000000000000` | Reports distance sensor readings and confirmed close passes        |


## Device Info Service
- *Description:* General information about the bluetooth device
- *UUID:* `0000180A-0000-1000-8000-00805F9B34FB`

| Characteristic    | UUID                                   | Property | Value              |
| ----------------- | -------------------------------------- | -------- | ------------------ |
| System Id         | `00002a23-0000-1000-8000-00805f9b34fb` | `READ`   | ?                  |
| Model Number      | `00002a24-0000-1000-8000-00805f9b34fb` | `READ`   | `H7`               |
| Serial Number     | `00002a25-0000-1000-8000-00805f9b34fb` | `READ`   | `20182DBA0B`       |
| Firmware Revision | `00002a26-0000-1000-8000-00805f9b34fb` | `READ`   | `1.4.0`            |
| Hardware Revision | `00002a27-0000-1000-8000-00805f9b34fb` | `READ`   | `39044024.10`      |
| Software Revision | `00002a28-0000-1000-8000-00805f9b34fb` | `READ`   | `H7 3.1.0`         |
| Manufacturer Name | `00002a29-0000-1000-8000-00805f9b34fb` | `READ`   | `Polar Electro Oy` |


## Heart Rate Service
- *Description:* Transmits the current distance by acting like a heart rate monitor
- *UUID:* `0000180D-0000-1000-8000-00805F9B34FB`

| Characteristic  | UUID                                   | Property         | Value                                     |
| --------------- | -------------------------------------- | ---------------- | ----------------------------------------- |
| Heart Rate      | `00002a37-0000-1000-8000-00805f9b34fb` | `READ`, `NOTIFY` | Minimum distance measured over one second |
| Sensor Location | `00002a38-0000-1000-8000-00805f9b34fb` | `READ`           | `1`                                       |


## Distance Service
- *Description:* Transmits the current distance every 50ms
- *UUID:* `1FE7FAF9-CE63-4236-0001-000000000000`

| Characteristic | UUID                                   | Property         | Value                                            |
| -------------- | -------------------------------------- | ---------------- | ------------------------------------------------ |
| Distance 50 ms | `1FE7FAF9-CE63-4236-0001-000000000001` | `READ`, `NOTIFY` | Current distance of all sensors with a timestamp |

The format of the transmitted string is `"timestamp;[leftSensor1, leftSensor2, ...];[rightSensor1, rightSensor2, ...]"`, e.g. `"43567893;100,30;400"` or `"43567893;100;"`.
The list of sensor values for one side might be empty but the entire transmitted string can be safely split on `";"` and each sensor value list safely on `","`.


## Connection Service
- *Description:* Connection status for interactive pairing
- *UUID:* `1FE7FAF9-CE63-4236-0002-000000000000`

| Characteristic    | UUID                                   | Property        | Value |
| ----------------- | -------------------------------------- | --------------- | ----- |
| Connection Status | `1FE7FAF9-CE63-4236-0002-000000000001` | `READ`,`NOTIFY` | `1`   |


## Close Pass Service
- *Description:* Detects and transmits possible close passes
- *UUID:* `1FE7FAF9-CE63-4236-0003-000000000000`

| Characteristic      | UUID                                   | Property        | Value                                                                               |
| ------------------- | -------------------------------------- | --------------- | ----------------------------------------------------------------------------------- |
| Close Pass Distance | `1FE7FAF9-CE63-4236-0003-000000000001` | `READ`,`NOTIFY` | During a close pass, transmits the current distance of all sensors with a timestamp |
| Close Pass Events   | `1FE7FAF9-CE63-4236-0003-000000000002` | `READ`,`NOTIFY` | Notifies of new close pass events                                                   |

The format of the transmitted string for the *distance characteristic* is `"timestamp;[leftSensor1, leftSensor2, ...];[rightSensor1, rightSensor2, ...]"`, e.g. `"43567893;100,30;400"` or `"43567893;100;"`.
The list of sensor values for one side might be empty but the entire transmitted string can be safely split on `";"` and each sensor value list safely on `","`.

The format of the transmitted string for the *event characteristic* is `"timestamp;eventName;[payload1, payload2, ...]"`, e.g. `"43567893;button;"` or `"43567893;avg2s;142,83"`.
The following events are defined:
* `button`: Triggered using a physical button
  * Payload: last distance value
* `avg2s`: Triggered when the average of a two second time window is below the mininum distance threshold of 200 cm
  * Payload: average distance; smallest distance

## OBS Service
- *Description:* Transmits current sensor readings
- *UUID:* `1FE7FAF9-CE63-4236-0004-000000000000`

| Characteristic      | UUID                                   | Property        | Value                                                                               |
| ------------------- | -------------------------------------- | --------------- | ----------------------------------------------------------------------------------- |
| Time                | `1FE7FAF9-CE63-4236-0004-000000000001` | `READ`          | reports the value of the ms timer of the OBS unit, can be used to synchronize time  |
| Sensor Distance     | `1FE7FAF9-CE63-4236-0004-000000000002` | `READ`,`NOTIFY` | Gives sensor reading of the left and right sensor.                                  |
| Close Pass          | `1FE7FAF9-CE63-4236-0004-000000000003` | `READ`,`NOTIFY` | Notifies of button confirmed close pass events.                                     |

The time is sent as uint32. The time counts linear milliseconds from the start 
of the obs. The value returned is the timer value at the time of the read.

*Sensor Distance* delivers the time of the left measurement with the current values of 
the left and right sensor. Content is time uint32, left uint16, right uint16. Left 
and right values are in cm, a value of 0xfff means no measurement.

*Close Pass* events are triggered when a pass was confirmed by button press. The 
values are same as with the *Sensor Distance* characteristic. Note that this 
information is sent after confirmation, so the timer information must be used 
to match the event to the correct time and so location.