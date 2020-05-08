# Install on Arch Linux

## Clone this Repository

```bash
git clone https://github.com/Friends-of-OpenBikeSensor/OpenBikeSensorFirmware
```

## Arduino IDE

Run the folling commands as root to install the [Arduino IDE](https://www.arduino.cc/en/Main/Software) from the Arch Linux Community Repo:

```bash
pacman -S arduino
```

## pyserial

The **arduino-esp32** compiler needs `pyserial`, thus install it as root:

```bash
pacman -S python-pyserial
```

Otherwise, [you will get the following error message](https://github.com/espressif/arduino-esp32/issues/13):

> ImportError: No module named serial

## Dependencies
To install the required dependencies, run the follwoing bash script (located in OpenBikeSensorFirmware directory), which installs the dependencies directly from Github into `~/Arduino/libraries/`:

```bash
./install/install_dependencies.sh
```

## Arduino IDE

Start the Arduino IDE (binary is named "arduino").

### Install the ESP32-Board

* Open *File -> Preferences -> Additional Boards Manager URLs* and add:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
* Open *Tools -> Board -> Boards Manager* and search for ESP32
    * You should see esp32 by "Espressif Systems"
    * Install version 1.0.4
* Open *Tools -> Board* and select **ESP 32 Dev Module** if it is not already selected.

### Compile the Application

* Open the `OpenBikeSensorFirmware.ino`
* Open *Sketch -> Verify/Compile*
* You should het the message **Done compiling.** and a message like:

```bash
Sketch uses 894754 bytes (68%) of program storage space. Maximum is 1310720 bytes.
Global variables use 45168 bytes (13%) of dynamic memory, leaving 282512 bytes for local variables. Maximum is 327680 bytes.
```

### Upload to the ESP32 the NodeMCU

> TODO

### Connect Sensor

> TODO


## Soruces

* Arduino IDE
    * https://www.arduino.cc/en/Main/Software
    * https://www.arduino.cc/en/guide/linux
