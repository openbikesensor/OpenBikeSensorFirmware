//
// Created by Andreas on 19.02.2021.
//

#include "httpsServer.h"
// Includes for setting up the server
#include <HTTPSServer.hpp>

// Define the certificate data for the server (Certificate and private key)
#include <SSLCert.hpp>

// Includes to define request handler callbacks
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

// Required do define ResourceNodes
#include <ResourceNode.hpp>


using namespace httpsserver;
/*
// Create certificate data (see extras/README.md on how to create it)
SSLCert *cert;

// Setup the server with default configuration and the
// certificate that has been specified before
HTTPSServer *myServer;

void ObsHttpsServer::begin() {
  int createCertResult = createSelfSignedCert(
    *cert,
    KEYSIZE_2048,
    "CN=obs.local,O=acme,C=US");

  if (createCertResult != 0) {
    Serial.printf("Error generating certificate");
    return;
  }
}
*/