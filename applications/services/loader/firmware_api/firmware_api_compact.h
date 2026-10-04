#pragma once

#include <flipper_application/api_hashtable/api_hashtable.h>

#include <array>
#include <cstddef>
#include <cstdint>

// Only the resident firmware uses this representation. The public hashtable
// layout and the tables embedded in external applications remain unchanged.
constexpr size_t FirmwareApiBlockEntries = 16;
constexpr size_t FirmwareApiBlockBytes = 4 + 3 * (FirmwareApiBlockEntries - 1);
constexpr uint32_t FirmwareApiMaxDelta = 0x00FFFFFF;

template <size_t N>
constexpr bool firmware_api_hashes_packable(const std::array<sym_entry, N>& entries) {
    if(N == 0) return false;
    for(size_t i = 1; i < N; ++i) {
        if(entries[i].hash <= entries[i - 1].hash) return false;
        if((i % FirmwareApiBlockEntries) &&
           (entries[i].hash - entries[i - 1].hash > FirmwareApiMaxDelta)) {
            return false;
        }
    }
    return true;
}

template <size_t N>
struct FirmwareCompactApiTable {
    static_assert(N > 0, "Firmware API table must contain an entry");
    static constexpr size_t BlockCount =
        (N + FirmwareApiBlockEntries - 1) / FirmwareApiBlockEntries;
    // Every entry takes three bytes, plus one extra byte for each block's
    // first full hash. The final block contains only its actual entries.
    std::array<uint8_t, N * 3 + BlockCount> hashes{};
    // Leave addresses uncompressed so the linker emits ordinary relocations
    // and preserves the full pointer, including the Thumb bit and RAM address.
    std::array<Elf32_Addr, N> addresses{};
};

template <size_t N>
constexpr FirmwareCompactApiTable<N>
    firmware_api_pack(const std::array<sym_entry, N>& entries) {
    FirmwareCompactApiTable<N> result{};
    size_t output = 0;
    for(size_t i = 0; i < N; ++i) {
        const bool first = (i % FirmwareApiBlockEntries) == 0;
        uint32_t value = first ? entries[i].hash : entries[i].hash - entries[i - 1].hash;
        for(size_t byte = 0; byte < (first ? 4U : 3U); ++byte) {
            result.hashes[output++] = static_cast<uint8_t>(value);
            value >>= 8;
        }
        result.addresses[i] = entries[i].address;
    }
    return result;
}

struct FirmwareCompactApiInterface : public ElfApiInterface {
    const uint8_t* hashes;
    const Elf32_Addr* addresses;
    size_t entry_count;
};

inline uint32_t firmware_api_read_hash(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

inline bool firmware_api_find_symbol(
    const FirmwareCompactApiInterface* interface,
    uint32_t hash,
    Elf32_Addr* address) {
    if(interface->entry_count == 0) return false;

    // Find the last block whose first full hash does not exceed the query.
    size_t begin = 0;
    size_t end =
        (interface->entry_count + FirmwareApiBlockEntries - 1) / FirmwareApiBlockEntries;
    while(begin < end) {
        const size_t middle = begin + (end - begin) / 2;
        const uint32_t first =
            firmware_api_read_hash(interface->hashes + middle * FirmwareApiBlockBytes);
        if(first <= hash) {
            begin = middle + 1;
        } else {
            end = middle;
        }
    }
    if(begin == 0) return false;

    const size_t block = begin - 1;
    size_t entry = block * FirmwareApiBlockEntries;
    const uint8_t* encoded = interface->hashes + block * FirmwareApiBlockBytes;
    uint32_t candidate = firmware_api_read_hash(encoded);
    encoded += 4;
    const size_t block_end =
        std::min(entry + FirmwareApiBlockEntries, interface->entry_count);

    while(true) {
        if(candidate == hash) {
            *address = interface->addresses[entry];
            return true;
        }
        if(candidate > hash || ++entry == block_end) return false;
        candidate += static_cast<uint32_t>(encoded[0]) |
                     (static_cast<uint32_t>(encoded[1]) << 8) |
                     (static_cast<uint32_t>(encoded[2]) << 16);
        encoded += 3;
    }
}
