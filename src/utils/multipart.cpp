/*
 * Copyright (C) 2019-2021 OpenBikeSensor Contributors
 * Contact: https://openbikesensor.org
 *
 * This file is part of the OpenBikeSensor sensor firmware.
 *
 * The OpenBikeSensor sensor firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * OpenBikeSensor sensor firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the OpenBikeSensor sensor firmware.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "multipart.h"

MultipartData::MultipartData(String name, String contentType) {
  this->name = std::move(name);
  this->contentType = std::move(contentType);
}

String MultipartData::getBaseHeaders(const String& fileName) {
  String headers = "Content-Disposition: form-data; name=\"" + name + "\"";
  if (!fileName.isEmpty()) {
    headers += "; filename=\"" + fileName + "\"";
  }
  headers += "\r\n";
  if (!contentType.isEmpty()) {
    headers += "Content-Type: " + contentType + "\r\n";
  }
  return headers;
}


MultipartDataString::MultipartDataString(String name, const String& content, String contentType)
  : MultipartData(std::move(name), std::move(contentType)), contentStream(content) {
  this->content = content;
}

String MultipartDataString::getHeaders() {
  return getBaseHeaders();
}

Stream *MultipartDataString::asStream() {
  return &
    contentStream;
}

size_t MultipartDataString::length() {
  return content.length();

}

String MultipartDataStream::getHeaders() {
  return getBaseHeaders(fileName);
}

Stream *MultipartDataStream::asStream() {
  return content;
}

size_t MultipartDataStream::length() {
  return content->size();
}

MultipartDataStream::MultipartDataStream(String name, String fileName, File *content, String contentType)
  : MultipartData(std::move(name), std::move(contentType)) {
  this->fileName = std::move(fileName);
  this->content = content;
}


MultipartStream::MultipartStream(HTTPClient *client) {
  this->httpClient = client;
  boundary = "----" + String(esp_random(), 16) + String(esp_random(), 16);
  httpClient->addHeader("Content-Type",
    "multipart/form-data; boundary=" + boundary);
}

MultipartStream::~MultipartStream() {
  for (const auto stream : myStreams) {
    delete stream;
  }
  myStreams.clear();
}

size_t MultipartStream::predictSize() const {
  return length;
}

void MultipartStream::add(MultipartData &newData) {
  String preamble = "\r\n--" + boundary + "\r\n"
    + newData.getHeaders() + "\r\n";
  Stream *preambleStream = new StringStream(preamble);
  streams.push(preambleStream);
  myStreams.push_back(preambleStream);
  streams.push(newData.asStream());
  length += preamble.length();
  length += newData.length();
  log_d("Adding new part, size: %d", newData.length());
}

// API is bad - client must make sure to call last after the last add
void MultipartStream::last() {
  String conclusion = "\r\n--" + boundary + "--\r\n";
  Stream *conclusionStream = new StringStream(conclusion);
  streams.push(conclusionStream);
  myStreams.push_back(conclusionStream);
  length += conclusion.length();
}

int MultipartStream::available() {
  return streams.available();
}

int MultipartStream::read() {
  return streams.read();
}

int MultipartStream::peek() {
  return streams.peek();
}

void MultipartStream::flush() {
}

size_t MultipartStream::write(uint8_t) {
  return 0;
}

size_t MultipartStream::readBytes(char *buffer, size_t readLength) {
  return streams.readBytes(buffer, readLength);
}

void MultipartStream::setProgressListener(std::function<void(size_t pos)> listener) {
  streams.setProgressListener(listener);
}
