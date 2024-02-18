# Fonts

Generated from `OpenSans-Regular.ttf` using the following fish-shell statement:

``` fish
for i in 6 7 15 26 37
otf2bdf OpenSans-Regular.ttf -p $i > OpenSans-Regular_$i.bdf
u8g2/tools/font/bdfconv/bdfconv -f 1 -n OpenSans_Regular_$i -o OpenSans-Regular_$i.h OpenSans-Regular_$i.bdf
end
```
