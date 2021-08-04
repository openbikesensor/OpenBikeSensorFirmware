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

#include "button.h"
#include "esp32-hal-gpio.h"

Button::Button(int pin) : mPin(pin), mPreviousStateDurationMillis(0) {
  pinMode(pin, INPUT);
  mLastStateChangeMillis = mLastRawReadMillis = millis();
  mLastState = mLastRawState = digitalRead(pin);
}

void Button::handle() {
  handle(millis());
}

void Button::handle(unsigned long millis) {
  const int state = digitalRead(mPin);

  if (state != mLastRawState) {
    mLastRawReadMillis = millis;
    mLastRawState = state;
  }

  if (state != mLastState && millis - mLastRawReadMillis > DEBOUNCE_DELAY_MS) {
    mLastState = state;
    mPreviousStateDurationMillis = millis - mLastStateChangeMillis;
    mLastStateChangeMillis = millis;
    if (state == LOW) {
      // can distinguish long / short here if needed
      mReleaseEvents++;
    }
  }
}

bool Button::gotPressed() {
  if (mReleaseEvents > 0) {
    mReleaseEvents = 0;
    return true;
  } else {
    return false;
  }
}

int Button::read() const {
  // not debounced
  return digitalRead(mPin);
}

int Button::getState() const {
  // debounced, needs handle to be called
  return mLastState;
}

unsigned long Button::getCurrentStateMillis() const {
  return millis() - mLastStateChangeMillis;
}

unsigned long  Button::getPreviousStateMillis() const {
  return mPreviousStateDurationMillis;
}
