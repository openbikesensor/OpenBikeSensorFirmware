#ifndef UPLOADER_H
#define UPLOADER_H

#include <Arduino.h>

#include <WiFiClientSecure.h>

class uploader {
  public:
    static uploader *instance() {
      if (inst) {
        return inst;
      }
      return inst = new uploader();
    };

    void destroy();

    void setClock();

    bool upload(
      const String &fileName); // uploads a file, moves it to uploaded directory and returns true if successfull, otherwise false

  private:
    uploader();

    ~uploader();

    WiFiClientSecure *client;
    static uploader *inst;
};

#endif
