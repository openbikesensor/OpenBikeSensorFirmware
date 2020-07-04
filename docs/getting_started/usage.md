---
layout: default
title: Usage
parent: Getting Started
nav_order: 2
---


# Use the OpenBikeSensor

## Quick start guide
> This assumes you have a working OpenBikeSensor. [If not start here](/readme.md)

In case you might get a OpenBikeSensor, you should do some first steps.

1. Check, if your handlebar fits the OpenBikeSensor's display/button holder.  
There are basically two different kinds of handlebars: thinner ones, with a 25,4mm diameter and the other ones, with a 31.8mm diameter. You might create some kind of adapter, if the device is designed for 31,8mm and you have a 25,4mm handlebar, but the other way around is not possible
2. Measure the distance between the outer edge of the device and the end of your handlebar. Enter these values into the configuration options.
3. Set your privacy zone(s). You could probably get the GPS location with any digital map app. Enter these values into the configuration options.
4. Check the SD card, if it's ok and - in best case - empty.
5. Load the device's battery.
6. Put it on the bike and go for a ride. Please take care about traffic around you and don't let the OpenBikeSensor distract you!


## Wiki

### Use

Don't put too much attention on the OpenBikeSensor, always take care about the traffic around you!  
* Power on the device and - if possible - wait until the device has a GPS fix, i.e. your GPS location. Since it's not using the location services, you know from your mobile phone, this might take some time. The display will exit the status screen, as soon as your location is known. In case you can't wait due to some reasons, you could skip the wait with a push of the button. While moving, it might take up to 15mins, until the devices knows where you are.
* Ride your bike and push the button every time you're passed (no matter if it's a car or truck or bus or motorbike). To get some idea, how often close passes occur, it's important to confirm every pass, not only the close ones. 
* Power off. Keep in mind to always keep the button pushed, when you switch off the device!

### Configuration

You can enter the configuration mode by pushing the button while turning on the device. Then it will either open a unique WiFi access point, including the device's MAC address named "OpenBikeSensor-xxxxxxxxxxxx" with the initial password "12345678" or try to connect to an existing WiFi if credentials were entered in a previous configuration. The configuration page can be found on "http://openbikesensor.local" or "172.20.0.1" on the AP or on IP adress shown on the device's display. It might be neccessary to de-activate the mobile data on your mobile phone to access this page.  
You can directly upload a precompiled binary; the latest release can always be found [here](https://github.com/Friends-of-OpenBikeSensor/OpenBikeSensorFirmware/releases).  
There are several chapters in the configuration.

#### Update Firmware

After downloading the latest release (or any other version, in case you need a special setup), just click on "Update" in the options. Select the downloaded file and click update. The device will automatically reboot after a successful update.

#### Privacy Zones

You could set as many privacy zones, as you like, including their own radius. In the options you could define, how the OpenBikeSensor behaves in these zone(s).

#### Config

##### Sensor

Here you can define the offset between the end of your handle bar and the outer edge of the OpenBikeSensor. These values will automatically get substracted from the current measurement. Additionally you could "swap" the left and right measurement, in case you mounted the device different.

##### GPS

You could define, in which way your device will acknowledge a valid GPS fix.

##### Generic Display

You could flip the display, if you need to mount it upside down or invert it, which might help in bright sunlight.

##### Measurement Display

Here are several options, which values you might want to see on your display. It includes a "simple mode", where you only see the measurement to the left.

##### Privacy Options

To keep some privacy, you could tell your device to stop recording near your home or any other privacy zone. This could be no recording at all or just no GPS-tracking any more, but still storing all confirmed passes.

#### WiFi Settings

The OpenBikeSensor can also connect to available WiFis if you supply credentials. This is convenient when you want to stay connected to your local WiFi when configuring the sensor. It will tell you the IP adress to connect to on the display.

#### Reboot

This button restarts the device into the regular measurement mode and leaves the options.
