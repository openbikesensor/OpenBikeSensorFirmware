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

#ifndef OPENBIKESENSORFIRMWARE_STREAMS_H
#define OPENBIKESENSORFIRMWARE_STREAMS_H

#include <Stream.h>
#include <vector>
#include <FS.h>
#include <functional>


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

    void setProgressListener(std::function<void(size_t pos)>);

    size_t readBytes(char *buffer, size_t length) override;

  private:
    Stream *current = nullptr;
    std::vector<Stream *> streams;
    size_t pos = 0;
    std::function<void(size_t pos)> progressListener = nullptr;
    size_t overallBytePos = 0;


    Stream *getCurrent();

    Stream *getNext();
};


#endif //OPENBIKESENSORFIRMWARE_STREAMS_H
