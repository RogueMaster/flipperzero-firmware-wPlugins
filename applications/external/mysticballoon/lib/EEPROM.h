#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <furi.h>
#include <storage/storage.h>
#include "../game/save_layout.h"

#define EEPROM_LIB_PATH APP_DATA_PATH("eeprom.bin")

class EEPROMClass {
public:
    static constexpr int kSize = MyblSave::kSize;
    static constexpr size_t kPathSize = 128;

    EEPROMClass()
        : loaded_(false)
        , dirty_(false)
        , path_resolved_(false) {
        memset(mem_, 0x00, kSize);
        memset(file_path_, 0x00, kPathSize);
        strncpy(file_path_, EEPROM_LIB_PATH, kPathSize - 1);
    }

    void begin() {
        ensureLoaded_();
    }

    int length() const {
        return kSize;
    }

    uint8_t read(int addr) const {
        ensureLoaded_();
        if(addr < 0 || addr >= kSize) return 0;
        return mem_[addr];
    }

    void write(int addr, uint8_t value) {
        ensureLoaded_();
        if(addr < 0 || addr >= kSize) return;
        mem_[addr] = value;
        dirty_ = true;
    }

    void update(int addr, uint8_t value) {
        ensureLoaded_();
        if(addr < 0 || addr >= kSize) return;
        if(mem_[addr] != value) {
            mem_[addr] = value;
            dirty_ = true;
        }
    }

    template <typename T>
    T& get(int addr, T& out) const {
        ensureLoaded_();
        if(addr < 0 || addr + (int)sizeof(T) > kSize) return out;
        memcpy(&out, mem_ + addr, sizeof(T));
        return out;
    }

    template <typename T>
    const T& put(int addr, const T& in) {
        ensureLoaded_();
        if(addr < 0 || addr + (int)sizeof(T) > kSize) return in;

        bool changed = false;
        const uint8_t* src = reinterpret_cast<const uint8_t*>(&in);
        for(size_t i = 0; i < sizeof(T); i++) {
            int a = addr + (int)i;
            if(mem_[a] != src[i]) {
                mem_[a] = src[i];
                changed = true;
            }
        }
        if(changed) dirty_ = true;
        return in;
    }

    void clear(uint8_t value = 0) {
        ensureLoaded_();
        memset(mem_, value, kSize);
        dirty_ = true;
    }

    bool commit() {
        ensureLoaded_();
        if(!dirty_) return true;

        const bool ok = writeFile_();
        if(ok) dirty_ = false;
        return ok;
    }

    bool isDirty() const {
        return dirty_;
    }

private:
    bool resolvePathIfNeeded_(Storage* storage) const {
        if(path_resolved_) return true;
        if(!storage) return false;

        FuriString* path = furi_string_alloc_set_str(file_path_);
        if(!path) return false;

        storage_common_resolve_path_and_ensure_app_directory(storage, path);
        const char* resolved = furi_string_get_cstr(path);

        bool ok = false;
        if(resolved && resolved[0]) {
            const size_t len = strlen(resolved);
            if(len < kPathSize) {
                memcpy(file_path_, resolved, len + 1);
                path_resolved_ = true;
                ok = true;
            }
        }

        furi_string_free(path);
        return ok;
    }

    static void ensureDefaultDir_(Storage* storage) {
        if(!storage) return;
        (void)storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    }

    void ensureLoaded_() const {
        if(loaded_) return;

        Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
        if(!storage) {
            return;
        }

        (void)resolvePathIfNeeded_(storage);
        ensureDefaultDir_(storage);

        File* file = storage_file_alloc(storage);
        if(!file) {
            furi_record_close(RECORD_STORAGE);
            return;
        }

        memset(mem_, 0x00, kSize);
        bool ok = storage_file_open(file, file_path_, FSAM_READ_WRITE, FSOM_OPEN_ALWAYS);
        if(ok) {
            const uint64_t file_size = storage_file_size(file);
            bool need_rewrite = false;

            (void)storage_file_seek(file, 0, true);
            const size_t rd = storage_file_read(file, mem_, kSize);
            if(rd < (size_t)kSize) {
                need_rewrite = true;
            }

            if(file_size != (uint64_t)kSize) {
                need_rewrite = true;
            }

            if(need_rewrite) {
                (void)storage_file_seek(file, 0, true);
                const size_t wr = storage_file_write(file, mem_, kSize);
                if(wr == (size_t)kSize) {
                    (void)storage_file_truncate(file);
                    (void)storage_file_sync(file);
                }
            }
            (void)storage_file_close(file);
        } else {
            (void)storage_file_close(file);
        }

        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);

        loaded_ = true;
        dirty_ = false;
    }

    bool writeFile_() const {
        Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
        if(!storage) return false;

        if(!path_resolved_ && !resolvePathIfNeeded_(storage)) {
            furi_record_close(RECORD_STORAGE);
            return false;
        }

        ensureDefaultDir_(storage);

        File* file = storage_file_alloc(storage);
        if(!file) {
            furi_record_close(RECORD_STORAGE);
            return false;
        }

        bool ok = storage_file_open(file, file_path_, FSAM_READ_WRITE, FSOM_OPEN_ALWAYS);
        bool success = false;

        if(ok) {
            (void)storage_file_seek(file, 0, true);
            size_t wr = storage_file_write(file, mem_, kSize);
            (void)storage_file_truncate(file);
            (void)storage_file_sync(file);
            (void)storage_file_close(file);
            success = (wr == (size_t)kSize);
        } else {
            (void)storage_file_close(file);
        }

        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return success;
    }

private:
    mutable uint8_t mem_[kSize];
    mutable bool loaded_;
    mutable bool dirty_;
    mutable bool path_resolved_;

    mutable char file_path_[kPathSize];
};

#if (__cplusplus >= 201703L)
inline EEPROMClass EEPROM;
#else
extern EEPROMClass EEPROM;
#ifdef EEPROM_DEFINE_INSTANCE
EEPROMClass EEPROM;
#endif
#endif
