/*
Copyright (C) 2019 Zweirat
Contact: https://openbikesensor.org

This file is part of the OpenBikeSensor project.

The OpenBikeSensor sensor firmware is free software: you can redistribute 
it and/or modify it under the terms of the GNU General Public License as 
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

The OpenBikeSensor sensor firmware is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General 
Public License for more details.

You should have received a copy of the GNU General Public License along with
the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

class HCSR04SensorInfo;

struct Config {
  uint8_t numSensors;
  Vector<HCSR04SensorInfo> sensorInfos;
  char ssid[32];
  char password[64];
  char hostname[64];
  int port;
};
