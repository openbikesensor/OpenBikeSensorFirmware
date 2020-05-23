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

void FileWriter::listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void FileWriter::createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void FileWriter::removeDir(fs::FS &fs, const char * path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void FileWriter::readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void FileWriter::writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void FileWriter::appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void FileWriter::renameFile(fs::FS &fs, const char * path1, const char * path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void FileWriter::deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void FileWriter::setFileName() {
  int fileSuffix = 0;
  String base_filename = "/sensorData";
  m_filename = base_filename + String(fileSuffix) + m_fileExtension;
  while (SD.exists(m_filename.c_str()))
  {
    fileSuffix++;
    m_filename = base_filename + String(fileSuffix) + m_fileExtension;
  }
}

uint16_t FileWriter::getDataLength() {
  return dataString.length();
}

void FileWriter::writeDataToSD() {
  this->appendFile(SD, m_filename.c_str(), dataString.c_str() );
  dataString = "";
}

void CSVFileWriter::writeHeader() {
  String headerString;
  headerString += "Date;Time;Latitude;Longitude;Course;Speed";
  for (size_t idx = 0; idx < sensorManager->m_sensors.size(); ++idx)
  {
    headerString += ";";
    headerString += sensorManager->m_sensors[idx].sensorLocation;
  }
  headerString += ";Confirmed";
  headerString += "\n";
  this->appendFile(SD, m_filename.c_str(), headerString.c_str() );
}

void CSVFileWriter::writeData(DataSet* set) {
  //String dataString = "";

  char dateString[12];
  sprintf(dateString, "%02d.%02d.%04d;", set->date.day(), set->date.month(), set->date.year());
  dataString = dataString + dateString;

  char timeString[12];
  sprintf(timeString, "%02d:%02d:%02d;", set->time.hour(), set->time.minute(), set->time.second());
  dataString = dataString + timeString;

  String latitudeString = String(set->location.lat(), 6);
  dataString = dataString + latitudeString + ";";

  String longitudeString = String(set->location.lng(), 6);
  dataString = dataString + longitudeString + ";";

  String courseString = String(set->course.deg(), 3);
  dataString = dataString + courseString + ";";

  String speedString = String(set->speed.kmph(), 4);
  dataString = dataString + speedString;

  for (size_t idx = 0; idx < set->sensorValues.size(); ++idx)
  {
    dataString = dataString + ";" + String(set->sensorValues[idx]);
  }
  dataString = dataString + ";" + String(set->confirmed);
  dataString = dataString + "\n";

}



void GPXFileWriter::writeHeader() {
  String headerString;
  headerString += F(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<gpx version=\"1.0\">\n"
                    "\t<trk><trkseg>\n");
  this->appendFile(SD, m_filename.c_str(), headerString.c_str() );
}

void GPXFileWriter::writeData(DataSet* set) {
  String dataString;

  dataString += "\t\t<trkpt ";
  dataString += "lat=\"";
  String latitudeString = String(set->location.lat(), 6);
  dataString += latitudeString;
  dataString += "\" lon=\"";
  String longitudeString = String(set->location.lng(), 6);
  dataString += longitudeString;
  dataString += "\">";

  char dateTimeString[25];
  sprintf(
    dateTimeString,
    "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
    set->date.year(),
    set->date.month(),
    set->date.day(),
    set->time.hour(),
    set->time.minute(),
    set->time.second(),
    set->time.centisecond());
  dataString += F("<time>");
  dataString += dateTimeString;
  dataString += F("</time>");

  dataString += F("<ele>");
  dataString += String(set->altitude.meters(), 2);
  dataString += F("</ele>");

  dataString += F("</trkpt>\n");

  this->appendFile(SD, m_filename.c_str(), dataString.c_str() );
}

/*
  // call back for timestamps
  void dateTime(uint16_t* date, uint16_t* time) {
  DateTime now = RTC.now();

  // return date using FAT_DATE macro to format fields
   date = FAT_DATE(now.year(), now.month(), now.day());

  // return time using FAT_TIME macro to format fields
   time = FAT_TIME(now.hour(), now.minute(), now.second());
  }
*/
