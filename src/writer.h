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

#ifndef OBS_WRITER_H
#define OBS_WRITER_H

#include <Arduino.h>
#include <SD.h>
#include <utility>
#include <vector>

#include "globals.h"


struct DataSet {
  time_t time;
  uint32_t  millis;
  String comment;
  GpsRecord gpsRecord;
  double batteryLevel;
  std::vector<uint16_t> sensorValues;
  std::vector<uint16_t> confirmedDistances;
  std::vector<uint16_t> confirmedDistancesIndex;
  uint16_t confirmed = 0;
  String marked;
  bool invalidMeasurement = false;
  bool isInsidePrivacyArea;
  uint8_t factor = MICRO_SEC_TO_CM_DIVIDER;
  uint8_t measurements;

  uint16_t position = 0; // fixme: num sensors?
  uint16_t startOffsetMilliseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
  int32_t readDurationsLeftInMicroseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
  int32_t readDurationsRightInMicroseconds[MAX_NUMBER_MEASUREMENTS_PER_INTERVAL + 1];
};

class FileWriter {
  public:
    FileWriter() = default;;
    explicit FileWriter(String ext) :
      mFileExtension(std::move(ext)) {};
    virtual ~FileWriter() = default;
    void setFileName();
    virtual bool writeHeader(String trackId) = 0;
    virtual bool append(DataSet &) = 0;
    bool appendString(const String &s);
    bool flush();
    unsigned long getWriteTimeMillis() const;

  protected:
    uint16_t getBufferLength() const;

  private:
    static void storeTrackNumber(int trackNumber);
    static int getTrackNumber();
    void correctFilename();
    String mBuffer;
    String mFileExtension;
    String mFileName;
    const unsigned long mStartedMillis = millis();
    bool mFinalFileName = false;
    unsigned long mWriteTimeMillis = 0;

};

class CSVFileWriter : public FileWriter {
  public:
    CSVFileWriter() : FileWriter(EXTENSION) {}
    ~CSVFileWriter() override = default;
    bool writeHeader(String trackId) override;
    bool append(DataSet&) override;
    static const String EXTENSION;
};

#endif
