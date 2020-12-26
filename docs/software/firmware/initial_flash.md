# Initial ESP flashing

You can always use a IDE setup to flash the ESP but if you 
simply want to start using the OBS this way might be more 
straight forward. 

As of today this thing is foe Windows only.

Other than for the next updates that you can do over the 
air using the small release package you need the "full flash"
zip file and the _Flash Download Tools_ from
[ESPRESSIF](https://www.espressif.com/en/support/download/other-tools?keys=&field_type_tid%5B%5D=13)

## Requirements

Download the latest release archive from 
[OpenBikeSensorFirmware at GITHub](https://github.com/openbikesensor/OpenBikeSensorFirmware/releases). 
You need the larger ZIP file named `obs-v9.9.9999-full-flash.zip`.
Extract the files, they will all be named 0x??????.bin. The
numbers are the base address where the data should be flashed.
Don`t worry this will make sense in the next steps.

Please download _Flash Download Tools (ESP8266 & ESP32 & ESP32-S2)_ from
[ESPRESSIF](https://www.espressif.com/en/support/download/other-tools?keys=&field_type_tid%5B%5D=13)
and extract the tool in a dedicated folder. 


## Steps

Connect ESP via USB (checkme - driver needed?)

Start `flash_download_tool_3.X.X.exe`, give it some time to startup it will 
open a console window and eventually a simple UI. 

Choose `Developer Mode` - `ESP32 DownloadTool`. 

Enter the 4 files and the address. Check if the right com port is selected.

Press: "START".


