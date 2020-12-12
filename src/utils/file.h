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
#ifndef OPENBIKESENSORFIRMWARE_FILE_H
#define OPENBIKESENSORFIRMWARE_FILE_H

#include <FS.h>

class FileUtil {
  public:
    static void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
    static void createDir(fs::FS &fs, const char * path);
    static void removeDir(fs::FS &fs, const char * path);
    static void readFile(fs::FS &fs, const char * path);
    static void writeFile(fs::FS &fs, const char * path, const char * message);
    static bool appendFile(fs::FS &fs, const char * path, const char * message);
    static bool renameFile(fs::FS &fs, const char * path1, const char * path2);
    static void deleteFile(fs::FS &fs, const char * path);
};

#endif //OPENBIKESENSORFIRMWARE_FILE_H
