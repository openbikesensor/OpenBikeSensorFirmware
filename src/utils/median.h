/*
 * Copyright (C) 2019-2021 OpenBikeSensor Contributors
 * Contact: https://openbikesensor.org
 *
 * This file is part of the OpenBikeSensor firmware.
 *
 * The OpenBikeSensor firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * OpenBikeSensor firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the OpenBikeSensor firmware.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENBIKESENSORFIRMWARE_MEDIAN_H
#define OPENBIKESENSORFIRMWARE_MEDIAN_H

#include <algorithm>

template<typename T> class Median {

  public:
    /* Simple implementation, size must be odd! */
    Median(size_t size, T initialValue):
      size{size},
      mid{size/2},
      data{new T[size]},
      temp{new T[size]},
      pos{0} {
      for(size_t i = 0; i < size; i++) {
        data[i] = initialValue;
      }
    };
    ~Median() {
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
