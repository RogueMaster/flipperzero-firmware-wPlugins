#pragma once

#include <storage/storage.h>

#include "profile_record.h"

namespace airrounds {
namespace flipper {

enum class ProfileSave {
    saved,
    unavailable,
    changed
};

class ProfileStorage {
public:
    explicit ProfileStorage(Storage* storage)
        : storage_(storage) {
    }

    session::Profile load() const {
        const Slot slots[]{read(0), read(1)};
        const int latest = newestProfile(slots[0].stored, slots[1].stored);
        return latest < 0 ? session::Profile{} : slots[latest].stored.points;
    }

    ProfileSave save(const session::Profile& expected, const session::Profile& profile) const {
        if(profile.size() > session::Profile::legacyCapacity) {
            return ProfileSave::unavailable;
        }
        // Recheck both files. The card may have returned since the app opened.
        const Slot slots[]{read(0), read(1)};
        if(!slots[0].readable || !slots[1].readable) {
            return ProfileSave::unavailable;
        }
        const int latest = newestProfile(slots[0].stored, slots[1].stored);
        const auto onCard = latest < 0 ? session::Profile{} : slots[latest].stored.points;
        if(!onCard.matches(expected)) {
            return ProfileSave::changed;
        }
        const int slot = latest == 0 ? 1 : 0;
        const uint32_t generation = latest < 0 ? 1U : slots[latest].stored.generation + 1U;
        const auto record = encodeProfile(profile, generation);

        File* file = storage_file_alloc(storage_);
        const bool opened = storage_file_open(file, path(slot), FSAM_WRITE, FSOM_CREATE_ALWAYS);
        const bool written = opened &&
                             storage_file_write(file, record.bytes, sizeof(record.bytes)) ==
                                 sizeof(record.bytes) &&
                             storage_file_sync(file);
        const bool closed = storage_file_close(file);
        storage_file_free(file);
        if(!written || !closed) {
            if(opened) {
                storage_common_remove(storage_, path(slot));
            }
            return ProfileSave::unavailable;
        }
        const auto result = read(slot);
        if(!result.stored.valid || result.stored.generation != generation ||
           !result.stored.points.matches(profile)) {
            storage_common_remove(storage_, path(slot));
            return ProfileSave::unavailable;
        }
        return ProfileSave::saved;
    }

private:
    struct Slot {
        StoredProfile stored{};
        bool readable = false;
    };

    static const char* path(int slot) {
        return slot == 0 ? APP_DATA_PATH("profile-a") : APP_DATA_PATH("profile-b");
    }

    Slot read(int slot) const {
        ProfileRecord record{};
        Slot result;
        File* file = storage_file_alloc(storage_);
        const bool opened = storage_file_open(file, path(slot), FSAM_READ, FSOM_OPEN_EXISTING);
        if(!opened) {
            result.readable = storage_file_get_error(file) == FSE_NOT_EXIST;
        } else {
            const auto size = storage_file_size(file);
            if(storage_file_get_error(file) == FSE_OK) {
                // A readable file with the wrong size is a damaged record, not an I/O failure.
                const bool correctSize = size == sizeof(record.bytes);
                result.readable = !correctSize ||
                                  storage_file_read(file, record.bytes, sizeof(record.bytes)) ==
                                      sizeof(record.bytes);
                if(correctSize && result.readable) {
                    result.stored = decodeProfile(record);
                }
            }
        }
        const bool closed = storage_file_close(file);
        storage_file_free(file);
        return opened && !closed ? Slot{} : result;
    }

    Storage* storage_;
};

} // namespace flipper
} // namespace airrounds
