# Fonts

## Preparation

1. https://github.com/arcao/esp8266-oled-ssd1306-font-creator
````
    git clone https://github.com/arcao/esp8266-oled-ssd1306-font-creator.git
    cd esp8266-oled-ssd1306-font-creator/
    ./gradlew
```` 
Needed some tweaks (https://github.com/amandel/esp8266-oled-ssd1306-font-creator) PR 
is currently open. (https://github.com/arcao/esp8266-oled-ssd1306-font-creator/pull/4)


## Generating

1. Download via from https://01.org/clear-sans https://01.org/sites/default/files/downloads/clear-sans/clearsans-1.00.zip
1. `fontcreator -s 8 ClearSans-Regular.ttf -c iso-8859-15 -o clearsans8.h -a "0123456789.%°C"` 
1. `fontcreator -s 10 ClearSans-Regular.ttf -c iso-8859-15 -o clearsans10.h`
1. `fontcreator -s 20 ClearSans-Regular.ttf -c iso-8859-15 -o clearsans20.h -a "0123456789-cm|kV"`
1. `fontcreator -s 26 ClearSans-Regular.ttf -c iso-8859-15 -o clearsans26.h -a "0123456789-cm|kV"`
1. `fontcreator -s 50 ClearSans-Regular.ttf -c iso-8859-15 -o clearsans50.h -a "0123456789"`


