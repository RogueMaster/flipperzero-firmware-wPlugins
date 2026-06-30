#pragma once

#include "lib/String.hpp"
#include <toolbox/path.h>
#include <storage/storage.h>

#include "FlipperFile.hpp"
#include "Directory.hpp"

class FileManager {
private:
    Storage* storage;

public:
    FileManager() {
        storage = (Storage*)furi_record_open(RECORD_STORAGE);
    }

    void CreateDirIfNotExists(const char* path) {
        if(!storage_dir_exists(storage, path)) {
            storage_common_mkdir(storage, path);
        }
    }

    Directory* OpenDirectory(const char* path) {
        Directory* dir = new Directory(storage, path);
        if(dir->IsOpened()) {
            return dir;
        }
        delete dir;
        return NULL;
    }

    FlipperFile* OpenRead(const char* path) {
        FlipperFile* file = new FlipperFile(storage, path, false);
        if(file->IsOpened()) {
            return file;
        }
        delete file;
        return NULL;
    }

    FlipperFile* OpenRead(const char* dir, const char* file) {
        String concatedPath = String("%s/%s", dir, file);
        return OpenRead(concatedPath.cstr());
    }

    FlipperFile* OpenWrite(const char* path) {
        FlipperFile* file = new FlipperFile(storage, path, true);
        if(file->IsOpened()) {
            return file;
        }
        delete file;
        return NULL;
    }

    FlipperFile* OpenWrite(const char* dir, const char* file) {
        String concatedPath = String("%s/%s", dir, file);
        return OpenWrite(concatedPath.cstr());
    }

    void DeleteFile(const char* dir, const char* file) {
        String concatedPath = String("%s/%s", dir, file);
        DeleteFile(concatedPath.cstr());
    }

    void DeleteFile(const char* filePath) {
        storage_common_remove(storage, filePath);
    }

    ~FileManager() {
        furi_record_close(RECORD_STORAGE);
    }
};
