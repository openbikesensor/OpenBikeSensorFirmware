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
| OBS         | `1FE7FAF9-CE63-4236-0004-000000000000` | Reports distance sensor readings and confirmed close passes        |
| Heart Rate  | `0000180D-0000-1000-8000-00805F9B34FB` | Transmits the current distance by acting like a heart rate monitor |


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
and right values are in cm, a value of `0xffff` means no measurement. The message is
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

