# Format specification for the internal CVS format

In addition, the will be a json file per CSV file containing some metadata. 
The file needs to be specified later - still there is some data needed here - 
so I will list it.

## Metadata

OBSVersion
: v0.3.999

FormatVersion
: 2

DatasPerMeasurement
: 3 (fix as of now is `Tms<n>`, `Lus<n>` and `Rus<n>`)

MaximumMeasurementsPerLine
: 60 (currently fix)

HandlebarOffsetLeft
: as set in the configuration

HandlebarOffsetRight
: as set in the configuration

NumberOfDefinedPrivacyAreas
: as set in the configuration, just to be aware of

PrivacyLevelApplied
: NoPrivacy|NoPosition|OverridePrivacy|AbsolutePrivacy|

MaximumValidFlightTimeMicroseconds
: 18560 - all echo times above this value must be discarded and treated as no object in sight.

DistanceSensorsUsed
: `HC-SR04/JSN-SR04T` - enum currently only one possible value


## CSV

The CSV File must contain a header line with headline entries for each field, the 
names must be the same as given here. The number of data entries per line
can differ.

One exception, the headline of the 1st cell in not `Date` as the field 
description would indicate, it is used as Version indicator in the form 
`OBSVER<n>`. `<n>` is an integer which is increased with every change 
that would need an adoption of the parser this is `OBSVER2` so you can 
check for `OBSVER2` as magic at the file start.

If there is no value in a entry this means no measurement or not 
available or hidden. Before we used `-1` or `NaN` for this, now we are 
less polite and save the space.

There is typically one line per second, but there are possible exceptions:
- Timing might be bad, and we miss one second, use the `Millis` field if 
  you need more precise timings. 
- If we have multiple confirmed measurements in one interval in that case 
  an interval appears multiple times - once with each confirmed value. 
  Other fields are identical in both lines.  

NOTE: The order of the fields is different from Version 1 of the CVS file.

Headline    | Format | Range | Sample | Description |
---         | --- | --- | --- | --- |
`Date`      | TT.MM.YYYY | | 24.11.2020 | UTC, typically as received by the GPS module in that second. If there is no GPS module present, system time is used. If there was no reception of a time signal yet, this might be unix time (starting 1.1.1970) which can be at least used as offset between the csv lines.    
`Time`      | HH.MM.SS | | 12:00:00 | UTC time, see also above
`Millis`    | int32  | 0-2^31 | 1234567 | Millisecond counter will continuously increase in the file, for time offset calculatio
`Comment`   | char[] |  |  | Space to leave a text comment, limit to ascii 
`Latitude`  | double |  | 9.123456 | Latitude as degrees
`Longitude` | double |  | 42.123456 | Longitude in degrees
`Altitude`  | double | -9999.9-17999.9 | 480.12 | meters above mean sea level (GPGGA)
`Course`    | double | 0-359.9 | 42 | Course over ground in degrees (GPRMC)
`Speed`     | double | 0-359.9 | 42.0 | Speed over ground in km/h
`HDOP`      | double | 0-99.9 | 2.3  | Relative accuracy of horizontal position (GPGGA)
`Satellites` | int16 | 0-99 | 5 | Number of satellites in use (GPGGA)
`BatteryLevel` | double | 0-9.99 | 3.3 | Current battery level reading (~V)
`Left`      | int16  | 0-999 | 150 | Left minimum measured distance in centimeters of this line, the measurement is already corrected for the handlebar offset, 999 for no measurement. 
`Right`     | int16  | 0-999 | 150 | Right minimum measured distance as `Left` above.
`Confirmed` | int32  |  | 1234 | If !=0 the Measurement was confirmed overtaking by button press, contains the `Tms<n>` of the related measurement    
`Marked`    | char[]  | | "OVERTAKING" | Measurement was marked (not possible yet) with the given tag use <code>&#124;</code> to separate multiple tags is needed. 
`Invalid`   | int16  | 0-1 | 1 | Measurement was marked as invalid reading (not possible yet)
`insidePrivacyArea`| int16 | 0-1 | 1 | 
`Factor`    | int16  |  | 58 | The factor used to calculate the time given in micro seconds (us) into centimeters (cm). Currently fix, might get adjusted by temperature some time later. |
`Measurements` | int16  | 0-999 | 18 | Number of measurements entries in this line |
_comment_   | | | | Now follows a series of #`Measurements` repetitions of #`DatasPerMeasurement` entries, `<n>` is always increased starting from 1 for the 1st measurement. Order is always the same, additional data might be added to the end, `DatasPerMeasurement` will be increased then.  |
`Tms<n>`    | int16   | 0-1999 | 234 | Millisecond (ms) offset of measurement in this series (line) of measurements |
`Lus<n>`    | int32  | 0-100000 | 3456 | Microseconds (us) till the echo was received by the left sensor, divide by the `Factor` given above to get the distance in centimeters you might also want to apply the handlebar offset given in the metadata. Empty for no measurement taken. You might see values above 30000 which point to a timeout reported by the sensor when there is no object in sight.|
`Rus<n>`    | int32  | 0-100000 | 3456 | As `Lus<n>` above for the right sensor. |


Possible Header:

```csv
Version: 1;Time;Millis;Latitude;Longitude;Altitude; \
  Course;Speed;HDOP;Satellites;BatteryLevel;Left;Right;Confirmed;Marked;Invalid; \
  insidePrivacyArea;Factor;Measurements;Tms1;Lus1;Rus1;Tms2;Lus2;Rus2; \
  Tms3;Lus3;Rus3;...;Tms60;Lus60;Rus60
```
