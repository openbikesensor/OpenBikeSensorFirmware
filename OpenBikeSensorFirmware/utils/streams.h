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
#ifndef OPENBIKESENSORFIRMWARE_STREAMS_H
#define OPENBIKESENSORFIRMWARE_STREAMS_H

#include <Stream.h>
#include <vector>
#include <FS.h>


class StringStream : public Stream {
public:
  explicit StringStream(String str);

  int available() override;

  int read() override;

  int peek() override;

  void flush() override;

  size_t write(uint8_t) override;

  size_t readBytes(char *buffer, size_t length) override;

private:
  String string;
  size_t pos = 0;
};

static StringStream EMPTY_STREAM("");

class StreamOfStreams : public Stream {
public:
  void push(Stream *addedStream);

  int available() override;

  int read() override;

  int peek() override;

  void flush() override;

  size_t write(uint8_t) override;

  size_t readBytes(char *buffer, size_t length) override;

private:
  Stream *current = nullptr;
  std::vector<Stream *> streams;
  size_t pos = 0;

  Stream *getCurrent();

  Stream *getNext();
};


#endif //OPENBIKESENSORFIRMWARE_STREAMS_H
