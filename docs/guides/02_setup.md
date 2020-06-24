# Setup your Development Environment

## [VSCode](https://code.visualstudio.com/)
<a name="vscode"></a>

1. Download and install [Visual Studio Code](https://code.visualstudio.com/). 
2. Open the `open-bike-sensor.code-workspace` in the project root. 
3. Install the recommended extensions (this might take a while, because [Platform.io]() gets installed) and restart VSCode when required.
3. Connect the ESP32
3. 
* Compile and upload to your ESP32
* Connect sensor


Alternatively you can also download the dependencies yourself and install it with the Arduino IDE (see below).


### Troubleshooting
* Can't upload to device  
You can specify the device port that VS Code should upload to. Duplicate the `custom_config.ini.example` file to `custom_config.ini` and set the `upload_port` there manually. If this option is not set, the upload port will be autodetected which can fail on some systems or might select the wrong device if other devices are plugged in.

* Compiling the code fails  
Use the `Clean` command and delete the `.pio` directory. Compiling the code again should work now. If any errors persist, please create a new issue!


## [CLion](https://www.jetbrains.com/de-de/clion/)
<a name="clion"></a>

1. Download [CLion](https://www.jetbrains.com/de-de/clion/). 

> TODO: Write guide for CLion

## Arduino IDE
<a name="arduino"></a>

* Install board
* Install dependencies
* Open `OpenBikeSensorFirmware.ino` in Arduino IDE
* Compile and upload to ESP32
* Connect sensor

There are detailed description for [Ubuntu](/docs/guides/02_setup_legacy/Ubuntu.md) and [ArchLinux](/docs/guides/02_setup_legacy/ArchLinux.md).


### Dependencies

Board:

* [ESP32 by Espressif](https://github.com/espressif/arduino-esp32)

Libraries:

* [ArduinoJson by Benoit Blanchon](https://github.com/bblanchon/ArduinoJson)
* [CircularBuffer by AgileWare](https://github.com/rlogiacco/CircularBuffer)
* [TinyGPSPlus by Mikal Hart](https://github.com/mikalhart/TinyGPSPlus)
* [SSD1306 by ThingPulse](https://github.com/ThingPulse/esp8266-oled-ssd1306) 

