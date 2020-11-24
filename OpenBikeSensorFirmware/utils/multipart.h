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
#ifndef OPENBIKESENSORFIRMWARE_MULTIPART_H
#define OPENBIKESENSORFIRMWARE_MULTIPART_H

#include <vector>
#include <HTTPClient.h>
#include "streams.h"

class MultipartData {
  public:
    virtual String getHeaders() = 0;

    virtual Stream *asStream() = 0;

    virtual size_t length() = 0;

    explicit MultipartData(String name, String contentType = "");

  protected:
    String getBaseHeaders(const String &fileName = "");

    String name;
    String contentType;
};

class MultipartDataString : public MultipartData {
  public:

    MultipartDataString(String name, const String &content, String contentType = "");

    String getHeaders() override;

    Stream *asStream() override;

    size_t length() override;

  private:
    String content;
    StringStream contentStream;
};

class MultipartDataStream : public MultipartData {
  public:
    String getHeaders() override;

    Stream *asStream() override;

    size_t length() override;

    MultipartDataStream(String name, String fileName, File *content, String contentType = "");

  private:
    String fileName;
    File *content;
};


class MultipartStream : public Stream {
  public:
    explicit MultipartStream(HTTPClient *client);

    ~MultipartStream() override;

    size_t predictSize() const;

    void add(MultipartData &newData);

    // API is bast - client must make sure to call last after the last add
    void last();

    int available() override;

    int read() override;

    int peek() override;

    void flush() override;

    size_t write(uint8_t) override;

    size_t readBytes(char *buffer, size_t length) override;

  private:
    HTTPClient *httpClient;
    StreamOfStreams streams;
    std::vector<Stream *> myStreams;
    String boundary;
    size_t length = 0;
};

#endif //OPENBIKESENSORFIRMWARE_MULTIPART_H
