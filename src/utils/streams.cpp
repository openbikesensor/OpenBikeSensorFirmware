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
#include "streams.h"

// StringStream

StringStream::StringStream(String str) {
  string = std::move(str);
}

int StringStream::available() {
  return string.length() - pos;
}

int StringStream::read() {
  const int result = peek();
  if (result != -1) {
    pos++;
  }
  return result;
}

int StringStream::peek() {
  int result;
  if (pos >= string.length()) {
    result = -1;
  } else {
    result = string.charAt(pos);
  }
  return result;
}

void StringStream::flush() {
}

size_t StringStream::write(uint8_t) {
  return 0;
}

size_t StringStream::readBytes(char *buffer, size_t length) {
  size_t count;
  if (length > available()) {
    count = available();
  } else {
    count = length;
  }
  strncpy(buffer, string.c_str(), count);
  pos += count;
  return count;
}

// StreamOfStreams

void StreamOfStreams::push(Stream *addedStream) {
  streams.push_back(addedStream);
}

int StreamOfStreams::available() {
  return getCurrent()->available();
}

int StreamOfStreams::read() {
  return getCurrent()->read();
}

int StreamOfStreams::peek() {
  return getCurrent()->peek();
}

void StreamOfStreams::flush() {
}

size_t StreamOfStreams::write(uint8_t) {
  return 0;
}

size_t StreamOfStreams::readBytes(char *buffer, size_t length) {
  return getCurrent()->readBytes(buffer, length);
}

Stream *StreamOfStreams::getCurrent() {
  if (current == nullptr || current->available() == 0) {
    current = getNext();
  }
  return current;
}

Stream *StreamOfStreams::getNext() {
  if (pos < streams.size()) {
    current = streams.at(pos++);
    log_d("About to switch to stream %d/%d", pos, streams.size());
  } else {
    current = &EMPTY_STREAM;
  }
  return current;
}

