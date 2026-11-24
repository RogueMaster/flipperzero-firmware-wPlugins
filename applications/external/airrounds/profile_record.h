#pragma once

#include "shared/target.h"

namespace airrounds {
namespace flipper {

struct ProfileRecord {
    uint8_t bytes[34];
};

struct StoredProfile {
    session::Profile points;
    uint32_t generation = 0;
    bool valid = false;
};

inline bool editProfile(
    const session::Profile& current,
    size_t slot,
    const session::Target* replacement,
    session::Profile& updated) {
    if(slot > current.size() || (slot == current.size() && replacement == nullptr)) {
        return false;
    }

    if(slot == current.size() && current.size() >= session::Profile::legacyCapacity) {
        return false;
    }
    session::Profile next;
    for(size_t index = 0; index < current.size(); ++index) {
        const auto* target = index == slot ? replacement : &current[index];
        if(target != nullptr && !next.add(*target)) {
            return false;
        }
    }
    if(slot == current.size() && !next.add(*replacement)) {
        return false;
    }
    updated = next;
    return true;
}

inline uint32_t profileWord(const uint8_t* data) {
    uint32_t value = 0;
    for(size_t i = 0; i < 4; ++i) {
        value = (value << 8U) | data[i];
    }
    return value;
}

inline void putProfileWord(uint8_t* data, uint32_t value) {
    for(size_t i = 4; i > 0; --i) {
        data[i - 1] = static_cast<uint8_t>(value);
        value >>= 8U;
    }
}

inline uint32_t profileChecksum(const ProfileRecord& record) {
    uint32_t hash = 2166136261U;
    for(size_t i = 0; i < sizeof(record.bytes) - 4; ++i) {
        hash = (hash ^ record.bytes[i]) * 16777619U;
    }
    return hash;
}

inline ProfileRecord encodeProfile(const session::Profile& profile, uint32_t generation) {
    ProfileRecord record{};
    memcpy(record.bytes, "ARP1", 4);
    putProfileWord(record.bytes + 4, generation);
    if(profile.size() > session::Profile::legacyCapacity) {
        return {};
    }
    record.bytes[8] = static_cast<uint8_t>(profile.size());
    for(size_t index = 0; index < profile.size(); ++index) {
        auto* field = record.bytes + 9 + index * 7;
        memcpy(field, profile[index].bssid, 6);
        field[6] = profile[index].channel;
    }
    putProfileWord(record.bytes + sizeof(record.bytes) - 4, profileChecksum(record));
    return record;
}

inline StoredProfile decodeProfile(const ProfileRecord& record) {
    const size_t count = record.bytes[8];
    if(memcmp(record.bytes, "ARP1", 4) != 0 || count > session::Profile::legacyCapacity ||
       profileWord(record.bytes + sizeof(record.bytes) - 4) != profileChecksum(record)) {
        return {};
    }
    StoredProfile result;
    for(size_t i = 0; i < count; ++i) {
        const auto* field = record.bytes + 9 + i * 7;
        session::Target target{{}, field[6]};
        memcpy(target.bssid, field, sizeof(target.bssid));
        if(!result.points.add(target)) {
            return {};
        }
    }
    result.generation = profileWord(record.bytes + 4);
    result.valid = true;
    return result;
}

inline int newestProfile(const StoredProfile& first, const StoredProfile& second) {
    if(!first.valid) {
        return second.valid ? 1 : -1;
    }
    const uint32_t difference = second.generation - first.generation;
    return second.valid && difference != 0 && difference < 0x80000000U ? 1 : 0;
}

} // namespace flipper
} // namespace airrounds
