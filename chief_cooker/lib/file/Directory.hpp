#pragma once

#include <toolbox/path.h>
#include <storage/storage.h>

class Directory {
private:
    bool isOpened;
    Storage* storage;
    File* dir;

public:
    Directory(Storage* storage, const char* dirPath) {
        this->storage = storage;
        dir = storage_file_alloc(storage);
        isOpened = storage_dir_open(dir, dirPath);
    }

    bool IsOpened() {
        return isOpened;
    }

    void Rewind() {
        if(isOpened) {
            storage_dir_rewind(dir);
        }
    }

    bool GetNextFile(char* name, uint16_t nameLength) {
        if(!isOpened) {
            return false;
        }

        FileInfo fileInfo = FileInfo();
        do {
            if(!storage_dir_read(dir, &fileInfo, name, nameLength)) {
                return false;
            }
        } while((fileInfo.flags & FSF_DIRECTORY) > 0); // dir

        return true;
    }

    bool GetNextDir(char* name, uint16_t nameLength) {
        if(!isOpened) {
            return false;
        }

        FileInfo fileInfo = FileInfo();
        do {
            if(!storage_dir_read(dir, &fileInfo, name, nameLength)) {
                return false;
            }
        } while((fileInfo.flags & FSF_DIRECTORY) == 0); // not dir

        return true;
    }

    ~Directory() {
        storage_dir_close(dir);
        storage_file_free(dir);
    }
};
