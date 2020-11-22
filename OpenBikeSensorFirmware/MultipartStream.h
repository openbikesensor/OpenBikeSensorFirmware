//
// Created by andreas on 22.11.2020.
//

#ifndef OPENBIKESENSORFIRMWARE_MULTIPARTSTREAM_H
#define OPENBIKESENSORFIRMWARE_MULTIPARTSTREAM_H


#include <Stream.h>
#include <vector>
#include <HTTPClient.h>
#include <FS.h>

// STREAMS

class StringStream : public Stream {
public:
  StringStream(String str) {
    string = str;
  }
  int available() {
    return string.length() - pos;
  }
  int read() {
    const int result = peek();
    if (result != -1) {
      pos++;
    }
    return result;
  }
  int peek() {
    int result;
    if (pos >= string.length()) {
      result = -1;
    } else {
      result = string.charAt(pos);
    }
    return result;
  }

  void flush() {
  }

  size_t write(uint8_t) {
    return 0;
  }

//  String readStringUntil(char terminator);

  size_t readBytes(char *buffer, size_t length) {
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

private:
  String string;
  size_t pos = 0;
};

static StringStream EMPTY_STREAM("");

class StreamOfStreams : public Stream {
public:
  void push(Stream *addedStream) {
    streams.push_back(addedStream);
  }

  int available() {
    return getCurrent()->available();
  }
  int read() {
    return getCurrent()->read();
  }
  int peek() {
    return getCurrent()->peek();
  }

  void flush() {
  }

  size_t write(uint8_t) {
    return 0;
  }

  size_t readBytes(char *buffer, size_t length) {
    return getCurrent()->readBytes(buffer, length);
  }

private:
  Stream* current = nullptr;
  std::vector<Stream*> streams;
  size_t pos = 0;
  Stream* getCurrent() {
    // FIXME We assume available is 0 only at the very end!
    if (current == nullptr || current->available() == 0) {
      current = getNext();
    }
    return current;
  }
  Stream* getNext() {
    if (pos < streams.size()) {
      // TODO: there should be a better method in this vector
      log_d("About to switch to stream %d/%d", pos, streams.size());
      current = streams.at(pos++);
    } else {
      current = &EMPTY_STREAM;
    }
    return current;
  }
};


// Multipart

class MultipartData {
public:
  virtual String getHeaders() = 0;
  virtual Stream* asStream() = 0;
  virtual size_t length() = 0;

  MultipartData(String name, String contentType = "") {
    this->name = name;
    this->contentType = contentType;
  }

protected:
  String getBaseHeaders(String fileName = "") {
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
  String name;
  String contentType;
};

class MultipartDataString : public MultipartData {
public:

  MultipartDataString(String name, String content, String contentType = "")
      : MultipartData(name, contentType), contentStream(content) {
    this->content = content;
  }
  String getHeaders() override {
    return getBaseHeaders();
  }
  Stream* asStream() override {
    return &contentStream;
  }
  size_t length() override {
    return content.length();
  }

private:
  String content;
  StringStream contentStream;
};

class MultipartDataStream : public MultipartData {
public:
  String getHeaders() override {
    return getBaseHeaders(fileName);
  }
  Stream* asStream() override {
    return content;
  }
  size_t length() override {
    return content->size();
  }

  MultipartDataStream(String name, String fileName, File *content, String contentType = "")
  : MultipartData(name, contentType){
    this->fileName = fileName;
    this->content = content;
  }
private:
  String fileName;
  File *content;
};


class MultipartStream : public Stream {
public:
  MultipartStream(HTTPClient *client) {
    this->httpClient = client;
    boundary = "----" + String(rand(), 16) + String(rand(), 16);
    httpClient->addHeader("Content-Type",
                          "multipart/form-data; boundary=" + boundary);
  }
  ~MultipartStream() {
    for (const auto stream : myStreams) {
      delete stream;
    }
    myStreams.clear();
  }
  size_t predictSize() {
    return length;
  }
  void add(MultipartData &newData) {
    String preamble = "\r\n--" + boundary + "\r\n"
      + newData.getHeaders() + "\r\n";
    Stream* preambleStream = new StringStream(preamble);
    streams.push(preambleStream);
    myStreams.push_back(preambleStream);
    streams.push(newData.asStream());
    length += preamble.length();
    length += newData.length();
    log_d("Adding new part, size: %d", newData.length());
  }
  // API is bast - client must make sure to call last after the last add
  void last() {
    String conclusion = "\r\n--" + boundary + "--\r\n";
    Stream* conclusionStream = new StringStream(conclusion);
    streams.push(conclusionStream);
    myStreams.push_back(conclusionStream);
    length += conclusion.length();
  }
  int available() {
    return streams.available();
  }
  int read() {
    return streams.read();
  }
  int peek() {
    return streams.peek();
  }
  void flush() {
  }
  size_t write(uint8_t) {
    return 0;
  }
//  String readStringUntil(char terminator);
  size_t readBytes(char *buffer, size_t length) {
    return streams.readBytes(buffer, length);
  }

private:
  HTTPClient *httpClient;
  StreamOfStreams streams;
  std::vector<Stream*> myStreams;
  String boundary;
  size_t length = 0;
};


#endif //OPENBIKESENSORFIRMWARE_MULTIPARTSTREAM_H
