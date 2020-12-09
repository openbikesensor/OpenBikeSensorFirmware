/*
  Copyright (C) 2019 Zweirat
  Contact: https://openbikesensor.org

  This file is part of the OpenBikeSensor project.

  The OpenBikeSensor sensor firmware is free software: you can redistribute
  it and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  The OpenBikeSensor sensor firmware is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along with
  the OpenBikeSensor sensor firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OBS_WRITER_H
#define OBS_WRITER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <utility>
#include <vector>

#include "config.h"
#include "globals.h"


struct DataSet {
  time_t time;
  uint32_t  millis;
  String comment;
  TinyGPSLocation location;
  TinyGPSAltitude altitude;
  TinyGPSCourse course;
  TinyGPSSpeed speed;
  TinyGPSHDOP hdop;
  uint8_t validSatellites;
  double batteryLevel;
  std::vector<uint16_t> sensorValues;
  std::vector<uint16_t> confirmedDistances;
  std::vector<uint16_t> confirmedDistancesTimeOffset;
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
    FileWriter() :
      mStartedMillis(millis()),
      mFinalFileName(false) {};
    explicit FileWriter(String ext) :
      mFileExtension(std::move(ext)),
      mStartedMillis(millis()),
      mFinalFileName(false) {};
    virtual ~FileWriter() = default;
    void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
    void createDir(fs::FS &fs, const char * path);
    void removeDir(fs::FS &fs, const char * path);
    void readFile(fs::FS &fs, const char * path);
    void writeFile(fs::FS &fs, const char * path, const char * message);
    void appendFile(fs::FS &fs, const char * path, const char * message);
    bool renameFile(fs::FS &fs, const char * path1, const char * path2);
    void deleteFile(fs::FS &fs, const char * path);
    void setFileName();
    String getFileName();
    void writeDataBuffered(DataSet *set);
    void writeDataToSD();
    uint16_t getBufferLength() const;
    virtual void init() = 0;
    virtual void writeHeader() = 0;
    virtual void writeData(DataSet*) = 0;

  protected:
    unsigned long getWriteTimeMillis();
    String mBuffer;

  private:
    void storeTrackNumber(int trackNumber) const;
    int getTrackNumber() const;
    void correctFilename();
    String mFileExtension;
    String mFileName;
    const unsigned long mStartedMillis;
    bool mFinalFileName;
    unsigned long mWriteTimeMillis = 0;

};

class CSVFileWriter : public FileWriter {
  public:
    CSVFileWriter() : FileWriter(EXTENSION) {}
    ~CSVFileWriter() override = default;
    void init() override {
    }
    void writeHeader() override;
    void writeData(DataSet*) override;
    static const String EXTENSION;
};

class GPXFileWriter : public FileWriter {
  public:
    GPXFileWriter() : FileWriter(EXTENSION) {
    }
    ~GPXFileWriter() override = default;
    void writeHeader() override;
    void writeData(DataSet*) override;
    static const String EXTENSION;
  protected:
  private:
};

#endif
