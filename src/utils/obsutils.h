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

#ifndef OPENBIKESENSORFIRMWARE_OBSUTILS_H
#define OPENBIKESENSORFIRMWARE_OBSUTILS_H

#include <Arduino.h>

/**
 * Class for internal - static only methods.
 */
class ObsUtils {
  public:
    static String createTrackUuid();
    static String sha256ToString(byte *sha256);
    /* Strips technical details like extension or '/' from the file name. */
    static String stripCsvFileName(const String &fileName);
    static String encodeForXmlAttribute(const String & text);
    static String encodeForXmlText(const String &text);
    static String encodeForCsvField(const String &field);
    static String encodeForUrl(const String &url);
    static String toScaledByteString(uint64_t size);
    static void logHexDump(const uint8_t *buffer, uint16_t length);
    static String to3DigitString(uint32_t value);

};


#endif //OPENBIKESENSORFIRMWARE_OBSUTILS_H
