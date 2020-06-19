# OpenBikeSensor

Distance Measurement HC-SR04 ultrasonic sensors connected to an ESP32

## Description

Inspired by the Berlin project Radmesser. This version uses a simple push button at the handle bar to confirm distance-measures were actually overtaking vehicles. It has its own GPS and a SD card for logging, so it does not require any additional hardware (like a smartphone).

## Getting Started

### Hardware

The prototype by [Zweirat](https://zweirat-stuttgart.de/projekte/openbikesensor/) used the following:
* [ESP32](https://www.az-delivery.de/products/esp32-developmentboard)
* [HC-SR04P](https://www.google.com/search?q=HC-SR04P&tbm=shop)
* [5-pin XS9 Aviation Connector](https://www.aliexpress.com/item/32512693653.html)
* [12mm Push Button](https://www.aliexpress.com/item/4000295670163.html)
* [0.96 inch OLED Display](https://www.aliexpress.com/item/32896971385.html)
* [USB-C Charging Module](https://www.ebay.de/itm/173893903484)
* [GPS Module](https://www.ebay.de/itm/GPS-NEO-6M-7M-8M-GY-GPS6MV2-Module-Aircraft-Flight-Controller-For-Arduino/272373338855)
* Plenty of wires (0.25mm^2) and heat-shrink tubing

To power the sensor you have a choice of Lithium-Iron or Lithium-Ion batteries
* [Automatic Buck-Boost Step Up Down Module for LiPo usage](https://www.ebay.de/itm/264075497616)
* [LiIon battery](https://www.akkuparts24.de/Samsung-INR18650-25R-36V-2500mAh-Li-Ion-Zelle)

or

* [Battery-Protection-Board](https://www.ebay.de/itm/1S-Cell-18650-LiFePo4-Battery-Charger-12A-3-2V-BMS-Protection-PCB-Board-Circuit/122651145073)
* [LiFePo charging module](https://www.ebay.de/itm/MicroUSB-TP5000-3-6v-1A-Charger-Module-3-2v-LiFePO4-Lithium-Battery-Charging-/122164745507) or [alternative](https://de.aliexpress.com/item/4000310107151.html)
* [LiFePo-Battery](https://www.akkuteile.de/lifepo-akkus/18650/a123-apr18650m-a1-1100mah-3-2v-3-3v-lifepo4-akku/a-1006861/)

Li-Ion batteries are usually cheaper and have higher capacity at the same size. Lithium-Iron batteries are considered quite safe.

Screws and nuts:
* [1x M5x35](https://www.amazon.de/gp/product/B078TNC9H1)
* [2x M4x30](https://www.amazon.de/gp/product/B01IMGZTT0)
* [7x M3x45](https://www.amazon.de/gp/product/B07KTBYPFP)
* [4x M2x12](https://www.amazon.de/gp/product/B078TQYZVX)
* [1x M2x10](https://www.amazon.de/gp/product/B01GQX070W)
* [1x M5 Nut](https://www.amazon.de/gp/product/B07961ZH1B)
* [2x M4 Nut](https://www.amazon.de/gp/product/B07961ZH19)
* [7x M3 Nut](https://www.amazon.de/gp/product/B01H8XN99A)
* [5x M2 Nut](https://www.amazon.de/gp/product/B01H8XN7VK)

You can consider getting slotted-head screws for the M2 ones, if you're worried about damaging the tiny Allen screws.

### Dependencies

* [Arduino IDE](https://www.arduino.cc/en/main/software)

Board:

* [ESP32 by Espressif](https://github.com/espressif/arduino-esp32)

Libraries:

* [ArduinoJson by Benoit Blanchon](https://github.com/bblanchon/ArduinoJson)
* [CircularBuffer by AgileWare](https://github.com/rlogiacco/CircularBuffer)
* [TinyGPSPlus by Mikal Hart](https://github.com/mikalhart/TinyGPSPlus)
* [SSD1306 by ThingPulse](https://github.com/ThingPulse/esp8266-oled-ssd1306) 

### Install

* Clone this repository
* Install board
* Install dependencies
* Open `OpenBikeSensorFirmware.ino` in Arduino IDE
* Compile and upload to ESP32
* Connect sensor

A detailed description for Ubuntu can be found in the [install/Ubuntu.md](./install/Ubuntu.md).

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

## Acknowledgments

Inspiration, code snippets, etc.
* [Use of ArduinoJson Library](https://arduinojson.org/v6/example/config/)
* [Interface for OTA Web Updates by LastMinuteEngineers.com](https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/)
