#!/bin/bash

# Ensure, the folder Arduino/libraries exists into the home directory

mkdir -p ~/Arduino/libraries

# Configure the package versions

VERSION_ArduinoJson=v6.15.1
VERSION_CircularBuffer=1.3.3
VERSION_Ssd1306=4.1.0
VERSION_TinyGPSPlus=v1.02b
 
########################################################
echo "Install ArduinoJson"
########################################################

if [[ -d ~/Arduino/libraries/ArduinoJson ]]
then
    rm -rf ~/Arduino/libraries/ArduinoJson
fi

git clone --single-branch --branch ${VERSION_ArduinoJson} https://github.com/bblanchon/ArduinoJson.git ~/Arduino/libraries/ArduinoJson

########################################################
echo "Install CircularBuffer"
########################################################

if [[ -d ~/Arduino/libraries/CircularBuffer ]]
then
    rm -rf ~/Arduino/libraries/CircularBuffer
fi

git clone --single-branch --branch ${VERSION_CircularBuffer} https://github.com/rlogiacco/CircularBuffer.git ~/Arduino/libraries/CircularBuffer

########################################################
echo "Install esp8266-oled-ssd1306"
########################################################

if [[ -d ~/Arduino/libraries/esp8266-oled-ssd1306 ]]
then
    rm -rf ~/Arduino/libraries/esp8266-oled-ssd1306
fi

git clone --single-branch --branch ${VERSION_Ssd1306} https://github.com/ThingPulse/esp8266-oled-ssd1306.git ~/Arduino/libraries/esp8266-oled-ssd1306

########################################################
echo "Install TinyGPSPlus"
########################################################

if [[ -d ~/Arduino/libraries/TinyGPSPlus ]]
then
    rm -rf ~/Arduino/libraries/TinyGPSPlus
fi

git clone --single-branch --branch ${VERSION_TinyGPSPlus} https://github.com/mikalhart/TinyGPSPlus.git ~/Arduino/libraries/TinyGPSPlus

