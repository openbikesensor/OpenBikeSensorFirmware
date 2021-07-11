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

1. Download via from https://fonts.google.com/specimen/Open+Sans
1. `fontcreator -s 8  -c iso-8859-15 -y 2 -o opensans8.h OpenSans-Regular.ttf`
1. `fontcreator -s 10 -c iso-8859-15 -y 2 -o opensans10.h OpenSans-Regular.ttf`
1. `fontcreator -s 20 -c iso-8859-15 -y 4 -o opensans20.h OpenSans-Regular.ttf`
1. `fontcreator -s 34 -c iso-8859-15 -y 7 -o opensans34.h OpenSans-Regular.ttf`
1. `fontcreator -s 50 -c iso-8859-15 -y 10 -o opensans50.h OpenSans-Regular.ttf -a "0123456789"`

