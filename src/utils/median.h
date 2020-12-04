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
#ifndef OPENBIKESENSORFIRMWARE_MEDIAN_H
#define OPENBIKESENSORFIRMWARE_MEDIAN_H

template<typename T> class Median {

  public:
    /* Simple implementation, size must be odd! */
    explicit Median(size_t size):
      size{size},
      mid{size/2},
      data{new T[size]},
      temp{new T[size]},
      pos{0} {
      for(size_t i = 0; i < size; i++) {
        data[i] = MAX_SENSOR_VALUE;
      }
    };
    ~Median() {
      Serial.printf("Will free Median(%d/%d) -> 0x%lx\n", size, mid, data);
      delete[] data;
      delete[] temp;
    };
    void addValue(T value) {
      sorted = false;
      data[pos++] = value;
      if (pos >= size) {
        pos = 0;
      }
    };
    T median() {
      if (!sorted) {
        memcpy(temp, data, sizeof(T) * size); // std:copy needs to much mem
        std::sort(&temp[0], &temp[size]);
        sorted = true;
      }
      return temp[mid];
    };

  private:
    const size_t size;
    const size_t mid;
    T *data;
    T *temp;
    size_t pos = 0;
    boolean sorted = false;
};

#endif //OPENBIKESENSORFIRMWARE_MEDIAN_H
