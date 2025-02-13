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

#include "gps.h"
#include "utils/timeutils.h"

/* Most input from u-blox6_ReceiverDescrProtSpec_(GPS.G6-SW-10018)_Public.pdf */

const String Gps::INF_SEVERITY_STRING[] = {
  String("ERR"), String("WRN"), String("NTC"),
  String("TST"), String("DBG")
};

void Gps::begin() {
  setBaud();
  softResetGps();
  if (mGpsNeedsConfigUpdate) {
    configureGpsModule();
  }
  enableAlpIfDataIsAvailable();
  pollStatistics();
  if (mLastTimeTimeSet == 0) {
#ifdef UBX_M10
    setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_TIMEGPS_UART1, 1);
#endif
#ifdef UBX_M6
    setMessageInterval(UBX_MSG::NAV_TIMEGPS, 1);
#endif
  } else {
#ifdef UBX_M10
    setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_TIMEGPS_UART1, 240);
#endif
#ifdef UBX_M6
    setMessageInterval(UBX_MSG::NAV_TIMEGPS, 240);
#endif
  }

  //Serial.updateBaudRate(9600);
  //mSerial.begin(9600, SERIAL_8N1);
  // Debug - forward gps serial to normal serial
  /*
  log_e("--------_STARTING LOOP");
  while(1)
  {
    int val = mSerial.read();
    if(val >= 0)
      Serial.write(val);
    val = Serial.read();
    if(val >= 0)
      mSerial.write(val);
  }*/
}

void Gps::sendUbx(UBX_MSG ubxMsgId, const uint8_t payload[], uint16_t length) {
  sendUbx(static_cast<uint16_t>(ubxMsgId), payload, length);
}

void Gps::sendUbx(uint16_t ubxMsgId, const uint8_t payload[], uint16_t length) {
  // We copy over all in one go, assume to be more efficient than byte by byte to serial
  uint8_t buffer[length + 8];
  uint8_t chkA = 0;
  uint8_t chkB = 0;

  buffer[0] = 0xb5;
  buffer[1] = 0x62;
  buffer[2] = ubxMsgId & 0xFFU;
  chkA += buffer[2]; chkB += chkA;
  buffer[3] = ubxMsgId >> 8;
  chkA += buffer[3]; chkB += chkA;
  buffer[4] = length & 0xFFU;
  chkA += buffer[4]; chkB += chkA;
  buffer[5] = length >> 8;
  chkA += buffer[5]; chkB += chkA;
  for (int i = 0; i < length; i++) {
    const uint8_t data = payload[i];
    buffer[i + 6] = data;
    chkA += data; chkB += chkA;
  }
  buffer[6 + length] = chkA;
  buffer[7 + length] = chkB;
  mSerial.write(buffer, length + 8);
}

#ifdef UBX_M10
// Send a CFG_VALSET ubx command to write a configuration
// Only available in M10 devices
void Gps::sendUbxCfg(enum UBX_CFG_LAYER layer, UBX_CFG_KEY_ID keyId, uint32_t value)
{
  uint8_t ubxCfgMsg[] = {0x00, layer, 0x00, 0x00,
    (uint8_t)((uint32_t)keyId), (uint8_t)(((uint32_t)keyId)>>8), (uint8_t)(((uint32_t)keyId)>>16), (uint8_t)(((uint32_t)keyId)>>24),
    (uint8_t)((uint32_t)value), (uint8_t)(((uint32_t)value)>>8), (uint8_t)(((uint32_t)value)>>16), (uint8_t)(((uint32_t)value)>>24)};
  uint16_t length = 0;
  uint8_t lengthId = (((uint32_t)keyId)>>28)&0x07;
  if(lengthId == 0x01 || lengthId == 0x02)
    length = 1;
  else if(lengthId == 0x03)
    length = 2;
  else if(lengthId == 0x04)
    length = 4;
  sendUbx(UBX_MSG::CFG_VALSET, ubxCfgMsg, length + 8);
}
#endif

/* Method sends the message in the receive buffer! */
void Gps::sendUbxDirect() {
  const uint16_t length = mGpsBuffer.ubxHeader.length;
  uint8_t chkA = 0;
  uint8_t chkB = 0;
  for (int i = 2; i < length + 6; i++) {
    chkA += mGpsBuffer.u1Data[i]; chkB += chkA;
  }
  mGpsBuffer.u1Data[length + 6] = chkA;
  mGpsBuffer.u1Data[length + 7] = chkB;
  const size_t didSend = mSerial.write(mGpsBuffer.u1Data, length + 8);
  if (didSend != (length + 8)) {
    log_e("GPS, did send: %d expected %d", didSend, length + 8);
  }
}

/* Resets the stored GPS config and stores our config! */
void Gps::configureGpsModule() {
  // The CFG_CFG_CLR caused trouble on newer modules - connection settings get reset.
  // no CFG_CFG_CLR for now if this fails we might need more fine-grained
  // reset
  // const uint8_t UBX_CFG_CFG_CLR[] = {
  //   0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  //   0xFE, 0xFF, 0x00, 0x00, 0x03
  // };
  log_i("Start GPS CFG");
#ifdef UBX_M6
  // INF messages via UBX only
  const uint8_t UBX_CFG_INF_UBX[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xef, 0x00, 0x00, 0x00, 0x00,
  };
  sendAndWaitForAck(UBX_MSG::CFG_INF, UBX_CFG_INF_UBX, sizeof(UBX_CFG_INF_UBX));

  // INF messages via UBX only
  const uint8_t UBX_CFG_INF_NMEA[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  sendAndWaitForAck(UBX_MSG::CFG_INF, UBX_CFG_INF_NMEA, sizeof(UBX_CFG_INF_NMEA));
#endif

#ifdef UBX_M6
  // Only required for M6 modules, NMEA is globally deactivated for M10 during begin()
  setMessageInterval(UBX_MSG::NMEA_GGA, 0);
  setMessageInterval(UBX_MSG::NMEA_RMC, 0);
  setMessageInterval(UBX_MSG::NMEA_GLL, 0);
  setMessageInterval(UBX_MSG::NMEA_GSA, 0);
  setMessageInterval(UBX_MSG::NMEA_GSV, 0);
  setMessageInterval(UBX_MSG::NMEA_VTG, 0);
#endif
  setStatisticsIntervalInSeconds(0);
#ifdef UBX_M6
  // AID is not available on M10 modules? At least I could not find it
  setMessageInterval(UBX_MSG::AID_ALPSRV, 0);
#endif
  enableSbas();

#ifdef UBX_M10
  // Enable Glonass for M10, GPS and Galileo are enabled by default
  sendCfgAndWaitForAck(UBX_CFG_LAYER::RAM, UBX_CFG_KEY_ID::CFG_SIGNAL_GLO_ENA, 1);
#endif

#ifdef UBX_M6
  setMessageInterval(UBX_MSG::NAV_SBAS, 59);
  setMessageInterval(UBX_MSG::NAV_POSLLH, 1);
  setMessageInterval(UBX_MSG::NAV_DOP, 1);
  setMessageInterval(UBX_MSG::NAV_SOL, 1);
  setMessageInterval(UBX_MSG::NAV_VELNED, 1);
  setMessageInterval(UBX_MSG::NAV_TIMEGPS, 1);
#endif
#ifdef UBX_M10
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_SBAS_UART1, 59);
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_POSLLH_UART1, 1);
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_DOP_UART1, 1);
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_VELNED_UART1, 1);
  // SOL not available in M10, the message NAV-PVT is used instead
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_PVT_UART1, 1);
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_TIMEGPS_UART1, 1);
#endif

#ifdef UBX_M6
  // "dynModel" - "3: pedestrian"
  // "static hold" - 80cm/s == 2.88 km/h
  // "staticHoldMaxDist" - 20m (not supported by our GPS receivers)
  const uint8_t UBX_CFG_NAV5[] = {
    0xff, 0xff, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xfa, 0x00,
    0xfa, 0x00, 0x64, 0x00, 0x2c, 0x01, 0x50, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  sendAndWaitForAck(UBX_MSG::CFG_NAV5, UBX_CFG_NAV5, sizeof(UBX_CFG_NAV5));
  // on m8n enable all nav systems but baidou - baidou fails when used together with galileo
  const uint8_t UBX_CFG_GNSS[] = {
      0x00, 0x20, 0x20, 0x07, 0x00, 0x08, 0x10, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x01, 0x03,
      0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 0x04, 0x08, 0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0x08,
      0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05,
      0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x06, 0x08, 0x0e, 0x00, 0x01, 0x00, 0x01, 0x00
  } ;
  bool success = sendAndWaitForAck(UBX_MSG::CFG_GNSS, UBX_CFG_GNSS, sizeof(UBX_CFG_GNSS));
  if (success) {
    addStatisticsMessage("Successfully set GNSS");
  } 
  else {
    addStatisticsMessage("GNSS not configured, likely older GPS");
  }
#endif
#ifdef UBX_M10
  sendCfgAndWaitForAck(UBX_CFG_LAYER::RAM, UBX_CFG_KEY_ID::CFG_NAVSPG_DYNMODEL, 4);  // Set dynamic model to 4 (automotive)
#endif

#ifdef UBX_M6
  // Not used for M10, as OBSPro does not have a GPS LED
  // "timepulse" - affecting the led, switching to 10ms pulse every 10sec,
  //   to be clearly differentiable from the default (100ms each second)
  const uint8_t UBX_CFG_TP[] = {
    0x80, 0x96, 0x98, 0x00, 0x10, 0x27, 0x00, 0x00,
    0x01, 0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  success = sendAndWaitForAck(UBX_MSG::CFG_TP, UBX_CFG_TP, sizeof(UBX_CFG_TP));
  // newer gps (m8n) use tp5 for timepulse config
  if (!success) {
    const uint8_t UBX_CFG_TP5[] = { 0x00, 0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x40, 0x42, 0x0f, 0x00, 0x40, 0x42, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00};
    if (!sendAndWaitForAck(UBX_MSG::CFG_TP5, UBX_CFG_TP5, sizeof(UBX_CFG_TP5))) {
      addStatisticsMessage("Successfully set timepulse for newer gps");
    } else {
      addStatisticsMessage("No ack for setting timepulse in either new or old format");
    }
  }
#endif

#ifdef UBX_M6
  // What is this good for? Not bothering implementing that for OBSPro
  // Leave some info in the GPS module
  String inv = String("\x01openbikesensor.org/") + OBSVersion;
  sendAndWaitForAck(UBX_MSG::CFG_RINV, reinterpret_cast<const uint8_t *>(inv.c_str()),
                    inv.length());
#endif

#ifdef UBX_M6
  // AID only available in M6 modules
  // Used to store GPS AID data every 3 minutes+
  setMessageInterval(UBX_MSG::AID_INI, 185);
#endif

  // Persist configuration
#ifdef UBX_M6
  const uint8_t UBX_CFG_CFG_SAVE[] = {
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x17
  };
#endif
#ifdef UBX_M10
  const uint8_t UBX_CFG_CFG_SAVE[] = {
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x03
  };
#endif
  // write cfg takes longer.
  if (sendAndWaitForAck(UBX_MSG::CFG_CFG, UBX_CFG_CFG_SAVE, sizeof(UBX_CFG_CFG_SAVE), 3000)) {
    addStatisticsMessage("OBS: Did update GPS settings.");
  } else {
    addStatisticsMessage("OBS: Failed to save updated GPS settings.");
  }
  handle(20);
  log_i("Config GPS done!");
}

void Gps::softResetGps() {
  log_i("Soft-RESET GPS!");
  handle();
  const uint8_t UBX_CFG_RST[] = {0x00, 0x00, 0x02, 0x00}; // WARM START
//  const uint8_t UBX_CFG_RST[] = {0xFF, 0xFF, 0x02, 0x00}; // Cold START
  // we had the case where the reset took several seconds
  // see https://github.com/openbikesensor/OpenBikeSensorFirmware/issues/309
  // Newer firmware (like M10 and likely also M8) will not ack this
  // message so we do not wait for the ACK
  sendUbx(UBX_MSG::CFG_RST, UBX_CFG_RST, 4);
  waitForData(1000);
  handle();
  log_i("Soft-RESET GPS! Done");
}

void Gps::coldResetGps() {
  log_i("Cold START GPS!");
  handle();
  const uint8_t UBX_CFG_RST[] = {0xFF, 0xFF, 0x04, 0x00}; // Cold START, 0x04 = Hardware reset (watchdog) after shutdown
  // we had the case where the reset took several seconds
  // see https://github.com/openbikesensor/OpenBikeSensorFirmware/issues/309
  // Newer firmware (like M10 and likely also M8) will not ack this
  // message so we do not wait for the ACK
  sendUbx(UBX_MSG::CFG_RST, UBX_CFG_RST, 4);
  waitForData(1000);
  handle();
  log_i("Cold START GPS! Done");
}

/* There had been changes for the satellites used for SBAS
 * in europe since the firmware of our GPS module was built
 * we configure the module to use the 2 satellites that are
 * in productive use as of 2021. This needs to be changed
 * if used outside europa or might be in the future.
 * We use EGNOS sats 123, 136 as of 2021 see
 * https://de.wikipedia.org/wiki/European_Geostationary_Navigation_Overlay_Service
 */
void Gps::enableSbas() {// Enable SBAS subsystem!
  const uint8_t UBX_CFG_SBAS[] = {
    0x01, // ON
    0x03, // Ranging | Correction | !integrity (like default)
    0x02, // max 2 sats, default is 3
    // NEO8-default:  120, 123, 127-129, 133, 135-138
    // NEO8-egnos: 120, 123-124, 126, 131
    // NEO6-egnos: 120, 123-124, 126, 131
    // Wikipedia-egnos: 123, 136 (Test: 120) aus: 124 & 126
    0x00, 0x08, 0x00, 0x01, 0x00 //
  };
  sendAndWaitForAck(UBX_MSG::CFG_SBAS, UBX_CFG_SBAS, sizeof(UBX_CFG_SBAS));
  sendAndWaitForAck(UBX_MSG::CFG_SBAS); // get setting once
}

void Gps::enableAlpIfDataIsAvailable() {
#ifdef UBX_M6
  if (AlpData::available()) {
    log_i("Enable ALP");
    // don't wait for ack - newer modules do not support this old ALPSRV
    setMessageInterval(UBX_MSG::AID_ALPSRV, 1, false);
    handle(100);
  } else {
    log_w("Disable ALP - no data!");
    setMessageInterval(UBX_MSG::AID_ALPSRV, 0);
  }
#endif
}

/* Poll or refresh one time statistics, also spends some time
 * to collect the results.
 */
void Gps::pollStatistics() {
  handle();
  sendUbx(UBX_MSG::AID_ALP);
  handle();
  sendUbx(UBX_MSG::MON_VER);
  handle();
  sendUbx(UBX_MSG::MON_HW);
  handle();
  sendUbx(UBX_MSG::NAV_STATUS);
  handle();
  sendAndWaitForAck(UBX_MSG::CFG_NAV5);
  handle();
  sendAndWaitForAck(UBX_MSG::CFG_RINV);
  handle(40);
}

/* Prefer to subscribe to messages rather than polling. This lets
 * the GPS module decide when to send the informative, none essential
 * messages.
 * Zero seconds means never.
 */
void Gps::setStatisticsIntervalInSeconds(uint16_t seconds) {
#ifdef UBX_M6
  setMessageInterval(UBX_MSG::NAV_STATUS, seconds);
  setMessageInterval(UBX_MSG::MON_HW, seconds);
#endif
#ifdef UBX_M10
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_STATUS_UART1, seconds);
  setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_MON_HW_UART1, seconds);
#endif
}

#ifdef UBX_M6
bool Gps::setMessageInterval(UBX_MSG msgId, uint8_t seconds, bool waitForAck) {
  uint8_t ubxCfgMsg[] = {
    0x0A, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  ubxCfgMsg[0] = ((uint16_t) msgId) & 0xFFU;
  ubxCfgMsg[1] = ((uint16_t) msgId) >> 8U;
  ubxCfgMsg[3] = seconds;
  bool result;
  if (waitForAck) {
    result = sendAndWaitForAck(UBX_MSG::CFG_MSG, ubxCfgMsg, sizeof(ubxCfgMsg));
  } else {
    result = true;
    sendUbx(UBX_MSG::CFG_MSG, ubxCfgMsg, sizeof(ubxCfgMsg));
  }
  if (!result) {
    log_e("Failed for CFG_MSG of 0x%04x!", msgId);
  }
  return result;
}
#endif

#ifdef UBX_M10
bool Gps::setMessageInterval(UBX_CFG_KEY_ID msgId, uint8_t seconds, bool waitForAck) {
  bool result;
  if (waitForAck) {
    result = sendCfgAndWaitForAck(UBX_CFG_LAYER::RAM, msgId, seconds);
  } else {
    result = true;
    sendUbxCfg(UBX_CFG_LAYER::RAM, msgId, seconds);
  }
  if (!result) {
    log_e("Failed for sendUbxCfg of 0x%08x!", msgId);
  }
  return result;
}
#endif

/* Initialize the GPS module to talk 115200.
 * If 115200 is not on by default the module is also configured,
 * otherwise it is assumed that we have already configured the module.
 */
bool Gps::setBaud() {
  mSerial.end();
  mSerial.setRxBufferSize(512);

  mSerial.begin(115200, SERIAL_8N1);
  while(mSerial.read() >= 0) ;

  if (checkCommunication()) {
    log_i("GPS startup already 115200");
    return true;
  }

  mSerial.updateBaudRate(9600);

  // switch to 115200 "blind", switch off NMEA
#ifdef UBX_M6
  const uint8_t UBX_CFG_PRT[] = {
    0x01, 0x00, 0x00, 0x00, 0xd0, 0x08, 0x00, 0x00,
    0x00, 0xc2, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  sendUbx(UBX_MSG::CFG_PRT, UBX_CFG_PRT, sizeof(UBX_CFG_PRT));
#endif
#ifdef UBX_M10
  sendCfgAndWaitForAck(UBX_CFG_LAYER::RAM, UBX_CFG_KEY_ID::CFG_UART1OUTPROT_NMEA, 0);  // Disable NMEA
  sendUbxCfg(UBX_CFG_LAYER::RAM, UBX_CFG_KEY_ID::CFG_UART1_BAUDRATE, 115200);  // Blindly set baudrate
  log_i("Blindly switched to 115200 baud rate");
#endif
  mSerial.flush();
  mSerial.updateBaudRate(115200);

  // check if connected:
  bool connected = checkCommunication();
  if (!connected) {
    log_e("Switch to 115200 was not possible, back to 9600.");
    mSerial.updateBaudRate(9600);
    connected = checkCommunication();
  }
  if (!connected) {
    log_e("NO GPS????");
    delay(5000);
  }
  else
    log_i("Connected to GPS");
  if (connected && mSerial.baudRate() / 10 != 115200 / 10) {
    log_e("Reported rate: %d", mSerial.baudRate());
#ifdef UBX_M6
    if (sendAndWaitForAck(UBX_MSG::CFG_PRT, UBX_CFG_PRT, sizeof(UBX_CFG_PRT))) {
      mSerial.updateBaudRate(115200);
    }
#endif
  }
  if (checkCommunication()) {
    configureGpsModule();
  }
  return checkCommunication();
}

/* Send a "ping" message to the gps module and check,
 * if we get a response. */
bool Gps::checkCommunication() {
  return sendAndWaitForAck(UBX_MSG::CFG_RINV) || mAckReceived || mNakReceived;
}

bool Gps::sendAndWaitForAck(UBX_MSG ubxMsgId, const uint8_t *buffer, size_t size, const uint16_t timeoutMs) {
  const int tries = 2;
  auto start = millis();

  bool result = false;
  for (int i = 0; i < tries; i++) {
    handle();
    auto const loopStart = millis();
    mNakReceived = false;
    mAckReceived = false;
    mLastAckMsgId = 0;

    sendUbx(ubxMsgId, buffer, size);
    mSerial.flush();
    handle();
    while (mLastAckMsgId != (uint16_t) ubxMsgId
      && millis() - loopStart < timeoutMs) {
      if (!handle()) {
        delay(1);
      }
    }
    if (mAckReceived || mNakReceived) {
      result = mAckReceived;
      break;
    }
    log_w("Retry to send 0x%04x after %dms.", ubxMsgId, millis() - loopStart);
  }
  if (result) {
    log_d("Success in sending. 0x%04x took %dms", ubxMsgId, millis() - start);
  } else {
    log_e("Failed to send. 0x%04x NAK: %d after %dms", ubxMsgId, mNakReceived, millis() - start);
  }
  return result;
}

#ifdef UBX_M10
bool Gps::sendCfgAndWaitForAck(enum UBX_CFG_LAYER layer, UBX_CFG_KEY_ID keyId, uint32_t value, const uint16_t timeoutMs) {
  const int tries = 2;
  auto start = millis();

  bool result = false;
  for (int i = 0; i < tries; i++) {
    handle();
    auto const loopStart = millis();
    mNakReceived = false;
    mAckReceived = false;
    mLastAckMsgId = 0;

    sendUbxCfg(layer, keyId, value);
    mSerial.flush();
    handle();
    while (mLastAckMsgId != (uint16_t) UBX_MSG::CFG_VALSET && millis() - loopStart < timeoutMs) {
      if (!handle()) {
        delay(1);
      }
    }
    if (mAckReceived || mNakReceived) {
      result = mAckReceived;
      break;
    }
    log_w("Retry to send cfg 0x%08x after %dms.", keyId, millis() - loopStart);
  }
  if (result) {
    log_d("Success in sending cfg. 0x%08x took %dms", keyId, millis() - start);
  } else {
    log_e("Failed to send cfg. 0x%08x NAK: %d after %dms", keyId, mNakReceived, millis() - start);
  }
  return result;
}
#endif


/* Will delay for the given number of ms and handle GPS if needed. */
void Gps::handle(uint32_t milliSeconds) {
  const auto end = millis() + milliSeconds;
  while (end > millis()) {
    if (!handle()) {
      delay(1);
    }
  }
}

bool Gps::handle() {
  auto handleStart = millis();
  auto messageStarted = mMessageStarted;
  auto lastCallDelayMs = handleStart - messageStarted;
  const auto bytesAvailable = mSerial.available();

  if (bytesAvailable > 0 && lastCallDelayMs > 600) { // we would get only old data
    addStatisticsMessage(String("readGPSData(clear: ") + String(bytesAvailable)
                         + " bytes in buffer, lastCall " + String(lastCallDelayMs)
                         + "ms ago, at " + TimeUtils::dateTimeToString() + ")");
    mMessageStarted = handleStart;
    for (int i = 0; i < bytesAvailable; i++) {
      mSerial.read();
    }
    handleStart = millis();
  } else if (bytesAvailable > 250) {
    addStatisticsMessage(String("readGPSData(av: ") + String(bytesAvailable)
                         + " bytes in buffer, lastCall " + String(lastCallDelayMs)
                         + "ms ago, at " + TimeUtils::dateTimeToString() + ")");
  }
  boolean gotGpsData = false;
  int bytesProcessed = 0;
  int data;
#ifdef GPS_TRANSPARENT_UART
  // Redicrect data to GPS
  while((data = Serial.read()) >= 0)
    mSerial.write(data);
#endif
  while ((data = mSerial.read()) >= 0) {
    bytesProcessed++;

#ifdef GPS_TRANSPARENT_UART
    // Redicrect data from GPS
    Serial.write(data);
#endif

#ifdef GPS_LOW_LEVEL_DEBUGGING
    log_w("GPS in: 0x%02x", data);
#endif
    if (encode(data)) {
      gotGpsData = true;
      if (bytesProcessed > 2048) {
        log_e("break after %d bytes", bytesProcessed);
        break;
      }
    }
  }
  auto between = handleStart - messageStarted;
  // ~150ms is needed if we write to SD
  if (between > 160 && bytesProcessed > 0 || bytesProcessed > 1024) {
    addStatisticsMessage(String("readGPSData(longDelay: ") + String(between)
      + "ms received " + String(bytesProcessed) + "(" + String(bytesAvailable) + ") in " + String(millis() - handleStart) + "ms)");
  }
  if (mSerial.available() == 0) {
    mMessageStarted = millis(); // buffer empty next message might already start now
  }
  // TODO: Add dead device detection re-init if no data for 60 seconds...
  return gotGpsData;
}

bool Gps::waitForData(const uint16_t timeoutMs) {
  if (mSerial.available() > 0) {
    return true;
  }
  auto end = millis() + timeoutMs;
  while (mSerial.available() <= 0 && millis() < end) {
    delay(1);
  }
  return mSerial.available() > 0;
}

static const String STATIC_MSG_PREFIX[] = {
  "DBG: San Vel ", "DBG: San Pos ", "DBG: San Alt ", "NTC: ANTSTATUS=", "readGPSData", "TIMEGPS set:"
};

void Gps::addStatisticsMessage(String newMessage) {
  int prefix = -1;
  for (int i = 0; i < (sizeof(STATIC_MSG_PREFIX)/sizeof(*STATIC_MSG_PREFIX)); i++) {
    if (newMessage.startsWith(STATIC_MSG_PREFIX[i])) {
      prefix = i;
    }
  }
  for (int i = 0; i < mMessages.size(); i++) {
    if (mMessages[i] == newMessage) {
      newMessage.clear();
      break;
    }
    if (prefix >= 0 && mMessages[i].startsWith(STATIC_MSG_PREFIX[prefix])) {
      log_i("Update: %s", newMessage.c_str());
      mMessages[i] = newMessage;
      newMessage.clear();
      break;
    }
  }
  if (!newMessage.isEmpty()) {
    mMessages.push_back(newMessage);
    log_i("New: %s", newMessage.c_str());
  }
  newMessage.clear();
  if (mMessages.size() > 20) {
    mMessages.erase(mMessages.cbegin());
  }
}

bool Gps::isInsidePrivacyArea() {
  // quite accurate haversine formula
  // consider using simplified flat earth calculation to save time

  // TODO: Config must not be read from the globals here!
  const double lon = mCurrentGpsRecord.getLongitude();
  const double lat = mCurrentGpsRecord.getLatitude();

  for (auto pa : config.privacyAreas) {
    double distance = haversine(
      lat, lon, pa.transformedLatitude, pa.transformedLongitude);
    if (distance < pa.radius) {
      return true;
    }
  }
  return false;
}

double Gps::haversine(double lat1, double lon1, double lat2, double lon2) {
  // https://www.geeksforgeeks.org/haversine-formula-to-find-distance-between-two-points-on-a-sphere/
  // distance between latitudes and longitudes
  double dLat = (lat2 - lat1) * M_PI / 180.0;
  double dLon = (lon2 - lon1) * M_PI / 180.0;

  // convert to radians
  lat1 = (lat1) * M_PI / 180.0;
  lat2 = (lat2) * M_PI / 180.0;

  // apply formulae
  double a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
  double rad = 6371000;
  double c = 2 * asin(sqrt(a));
  return rad * c;
}

void Gps::randomOffset(PrivacyArea &p) {
  randomSeed(analogRead(0));
  // Offset in degree and distance
  int offsetAngle = random(0, 360);
  int offsetDistance = random(p.radius / 10.0, p.radius / 10.0 * 9.0);
  //Offset in m
  int dLatM = sin(offsetAngle / 180.0 * M_PI ) * offsetDistance;
  int dLongM = cos(offsetAngle / 180.0 * M_PI ) * offsetDistance;
#ifdef DEVELOP
  Serial.print(F("offsetAngle = "));
  Serial.println(String(offsetAngle));

  Serial.print(F("offsetDistance = "));
  Serial.println(String(offsetDistance));

  Serial.print(F("dLatM = "));
  Serial.println(String(dLatM));

  Serial.print(F("dLongM = "));
  Serial.println(String(dLongM));
#endif
  //Earth’s radius, sphere
  double R = 6378137.0;

  //Coordinate offsets in radians
  double dLat = dLatM / R;
  double dLon = dLongM / (R * cos(M_PI * p.latitude / 180.0));
#ifdef DEVELOP
  Serial.print(F("dLat = "));
  Serial.println(String(dLat, 5));

  Serial.print(F("dLong = "));
  Serial.println(String(dLon, 5));
#endif
  //OffsetPosition, decimal degrees
  p.transformedLatitude = p.latitude + dLat * 180.0 / M_PI;
  p.transformedLongitude = p.longitude + dLon * 180.0 / M_PI ;
#ifdef DEVELOP
  Serial.print(F("p.transformedLatitude = "));
  Serial.println(String(p.transformedLatitude, 5));

  Serial.print(F("p.transformedLongitude = "));
  Serial.println(String(p.transformedLongitude, 5));
#endif
}

PrivacyArea Gps::newPrivacyArea(double latitude, double longitude, int radius) {
  PrivacyArea newPrivacyArea;
  newPrivacyArea.latitude = latitude;
  newPrivacyArea.longitude = longitude;
  newPrivacyArea.radius = radius;
  randomOffset(newPrivacyArea);
  return newPrivacyArea;
}

bool Gps::hasFix(DisplayDevice *display) const {
  bool result = false;
  if (mCurrentGpsRecord.hasValidFix() && mLastTimeTimeSet) {
    log_d("Got location...");
    display->showTextOnGrid(2, 4, "Got location");
    result = true;
  }
  return result;
}

int32_t Gps::getValidMessageCount() const {
  return mValidMessagesReceived;
}

int32_t Gps::getMessagesWithFailedCrcCount() const {
  return mMessagesWithFailedCrcReceived;
}

void Gps::showWaitStatus(DisplayDevice const * display) const {
  String satellitesString[2];
  if (mValidMessagesReceived == 0) { // could not get any valid char from GPS module
    satellitesString[0] = "OFF?";
  } else if (mLastTimeTimeSet == 0) {
    satellitesString[0] = "aGain:" + String(mLastGain);
    satellitesString[1] = String(mCurrentGpsRecord.mSatellitesUsed) + "sats SN:" + String(mLastNoiseLevel);
  } else {
    satellitesString[0] = "GPS " + TimeUtils::timeToString();
    satellitesString[1] = String(mCurrentGpsRecord.mSatellitesUsed) + "sats SN:" + String(mLastNoiseLevel);
  }

  if (satellitesString[1].isEmpty()) {
    obsDisplay->showTextOnGrid(2, display->currentLine(), satellitesString[0]);
  } else {
    obsDisplay->showTextOnGrid(2, display->currentLine() - 1, satellitesString[0]);
    obsDisplay->showTextOnGrid(2, display->currentLine(), satellitesString[1]);
  }
}

bool Gps::moduleIsAlive() const {
  return mValidMessagesReceived > 0;
}

uint8_t Gps::getValidSatellites() const {
  return mCurrentGpsRecord.mSatellitesUsed;
}

double Gps::getSpeed() const {
  return (double) mCurrentGpsRecord.mSpeed * (60.0 * 60.0) / 100.0 / 1000.0;
}

String Gps::getHdopAsString() const {
  return mCurrentGpsRecord.getHdopString();
}

String Gps::getMessages() const {
  String theGpsMessage = "";
  for (const String& msg : mMessages) {
    theGpsMessage += " // ";
    theGpsMessage += msg;
  }
  return theGpsMessage;
}

String Gps::popMessage() {
  String theGpsMessage = "";
  if (mMessages.size() > 0) {
    theGpsMessage = mMessages[0];
    mMessages.erase(mMessages.begin());
  }
  return theGpsMessage;
}

String Gps::getMessage(uint16_t idx) const {
  String theGpsMessage = "";
  if (mMessages.size() > idx) {
    theGpsMessage = mMessages[idx];
  }
  return theGpsMessage;
}

String Gps::getMessagesHtml() const {
  String theGpsMessage = "";
  for (const String& msg : mMessages) {
    theGpsMessage += "<br/>";
    theGpsMessage += ObsUtils::encodeForXmlText(msg);
  }
  return theGpsMessage;
}

/* This is the last received uptime info, this might
 * lag behind only updated with statistic messages.
 */
uint32_t Gps::getUptime() const {
  return mGpsUptime;
}

/* Read data as received form the GPS module. */
bool Gps::encode(uint8_t data) {
  bool result = false;
  // TODO: Detect delay while inside a message
  checkForCharThatCausesMessageReset(data);
  if (mReceiverState == GPS_NULL) {
    mGpsBufferBytePos = 0;
  }
  mGpsBuffer.u1Data[mGpsBufferBytePos++] = data;
  switch (mReceiverState) {
    case GPS_NULL:
      if (data == 0xB5) {
        mReceiverState = UBX_SYNC;
        mUbxChA = 0;
        mUbxChB = 0;
      } else if (data == '$') {
        mReceiverState = NMEA_START;
        mNmeaChk = 0;
      } else {
        if (data != 0) {
          log_w("Unexpected GPS char in state null: %02x %c", data, data);
          mUnexpectedCharReceivedCount++;
        }
      }
      break;
    case UBX_SYNC:
      if (data == 0x62) {
        mReceiverState = UBX_SYNC1;
      } else {
        log_w("Unexpected GPS char in state ubx sync: %02x", data);
        mUnexpectedCharReceivedCount++;
        mReceiverState = GPS_NULL;
      }
      break;
    case UBX_SYNC1:
      mUbxChA += data;
      mUbxChB += mUbxChA;
      if (mGpsBufferBytePos == 6) {
        mGpsPayloadLength = mGpsBuffer.ubxHeader.length;
        if (mGpsPayloadLength + 6 > MAX_MESSAGE_LENGTH) {
          log_w("Message claims to be %d (0x%04x) bytes long. Will ignore it, reset.",
                mGpsPayloadLength, mGpsPayloadLength);
          mReceiverState = GPS_NULL;
        } else {
          mReceiverState = UBX_PAYLOAD;
          log_v("Expecting UBX Payload: %d bytes", mGpsPayloadLength);
        }
      }
      break;
    case UBX_PAYLOAD:
      mUbxChA += data;
      mUbxChB += mUbxChA;
      if (mGpsBufferBytePos == 6 + mGpsPayloadLength) {
        mReceiverState = UBX_CHECKSUM;
      }
      break;
    case UBX_CHECKSUM:
      if (mUbxChA != data) {
        mMessagesWithFailedCrcReceived++;
        log_w("UBX CK_A error: %02x != %02x after %d bytes for 0x%04x",
              mUbxChA, data, mGpsBufferBytePos, mGpsBuffer.ubxHeader.ubxMsgId);
        mReceiverState = GPS_NULL;
      } else {
        mReceiverState = UBX_CHECKSUM1;
      }
      break;
    case UBX_CHECKSUM1:
      mReceiverState = GPS_NULL;
      if (mUbxChB != data) {
        mMessagesWithFailedCrcReceived++;
        log_w("UBX CK_B error: %02x != %02x after %b bytes for 0x%04x",
              mUbxChB, data, mGpsBufferBytePos, mGpsBuffer.ubxHeader.ubxMsgId);
      } else {
        mValidMessagesReceived++;
        result = true;
        parseUbxMessage();
      }
      break;
    case NMEA_START:
      if (mGpsBufferBytePos == 6) {
        log_v("Start of %c%c%c%c%c", mGpsBuffer.u1Data[1], mGpsBuffer.u1Data[2],
              mGpsBuffer.u1Data[3], mGpsBuffer.u1Data[4], mGpsBuffer.u1Data[5]);
        mReceiverState = NMEA_DATA;
      }
      mNmeaChk ^= data;
      break;
    case NMEA_DATA:
      if (data == '*') {
        mReceiverState = NMEA_CHECKSUM1;
        mGpsPayloadLength = mGpsBufferBytePos;
      } else {
        mNmeaChk ^= data;
      }
      break;
    case NMEA_CHECKSUM1:
      if (mNmeaChk >> 4U == hexCharToInt(data)) {
        mReceiverState = NMEA_CHECKSUM2;
      } else {
        log_w("NMEA chk 1st char error: %cX != %02x msg: %s",
              data, mNmeaChk,
              String(mGpsBuffer.charData).substring(0, mGpsBufferBytePos).c_str());
        mMessagesWithFailedCrcReceived++;
        mReceiverState = GPS_NULL;
      }
      break;
    case NMEA_CHECKSUM2:
      if ((mNmeaChk & 0x0F) == hexCharToInt(data)) {
        mValidMessagesReceived++;
        mReceiverState = NMEA_CR;
        result = true;
        // terminate string
        mGpsBuffer.u1Data[mGpsBufferBytePos] = 0;
        parseNmeaMessage();
      } else {
        // ERROR!
        log_w("NMEA chk 1st char error: %cX != %02x msg: %s",
              data, mNmeaChk,
              String(mGpsBuffer.charData).substring(0, mGpsBufferBytePos).c_str());
        mMessagesWithFailedCrcReceived++;
        mReceiverState = GPS_NULL;
      }
      break;
    case NMEA_CR:
      if (data == '\r') {
        mReceiverState = NMEA_LF;
      } else {
        log_i("Expected NMEA CR END but got %0x '%c'", data, data);
        mReceiverState = GPS_NULL;
      }
      break;
    case NMEA_LF:
      if (data != '\n') {
        log_i("Expected NMEA LF END but got %0x '%c'", data, data);
      }
      mReceiverState = GPS_NULL;
      break;
    default:
      log_e("Unexpected state: %d", mReceiverState);
      mReceiverState = GPS_NULL;
  }
  return result;
}

/* Early quick check of data on the serial that causes the current
 * collected data to be thrown away and listen to a possible new
 * message start,
 */
void Gps::checkForCharThatCausesMessageReset(uint8_t data) {
  if (mReceiverState >= NMEA_START && mReceiverState < NMEA_CR
      && !validNmeaMessageChar(data)) {
    log_w("Invalid char in NMEA message, reset: %02x '%c' \nMSG: '%s'",
          data, data,
          String(mGpsBuffer.charData).substring(0, mGpsBufferBytePos).c_str());
    mReceiverState = GPS_NULL;
    mUnexpectedCharReceivedCount++;
  }
  if (mReceiverState == NMEA_CR && data != '\r') {
    log_w("Invalid char while expecting \\r in NMEA message, reset: %02x '%c'",
          data, data);
    mReceiverState = GPS_NULL;
    mUnexpectedCharReceivedCount++;
  }
  if (mReceiverState == NMEA_LF && data != '\n') {
    log_w("Invalid char while expecting \\n in NMEA message, reset: %02x '%c'",
          data, data);
    mReceiverState = GPS_NULL;
    mUnexpectedCharReceivedCount++;
  }
  if (mReceiverState >= UBX_SYNC
      && mReceiverState <= UBX_PAYLOAD
      && mGpsBufferBytePos < 3
      && (data == 0xB5 || data == '$' /* 0x24 */)) {
    log_w("Message start char ('%c') early in UBX message (pos: %d), reset.",
          data, mGpsBufferBytePos);
    mReceiverState = GPS_NULL;
    mUnexpectedCharReceivedCount++;
  }
}

/* Note: splitting this up eats memory */
void Gps::parseUbxMessage() {
  const uint32_t receivedMs = millis();
  const uint32_t delayMs = receivedMs - mMessageStarted;
  switch (mGpsBuffer.ubxHeader.ubxMsgId) {
    case (uint16_t) UBX_MSG::ACK_ACK: {
      if (mLastAckMsgId != 0) {
        log_e("ACK overrun had ack: %d for 0x%04x", mAckReceived, mLastAckMsgId);
      }
      log_v("ACK-ACK 0x%04x", mGpsBuffer.ack.ubxMsgId);
      mAckReceived = true;
      mNakReceived = false;
      mLastAckMsgId = mGpsBuffer.ack.ubxMsgId;
    }
      break;
    case (uint16_t) UBX_MSG::ACK_NAK: {
      if (mLastAckMsgId != 0) {
        log_e("ACK-NAK overrun had ack: %d for 0x%04x", mAckReceived, mLastAckMsgId);
      }
      log_e("ACK-NAK 0x%04x", mGpsBuffer.ack.ubxMsgId);
      mAckReceived = false;
      mNakReceived = true;
      mLastAckMsgId = mGpsBuffer.ack.ubxMsgId;
    }
      break;
    case (uint16_t) UBX_MSG::CFG_PRT:
      log_i("CFG-PRT Port: %d, Baud: %d",
            mGpsBuffer.cfgPrt.portId, mGpsBuffer.cfgPrt.baudRate);
      break;
    case (uint16_t) UBX_MSG::CFG_RINV: {
      mGpsBuffer.u1Data[mGpsBufferBytePos - 2] = 0;
      log_v("CFG-RINV flags: %02x, Message %s",
            mGpsBuffer.cfgRinv.flags, &mGpsBuffer.cfgRinv.data);
      String rinv = String(mGpsBuffer.cfgRinv.data);
      addStatisticsMessage(String("RINV: ") + rinv);
      if (!rinv.equals(String("openbikesensor.org/") + OBSVersion)) {
        log_i("GPS config from %s outdated - will trigger update.", rinv.c_str());
        mGpsNeedsConfigUpdate = true;
      } else {
        mGpsNeedsConfigUpdate = false;
      }
    }
      break;
    case (uint16_t) UBX_MSG::MON_VER: {
// a bit a hack - but do not let the strings none zero terminated.
      mGpsBuffer.monVer.swVersion[sizeof(mGpsBuffer.monVer.swVersion) - 1] = 0;
      mGpsBuffer.monVer.hwVersion[sizeof(mGpsBuffer.monVer.hwVersion) - 1] = 0;
      mGpsBuffer.monVer.romVersion[sizeof(mGpsBuffer.monVer.romVersion) - 1] = 0;
      mGpsBuffer.monVer.extension0[sizeof(mGpsBuffer.monVer.extension0) - 1] = 0;
      mGpsBuffer.monVer.extension1[sizeof(mGpsBuffer.monVer.extension1) - 1] = 0;

      addStatisticsMessage("swVersion: " + String(mGpsBuffer.monVer.swVersion));
      addStatisticsMessage("hwVersion: " + String(mGpsBuffer.monVer.hwVersion));
      if (mGpsPayloadLength > 40) {
        addStatisticsMessage("romVersion: " + String(mGpsBuffer.monVer.romVersion));
      }
      if (mGpsPayloadLength > 70) {
        addStatisticsMessage("extension: " + String(mGpsBuffer.monVer.extension0));
      }
      if (mGpsPayloadLength > 100) {
        addStatisticsMessage("extension: " + String(mGpsBuffer.monVer.extension1));
      }
      log_d("MON-VER SW Version: %s, HW Version %s, len %d",
            String(mGpsBuffer.monVer.swVersion).c_str(),
            String(mGpsBuffer.monVer.hwVersion).c_str(),
            mGpsBuffer.ubxHeader.length);
    }
      break;
    case (uint16_t) UBX_MSG::MON_HW: {
      const char* aStatus;
      switch (mGpsBuffer.monHw.aStatus) {
        case mGpsBuffer.monHw.INIT: aStatus = "init"; break;
        case mGpsBuffer.monHw.DONTKNOW: aStatus = "?"; break;
        case mGpsBuffer.monHw.OK: aStatus = "ok"; break;
        case mGpsBuffer.monHw.SHORT: aStatus = "short"; break;
        case mGpsBuffer.monHw.OPEN: aStatus = "open"; break;
        default: aStatus = "invalid";
      }
      log_d("MON-HW Antenna Status %d %s, Antenna Power %d, Gain (0-8191) %d, noise level %d", mGpsBuffer.monHw.aStatus, aStatus, mGpsBuffer.monHw.aPower, mGpsBuffer.monHw.agcCnt, mGpsBuffer.monHw.noisePerMs);
      mLastNoiseLevel = mGpsBuffer.monHw.noisePerMs;
      mLastGain = mGpsBuffer.monHw.agcCnt;
    }
      break;
    case (uint16_t) UBX_MSG::NAV_STATUS: {
      log_v("NAV-STATUS uptime: %d, timeToFix: %d, gpsFix: %02x",
            mGpsBuffer.navStatus.msss, mGpsBuffer.navStatus.ttff,
            mGpsBuffer.navStatus.gpsFix);
      mGpsUptime = mGpsBuffer.navStatus.msss;
      if (mGpsBuffer.navStatus.ttff != 0) {
        addStatisticsMessage("TimeToFix: " + String(mGpsBuffer.navStatus.ttff) + "ms");
      } else if (!mAidIniSent) {
        mAidIniSent = true;
        aidIni();
      }
    }
      break;
    case (uint16_t) UBX_MSG::NAV_DOP: {
      log_v("DOP: iTOW: %u, gDop: %04d, pDop: %04d, tDop: %04d, "
            "vDop: %04d, hDop: %04d, nDop: %04d, eDop: %04d",
            mGpsBuffer.navDop.iTow, mGpsBuffer.navDop.gDop, mGpsBuffer.navDop.pDop,
            mGpsBuffer.navDop.tDop, mGpsBuffer.navDop.vDop, mGpsBuffer.navDop.hDop,
            mGpsBuffer.navDop.nDop, mGpsBuffer.navDop.eDop);
      prepareGpsData(mGpsBuffer.navDop.iTow, mMessageStarted);
      mIncomingGpsRecord.setHdop(mGpsBuffer.navDop.hDop);
    }
      break;
    case (uint16_t) UBX_MSG::NAV_SOL: {
      log_d("SOL: iTOW: %u, gpsFix: %d, flags: %02x, numSV: %d, pDop: %04d.",
            mGpsBuffer.navSol.iTow, mGpsBuffer.navSol.gpsFix, mGpsBuffer.navSol.flags,
            mGpsBuffer.navSol.numSv, mGpsBuffer.navSol.pDop);
      if (mGpsBuffer.navSol.flags & 4) { // WKNSET
        if (mLastGpsWeek != mGpsBuffer.navSol.week) {
          // debugging #294
          addStatisticsMessage(String("NAVSOL gps week changed: ")
                               + mLastGpsWeek + " -> " + mGpsBuffer.navSol.week
                               + " at " + TimeUtils::dateTimeToString());
        }
        mLastGpsWeek = mGpsBuffer.navSol.week;
        mIncomingGpsRecord.setWeek(mLastGpsWeek);
      }
      prepareGpsData(mGpsBuffer.navDop.iTow, mMessageStarted);
      mIncomingGpsRecord.setInfo(mGpsBuffer.navSol.numSv, mGpsBuffer.navSol.gpsFix, mGpsBuffer.navSol.flags);
    }
      break;
    case (uint16_t) UBX_MSG::NAV_PVT: {
      log_d("PVT: iTOW: %u, fixType: %d, flags: %02x, numSV: %d, pDop: %04d.",
            mGpsBuffer.navPvt.iTow, mGpsBuffer.navPvt.fixType, mGpsBuffer.navPvt.flags,
            mGpsBuffer.navPvt.numSV, mGpsBuffer.navPvt.pDOP);
      prepareGpsData(mGpsBuffer.navPvt.iTow, mMessageStarted);
      mIncomingGpsRecord.setInfo(mGpsBuffer.navPvt.numSV, mGpsBuffer.navPvt.fixType, mGpsBuffer.navPvt.flags);
    }
      break;
    case (uint16_t) UBX_MSG::NAV_VELNED: {
      log_d("VELNED: iTOW: %u, speed: %d cm/s, gSpeed: %d cm/s, heading: %d,"
            " speedAcc: %d, cAcc: %d",
            mGpsBuffer.navVelned.iTow, mGpsBuffer.navVelned.speed, mGpsBuffer.navVelned.gSpeed,
            mGpsBuffer.navVelned.heading, mGpsBuffer.navVelned.sAcc, mGpsBuffer.navVelned.cAcc);
      prepareGpsData(mGpsBuffer.navVelned.iTow, mMessageStarted);
      mIncomingGpsRecord.setVelocity(mGpsBuffer.navVelned.gSpeed, mGpsBuffer.navVelned.heading);
    }
      break;
    case (uint16_t) UBX_MSG::NAV_POSLLH: {
      log_d("POSLLH: iTOW: %u lon: %d lat: %d height: %d hMsl %d, hAcc %d, vAcc %d delay %dms",
            mGpsBuffer.navPosllh.iTow, mGpsBuffer.navPosllh.lon, mGpsBuffer.navPosllh.lat,
            mGpsBuffer.navPosllh.height, mGpsBuffer.navPosllh.hMsl, mGpsBuffer.navPosllh.hAcc,
            mGpsBuffer.navPosllh.vAcc, delayMs);
      prepareGpsData(mGpsBuffer.navPosllh.iTow, mMessageStarted);
      mIncomingGpsRecord.setPosition(mGpsBuffer.navPosllh.lon, mGpsBuffer.navPosllh.lat, mGpsBuffer.navPosllh.hMsl);
    }
      break;
    case (uint16_t) UBX_MSG::NAV_TIMEGPS:
      handleUbxNavTimeGps(mGpsBuffer.navTimeGps, receivedMs, delayMs);
      break;
    case (uint16_t) UBX_MSG::NAV_TIMEUTC: {
      prepareGpsData(mGpsBuffer.navTimeUtc.iTow, mMessageStarted);
      log_d("TIMEUTC: iTOW: %u acc: %uns nano: %d %04u-%02u-%02uT%02u:%02u:%02u valid 0x%02x delay %dms",
            mGpsBuffer.navTimeUtc.iTow, mGpsBuffer.navTimeUtc.tAcc, mGpsBuffer.navTimeUtc.nano,
            mGpsBuffer.navTimeUtc.year, mGpsBuffer.navTimeUtc.month, mGpsBuffer.navTimeUtc.day,
            mGpsBuffer.navTimeUtc.hour, mGpsBuffer.navTimeUtc.minute, mGpsBuffer.navTimeUtc.sec,
            mGpsBuffer.navTimeUtc.valid, delayMs);
    }
      break;
    case (uint16_t) UBX_MSG::NAV_SBAS: {
      prepareGpsData(mGpsBuffer.navSbas.iTow, mMessageStarted);
      log_d("SBAS: iTOW: %u geo: %u, mode: %u, sys: %u, service: %02x, cnt: %d",
            mGpsBuffer.navSbas.iTow, mGpsBuffer.navSbas.geo, mGpsBuffer.navSbas.mode,
            mGpsBuffer.navSbas.sys, mGpsBuffer.navSbas.service, mGpsBuffer.navSbas.cnt);
      addStatisticsMessage(String("SBAS: mode: ")
                           + String((int16_t) mGpsBuffer.navSbas.mode)
                           + " System: " + String((int16_t) mGpsBuffer.navSbas.sys)
                           + " cnt: " + String((int16_t) mGpsBuffer.navSbas.cnt));
    }
      break;
    case (uint16_t) UBX_MSG::AID_INI:
      handleUbxAidIni(mGpsBuffer.aidIni);
      break;
    case (uint16_t) UBX_MSG::AID_HUI:
      handleUbxAidHui(mGpsBuffer.aidHui);
      break;
    case (uint16_t) UBX_MSG::AID_ALPSRV: {
      uint16_t startOffset
        = mGpsBuffer.aidAlpsrvClientReq.idSize + 6;
      if (mGpsBuffer.aidAlpsrvClientReq.type != 0xFF) {
        log_d("AID-ALPSRV-REQ Got data request %d for type %d, offset %d, size %d",
              mGpsBuffer.aidAlpsrvClientReq.idSize,
              mGpsBuffer.aidAlpsrvClientReq.type,
              mGpsBuffer.aidAlpsrvClientReq.ofs,
              mGpsBuffer.aidAlpsrvClientReq.size);

        mGpsBuffer.aidAlpsrvClientReq.fileId = 2;
        uint16_t length = (uint16_t) MAX_MESSAGE_LENGTH - startOffset;
        if (length > 2 * mGpsBuffer.aidAlpsrvClientReq.size) {
          length = 2 * mGpsBuffer.aidAlpsrvClientReq.size;
        }
        mGpsBuffer.aidAlpsrvClientReq.dataSize =
          mAlpData.fill(&mGpsBuffer.u1Data[startOffset],
                        2u * mGpsBuffer.aidAlpsrvClientReq.ofs,
                        length);
        if (mGpsBuffer.aidAlpsrvClientReq.dataSize > 0) {
          mGpsBuffer.ubxHeader.length
            = mGpsBuffer.aidAlpsrvClientReq.dataSize + mGpsBuffer.aidAlpsrvClientReq.idSize;
          sendUbxDirect();
          mAlpBytesSent += mGpsBuffer.aidAlpsrvClientReq.dataSize;
          log_d("Did send %d bytes Pos: 0x%x",
                mGpsBuffer.ubxHeader.length,
                2 * mGpsBuffer.aidAlpsrvClientReq.ofs);
        }
#ifdef RANDOM_ACCESS_FILE_AVAILAVLE
        } else {
          log_e("AID-ALPSRV-REQ Got store data request %d for type %d, offset %d, size %d, file %d",
                mGpsBuffer.aidAlpsrvClientReq.idSize,
                mGpsBuffer.aidAlpsrvClientReq.type,
                mGpsBuffer.aidAlpsrvClientReq.ofs,
                mGpsBuffer.aidAlpsrvClientReq.size,
                mGpsBuffer.aidAlpsrvClientReq.fileId);
          // Check boundaries!
          mAlpData.save(&mGpsBuffer.u1Data[startOffset],
                        2 * mGpsBuffer.aidAlpsrvClientReq.ofs,
                        2 * mGpsBuffer.aidAlpsrvClientReq.size);
          log_e("save %d bytes took %d ms", 2 * mGpsBuffer.aidAlpsrvClientReq.size, millis() - start);
#endif
      }
    }
      break;
    case (uint16_t) UBX_MSG::AID_ALP: {
      log_d("AID-ALP status data age %d duration %d valid from %s to %s",
            mGpsBuffer.aidAlpStatus.age,
            mGpsBuffer.aidAlpStatus.predDur,
            TimeUtils::dateTimeToString(
              TimeUtils::toTime(mGpsBuffer.aidAlpStatus.predWno, mGpsBuffer.aidAlpStatus.predTow)).c_str(),
            TimeUtils::dateTimeToString(
              TimeUtils::toTime(mGpsBuffer.aidAlpStatus.predWno,
                     mGpsBuffer.aidAlpStatus.predDur + mGpsBuffer.aidAlpStatus.predTow)).c_str());
      if (mGpsBuffer.aidAlpStatus.predWno != 0) {
        addStatisticsMessage(String("ALP Data valid from: ") +
                               TimeUtils::dateTimeToString(
                                 TimeUtils::toTime(mGpsBuffer.aidAlpStatus.predWno, mGpsBuffer.aidAlpStatus.predTow)));
        addStatisticsMessage(String("ALP Data valid to: ") +
                               TimeUtils::dateTimeToString(
                               TimeUtils::toTime(mGpsBuffer.aidAlpStatus.predWno,
                                      mGpsBuffer.aidAlpStatus.predDur + mGpsBuffer.aidAlpStatus.predTow)).c_str());
      }
    }
      break;
    case (uint16_t) UBX_MSG::INF_ERROR:
    case (uint16_t) UBX_MSG::INF_WARNING:
    case (uint16_t) UBX_MSG::INF_NOTICE:
    case (uint16_t) UBX_MSG::INF_TEST:
    case (uint16_t) UBX_MSG::INF_DEBUG: {
      mGpsBuffer.u1Data[mGpsBufferBytePos - 2] = 0;
      log_d("INF %d message: %s",
            mGpsBuffer.ubxHeader.ubxMsgId, String(mGpsBuffer.inf.message).c_str());
      addStatisticsMessage(
        INF_SEVERITY_STRING[mGpsBuffer.u1Data[3]] + ": " + String(mGpsBuffer.inf.message));
    }
      break;
    case (uint16_t) UBX_MSG::CFG_SBAS:
      log_d("CFG_SBAS");
      break;
    case (uint16_t) UBX_MSG::CFG_NAV5:
      log_d("CFG_NAV5");
      break;
    case (uint16_t) UBX_MSG::CFG_GNSS:
      log_d("CFG_GNSS");
      break;
    default:
      log_e("Got unparsed UBX_MESSAGE! Id: 0x%04x Len %d iTOW %d", mGpsBuffer.ubxHeader.ubxMsgId,
            mGpsBuffer.ubxHeader.length, mGpsBuffer.navStatus.iTow);
  }
}

void Gps::handleUbxNavTimeGps(const GpsBuffer::UbxNavTimeGps &message, const uint32_t receivedMs, const uint32_t delayMs) {
  prepareGpsData(message.iTow, mMessageStarted);
  log_i("TIMEGPS: iTOW: %u, fTOW: %d, week %d, leapS: %d, valid: 0x%02x (%s%s%s), tAcc %uns, DATE: %s, delay %dms",
        message.iTow, message.fTow, message.week, message.leapS, message.valid,
        message.valid & 1 ? "TOW" : "",
        message.valid & 2 ? " WEEK" : "",
        message.valid & 4 ? " UTC" : "",
        message.tAcc,
        TimeUtils::dateTimeToString(TimeUtils::toTime(message.week, message.iTow / 1000)).c_str(),
        delayMs);
  if (message.valid & 2) {
    if (mLastGpsWeek != message.week) {
      // debugging #294
      addStatisticsMessage(String("TIMEGPS gps week changed: ")
                           + mLastGpsWeek + " -> " + message.week);
    }
    mLastGpsWeek = message.week;
    mIncomingGpsRecord.setWeek(mLastGpsWeek);
  }
  if ((message.valid & 0x03) == 0x03  // WEEK && TOW
      && delayMs < 250
      && message.tAcc < (20 * 1000 * 1000 /* 20ms */)
      && (mLastTimeTimeSet == 0
          || (mLastTimeTimeSet + (2 * 60 * 1000 /* 2 minutes */)) < receivedMs)) {
    String oldTime = TimeUtils::dateTimeToString();
    TimeUtils::setClockByGps(message.iTow, message.fTow, message.week);
    String newTime = TimeUtils::dateTimeToString();
    if (oldTime != newTime) {
      log_i("TIMEGPS set: %s -> %s", oldTime.c_str(), newTime.c_str());
      addStatisticsMessage(String("TIMEGPS set: ")
                           + oldTime + " -> " + newTime + " delay " + String(delayMs)
                           + "ms. tAcc:" + String(message.tAcc) + "ns");
    }
    if (mLastTimeTimeSet == 0) {
      mLastTimeTimeSet = receivedMs;
      // This triggers another NAV-TIMEGPS message!
#ifdef UBX_M6
      setMessageInterval(UBX_MSG::NAV_TIMEGPS, 240, false); // every 4 minutes
#endif
#ifdef UBX_M10
      setMessageInterval(UBX_CFG_KEY_ID::CFG_MSGOUT_UBX_NAV_TIMEGPS_UART1, 240, false); // every 4 minutes
#endif
    } else {
      mLastTimeTimeSet = receivedMs;
    }
  }
}

void Gps::handleUbxAidIni(const GpsBuffer::AidIni &message) const {
  log_d("AidIni received Status: 0x%04x, Location valid: %d.", message.flags,
        (message.flags & GpsBuffer::AidIni::POS));
  if ((message.flags & GpsBuffer::AidIni::POS)
      && message.posAcc < 50000) {
    AlpData::saveMessage(mGpsBuffer.u1Data, mGpsPayloadLength + 6);
    log_d("Stored new AidIni data.");
  }
}

void Gps::handleUbxAidHui(const GpsBuffer::AidHui &massage) const {
  if ((uint32_t) massage.flags & (uint32_t) GpsBuffer::AidHui::Flags::utc) {
    log_i("AID_HUI received Flags: %04x, Current Leap Seconds %d"
          " Event: %s, Leap Seconds after Event: %d",
          massage.flags, massage.utcLS,
          TimeUtils::dateTimeToString(
            TimeUtils::gpsDayToTime(massage.utcWNF, massage.utcDN)).c_str(),
          massage.utcLSF);
  }
}

bool Gps::validNmeaMessageChar(uint8_t chr) {
  return (chr >= 0x20 && chr <= 0x7e && chr != '$');
}

uint8_t Gps::hexCharToInt(uint8_t data) {
  if (data >= '0' && data <='9') {
    return data - '0';
  }
  if (data >= 'A' && data <='F') {
    return data - 'A' + 10;
  }
  if (data >= 'a' && data <='f') {
    return data - 'a' + 10;
  }
  return 99; // ERROR
}

void Gps::parseNmeaMessage() const {
  log_w("Unparsed NMEA %c%c%c%c%c: %s", mGpsBuffer.u1Data[1], mGpsBuffer.u1Data[2],
        mGpsBuffer.u1Data[3], mGpsBuffer.u1Data[4], mGpsBuffer.u1Data[5],
        mGpsBuffer.charData);
}

void Gps::aidIni() {
  if (AlpData::loadMessage(mGpsBuffer.u1Data, 48 + 8) > 48) {
    log_i("Will send AID_INI");
    mGpsBuffer.aidIni.posAcc = 5000; // 50m
    mGpsBuffer.aidIni.tAccMs = 3 * 24 * 60 * 60 * 1000; // 3 days!?
    mGpsBuffer.aidIni.flags = (GpsBuffer::AidIni::FLAGS) 0x03;
    time_t t = time(nullptr);
    if (t > TimeUtils::PAST_TIME) {
      mGpsBuffer.aidIni.tAccMs = 1000; // 1 sec
      mGpsBuffer.aidIni.tAccNs = 0;
      mGpsBuffer.aidIni.towNs = 0;
      mGpsBuffer.aidIni.tow = TimeUtils::utcTimeToTimeOfWeek(t);
      mGpsBuffer.aidIni.wn = TimeUtils::utcTimeToWeekNumber(t);
    }
    sendUbxDirect();
    handle(5);
  } else {
    log_e("Will not send AID_INI - invalid data on SD?");
  }
}

uint16_t Gps::getLastNoiseLevel() const {
  return mLastNoiseLevel;
}

uint32_t Gps::getBaudRate() {
  return mSerial.baudRate();
}

void Gps::resetMessages() {
  mMessages.clear();
}

/* Prepare the GPS data record for incoming data for the given tow. */
void Gps::prepareGpsData(uint32_t tow, uint32_t messageStartedMillisTicks) {
  if (mIncomingGpsRecord.mCollectTow == tow) {
    // fine already prepared
  } else if (mIncomingGpsRecord.mCollectTow == 0) {
    // new tow
    mIncomingGpsRecord.reset(tow, mLastGpsWeek, messageStartedMillisTicks);
  } else {
    if (mIncomingGpsRecord.mCollectTow > tow) {
      log_e("TOW getting smaller -  published: %d, received: %d",
            mIncomingGpsRecord.mCollectTow, tow);
    }
    if (!mIncomingGpsRecord.isAllSet()) {
      log_w("Had to switch incomplete record tow: %d"
            " pos: %d, info: %d, hdop: %d, vel: %d, started at %d",
            mIncomingGpsRecord.mCollectTow,
            mIncomingGpsRecord.mPositionSet,
            mIncomingGpsRecord.mInfoSet,
            mIncomingGpsRecord.mHdopSet,
            mIncomingGpsRecord.mVelocitySet,
            mIncomingGpsRecord.mCreatedAtMillisTicks);
    }
    mCurrentGpsRecord = mIncomingGpsRecord;
    mIncomingGpsRecord.reset(tow, mLastGpsWeek, messageStartedMillisTicks);
  }
}

GpsRecord Gps::getIncomingGpsRecord() const {
  return mIncomingGpsRecord;
}

bool Gps::currentTowEquals(uint32_t tow) const {
  return mIncomingGpsRecord.mCollectTow == tow;
};

bool Gps::hasTowTicks() const {
  return mIncomingGpsRecord.mCollectTow != 0
    || mIncomingGpsRecord.mCollectTow != mCurrentGpsRecord.mCollectTow;
};

GpsRecord Gps::getCurrentGpsRecord() const {
  return mCurrentGpsRecord;
}

uint32_t Gps::getNumberOfAlpBytesSent() const {
  return mAlpBytesSent;
}

uint32_t Gps::getUnexpectedCharReceivedCount() const {
  return mUnexpectedCharReceivedCount;
}
