#ifndef UPLOADER_H
#define UPLOADER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>

class Uploader {
  public:
    Uploader(String portalUrl, String userToken);
    static void setClock();
    /* uploads the named file to the portal,
     * moves it to uploaded directory and
     * returns true if successful, otherwise false */
    bool upload(const String& fileName);
    String getLastStatusMessage();
    String getLastLocation();

  private:
    const String mPortalUrl;
    const String mPortalUserToken;
    WiFiClientSecure mWiFiClient;
    String mLastLocation = "";
    String mLastStatusMessage = "NO UPLOAD";
};
#endif
