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

#ifndef OPENBIKESENSORFIRMWARE_BUTTON_H
#define OPENBIKESENSORFIRMWARE_BUTTON_H


class Button {
  public:
    explicit Button(int pin);
    bool handle();
    bool handle(unsigned long millis);
    int read() const;
    int getState() const;
    bool gotPressed();
    unsigned long getCurrentStateMillis() const;
    unsigned long getPreviousStateMillis() const;

  private:
    static const int DEBOUNCE_DELAY_MS = 50;
    const int mPin;
    unsigned long mLastRawReadMillis;
    int mLastRawState;
    int mLastState;
    unsigned long mLastStateChangeMillis;
    unsigned long mPreviousStateDurationMillis = 0;
    int mReleaseEvents = 0;
};


#endif //OPENBIKESENSORFIRMWARE_BUTTON_H
