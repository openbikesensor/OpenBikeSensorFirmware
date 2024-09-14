# Fonts

The `OpenSans-Regular_*` files in this directory were generated from
[`OpenSans-Regular.ttf`](https://fonts.google.com/specimen/Open+Sans) using the following bash statement:

``` bash
for i in 6 7 14 24 35; do
otf2bdf OpenSans-Regular.ttf -p $i > OpenSans-Regular_$i.bdf
u8g2/tools/font/bdfconv/bdfconv -f 1 -n OpenSans_Regular_$i -o OpenSans-Regular_$i.h OpenSans-Regular_$i.bdf
done
```
