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

1. Download via from https://fonts.google.com/specimen/Ubuntu
1. `fontcreator -s 8 Ubuntu-Regular.ttf -c iso-8859-15 -o ubuntu8.h -a " 0123456789.%°C"`
1. `fontcreator -s 10 Ubuntu-Regular.ttf -c iso-8859-15 -o ubuntu10.h`
1. `fontcreator -s 22 Ubuntu-Regular.ttf -c iso-8859-15 -o ubuntu22.h -a "0123456789-cm|kVs"`
1. `fontcreator -s 34 Ubuntu-Regular.ttf -c iso-8859-15 -o ubuntu34.h -a "0123456789-cm|kVs" -y 3`
1. `fontcreator -s 54 Ubuntu-Regular.ttf -c iso-8859-15 -o ubuntu58.h -a "0123456789" -y 5`


