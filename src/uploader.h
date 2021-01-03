#ifndef UPLOADER_H
#define UPLOADER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <FS.h>

class Uploader {
  public:
    Uploader(String portalUrl, String userToken);
    /* uploads the named file to the portal,
     * moves it to uploaded directory and
     * returns true if successful, otherwise false */
    bool upload(const String& fileName);
    String getLastStatusMessage() const;
    String getLastLocation() const;

  private:
    const String mPortalUrl;
    const String mPortalUserToken;
    WiFiClientSecure mWiFiClient;
    String mLastLocation = "";
    String mLastStatusMessage = "NO UPLOAD";

    bool uploadFile(fs::File &file);
};
#endif
