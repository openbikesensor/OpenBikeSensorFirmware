#ifndef UPLOADER_H
#define UPLOADER_H

#include <Arduino.h>
#ifdef HTTP_INSECURE
#include <WiFiClient.h>
#else
#include <WiFiClientSecure.h>
#endif
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
#ifdef HTTP_INSECURE
    WiFiClient mWiFiClient;
#else
    WiFiClientSecure mWiFiClient;
#endif
    String mLastLocation = "";
    String mLastStatusMessage = "NO UPLOAD";

    bool uploadFile(fs::File &file);
};
#endif
