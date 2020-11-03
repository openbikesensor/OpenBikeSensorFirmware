## Release Build

The github action creates a draft release for every build on the
master branch. You can publish any release from the master branch
as either "Pre-release" or final release upon publishing.

Idea is to delete intermediate releases after a while manually.

Version number major part is defined in 
`OpenBikeSensorFirmware/OpenBikeSensorFirmware.ino` as OBSVersion.
Just modify the static numeric part, the build script will take
care for the rest.

Format of the version follows https://semver.org the 3 segment
is automatically increased with every build by github or for
local builds is `-dev`. Also builds from a other branch than 
master get `-RC` as part of the version number.

A typical master branch version looks like `v1.2.432` a build from
any temporary branch is `v1.2-RC433`. Please also note that the numeric 
last part (patch) of github builds always increases and is never the 
same for 2 different builds. 