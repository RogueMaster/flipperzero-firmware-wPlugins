/*
    FS.h - file system wrapper
    Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.
    This file is part of the esp8266 core for Arduino environment.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include <memory>
#include <Arduino.h>
#include <../include/time.h>  // See issue #6714

class SDClass;

namespace fs {

class File;
class Dir;
class FS;

class FileImpl;
typedef std::shared_ptr<FileImpl> FileImplPtr;
class FSImpl;
typedef std::shared_ptr<FSImpl> FSImplPtr;
class DirImpl;
typedef std::shared_ptr<DirImpl> DirImplPtr;

template<typename Tfs> bool mount(Tfs &fs, const char *mountPoint);

enum SeekMode {
  SeekSet = 0,
  SeekCur = 1,
  SeekEnd = 2
};

struct FSStat {
  size_t size;
  size_t blocksize;
  time_t ctime;
  time_t atime;
  bool isDir;
};

class File : public Stream {
public:
  File(FileImplPtr p = FileImplPtr(), FS *baseFS = nullptr) : _p(p), _fakeDir(nullptr), _baseFS(baseFS) {
    _startMillis = millis(); /* workaround -O3 spurious warning #768 */
  }

  // Print methods:
  size_t write(uint8_t) override;
  size_t write(const uint8_t *buf, size_t size) override;
  int availableForWrite() override;

  // Stream methods:
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t readBytes(char *buffer, size_t length) {
    return read((uint8_t *)buffer, length);
  }
  int read(uint8_t *buf, size_t size);
  bool seek(uint32_t pos, SeekMode mode);
  bool seek(uint32_t pos) {
    return seek(pos, SeekSet);
  }
  size_t position() const;
  size_t size() const;
  virtual ssize_t streamRemaining() {
    return (ssize_t)size() - (ssize_t)position();
  }
  void close();
  operator bool() const;
  const char *name() const;
  const char *fullName() const;  // Includes path
  bool truncate(uint32_t size);

  bool isFile() const;
  bool isDirectory() const;

  // Arduino "class SD" methods for compatibility
  //TODO use stream::send / check read(buf,size) result
  template<typename T> size_t write(T &src) {
    uint8_t obuf[256];
    size_t doneLen = 0;
    size_t sentLen;

    while ((size_t)src.available() > sizeof(obuf)) {
      src.read(obuf, sizeof(obuf));
      sentLen = write(obuf, sizeof(obuf));
      doneLen = doneLen + sentLen;
      if (sentLen != sizeof(obuf)) {
        return doneLen;
      }
    }

    size_t leftLen = src.available();
    src.read(obuf, leftLen);
    sentLen = write(obuf, leftLen);
    doneLen = doneLen + sentLen;
    return doneLen;
  }
  using Print::write;

  void rewindDirectory();
  File openNextFile();

  String readString();

  time_t getLastWrite();
  time_t getCreationTime();
  void setTimeCallback(time_t (*cb)(void));

  bool stat(FSStat *st);

protected:
  FileImplPtr _p;
  time_t (*_timeCallback)(void) = nullptr;

  // Arduino SD class emulation
  std::shared_ptr<Dir> _fakeDir;
  FS *_baseFS;
};

class Dir {
public:
  Dir(DirImplPtr impl = DirImplPtr(), FS *baseFS = nullptr) : _impl(impl), _baseFS(baseFS) {}

  File openFile(const char *mode);

  String fileName();
  size_t fileSize();
  time_t fileTime();
  time_t fileCreationTime();
  bool isFile() const;
  bool isDirectory() const;

  bool next();
  bool rewind();

  void setTimeCallback(time_t (*cb)(void));

protected:
  DirImplPtr _impl;
  FS *_baseFS;
  time_t (*_timeCallback)(void) = nullptr;
};

// Support > 4GB filesystems (SD, etc.)
struct FSInfo {
  uint64_t totalBytes;
  uint64_t usedBytes;
  size_t blockSize;
  size_t pageSize;
  size_t maxOpenFiles;
  size_t maxPathLength;
};

class FSConfig {
public:
  static constexpr uint32_t FSId = 0x00000000;

  FSConfig(uint32_t type = FSId, bool autoFormat = true) : _type(type), _autoFormat(autoFormat) {}

  FSConfig setAutoFormat(bool val = true) {
    _autoFormat = val;
    return *this;
  }

  uint32_t _type;
  bool _autoFormat;
};

class FS {
public:
  FS(FSImplPtr impl) : _impl(impl) {
    _timeCallback = _defaultTimeCB;
  }

  bool setConfig(const FSConfig &cfg);

  bool begin();
  void end();

  bool format();
  bool info(FSInfo &info);

  File open(const char *path, const char *mode);
  File open(const String &path, const char *mode);

  bool exists(const char *path);
  bool exists(const String &path);

  Dir openDir(const char *path);
  Dir openDir(const String &path);

  bool remove(const char *path);
  bool remove(const String &path);

  bool rename(const char *pathFrom, const char *pathTo);
  bool rename(const String &pathFrom, const String &pathTo);

  bool mkdir(const char *path);
  bool mkdir(const String &path);

  bool rmdir(const char *path);
  bool rmdir(const String &path);

  bool stat(const char *path, FSStat *st);
  bool stat(const String &path, FSStat *st);

  // Low-level FS routines, not needed by most applications
  bool gc();
  bool check();

  time_t getCreationTime();

  void setTimeCallback(time_t (*cb)(void));

  friend class ::SDClass;  // More of a frenemy, but SD needs internal implementation to get private FAT bits
protected:
  FSImplPtr _impl;
  FSImplPtr getImpl() {
    return _impl;
  }
  time_t (*_timeCallback)(void) = nullptr;
  static time_t _defaultTimeCB(void) {
    return time(nullptr);
  }
};

}  // namespace fs

extern "C" {
void close_all_fs(void);
void littlefs_request_end(void);
void spiffs_request_end(void);
}

#ifndef FS_NO_GLOBALS
using fs::Dir;
using fs::File;
using fs::FS;
using fs::FSConfig;
using fs::FSInfo;
using fs::SeekCur;
using fs::SeekEnd;
using fs::SeekMode;
using fs::SeekSet;
#endif  //FS_NO_GLOBALS
