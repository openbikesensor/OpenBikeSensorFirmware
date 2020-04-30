# OpenBikeSensor

Distance Measurement HC-SR04 ultrasonic sensors connected to ESP32

## Description

Inspired by the Berlin project Radmesser. This version uses a simple bush button to confirm distances measures were actually overtaking vehicles. It has its own GPS and SD card for logging, so it does not require additional hardware like a smartphone.

## Getting Started

### Hardware

The prototype by [Zweirat](https://zweirat-stuttgart.de/projekte/openbikesensor/) used the following:
* [ESP32](https://www.az-delivery.de/products/esp32-developmentboard)
* [LiFePo-Battery](https://www.akkuteile.de/lifepo-akkus/18650/a123-apr18650m-a1-1100mah-3-2v-3-3v-lifepo4-akku/a-1006861/)
* [Battery-Protection-Board](https://www.ebay.de/itm/202033076322)
* [LiFePo charging module](https://www.ebay.de/itm/MicroUSB-TP5000-3-6v-1A-Charger-Module-3-2v-LiFePO4-Lithium-Battery-Charging-/122164745507)
* [HC-SR04P](https://www.ebay.de/itm/183610614563)

With LiFePo Batteries you can power the device directly and avoid step-up/step-down losses that you have when using a powerbank. On the other hand you need the HC-SR04P instead of the HC-SR04 sensor if you don't have 5V power supply.

### Dependencies

* ESP32 device driver
* Arduino IDE
* [TM1637 Library](https://github.com/avishorp/TM1637) - installed through Arduino library manager
* [SSD1306 Library](https://github.com/adafruit/Adafruit_SSD1306) - installed through Arduino library manager
* [TinyGPSPlus Library](https://github.com/mikalhart/TinyGPSPlus)

### Installing

* clone or download repository
* open in Arduino IDE
* compile and upload to ESP32
* Connect sensor

### Use
* power up device
* push the button each time there is a value shown in the display and you want to confirm it was a vehicle overtaking you
* power down

### Configuration
Most of the configuration is a stub and needs further development. But uploading new firmware works. You can enter the configuration mode by pushing the button while turning the device on. Then it will open a unique WiFi including the devices Macadress named "OpenBikeSensor-xxxxxxxxxxxx". Initial password is "12345678". The configuration page can be found on "http://openbikesensor.local" or "172.20.0.1". You can directly upload a precompiled binary.

## Acknowledgments

Inspiration, code snippets, etc.
* [Use of ArduinoJson Library](https://arduinojson.org/v6/example/config/)
* [Interface for OTA Web Updates by LastMinuteEngineers.com](https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/)
