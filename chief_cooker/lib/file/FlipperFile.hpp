#pragma once

#include "flipper_format.h"
#include "lib/String.hpp"

class FlipperFile {
private:
    bool isOpened;
    FlipperFormat* flipperFormat;

public:
    FlipperFile(Storage* storage, const char* path, bool write) {
        flipperFormat = flipper_format_file_alloc(storage);
        if(write) {
            isOpened = flipper_format_file_open_always(flipperFormat, path);
        } else {
            isOpened = flipper_format_file_open_existing(flipperFormat, path);
        }
    }

    bool IsOpened() {
        return isOpened;
    }

    bool ReadUInt32(const char* key, uint32_t* valueTarget) {
        return flipper_format_read_uint32(flipperFormat, key, valueTarget, 1);
    }

    bool ReadBool(const char* key, bool* valueTarget) {
        return flipper_format_read_bool(flipperFormat, key, valueTarget, 1);
    }

    bool ReadString(const char* key, String* valueTarget) {
        return flipper_format_read_string(flipperFormat, key, valueTarget->furiString());
    }

    bool ReadHex(const char* key, uint64_t* value) {
        return flipper_format_read_hex(flipperFormat, key, (uint8_t*)value, sizeof(value));
    }

    bool WriteUInt32(const char* key, uint32_t value) {
        return flipper_format_write_uint32(flipperFormat, key, &value, 1);
    }

    bool WriteBool(const char* key, bool value) {
        return flipper_format_write_bool(flipperFormat, key, &value, 1);
    }

    bool WriteString(const char* key, const char* value) {
        return flipper_format_write_string_cstr(flipperFormat, key, value);
    }

    bool WriteHex(const char* key, uint64_t value) {
        return flipper_format_write_hex(flipperFormat, key, (const uint8_t*)&value, sizeof(value));
    }

    ~FlipperFile() {
        flipper_format_file_close(flipperFormat);
        flipper_format_free(flipperFormat);
    }
};
