#include "firmware_api.h"
#include "firmware_api_compact.h"

#include <flipper_application/api_hashtable/api_hashtable.h>
#include <flipper_application/api_hashtable/compilesort.hpp>

/* Generated table */
#include <firmware_api_table.h>

#include <furi_hal_info.h>
#include <furi.h>

static_assert(!has_hash_collisions(elf_api_table), "Detected API method hash collision!");
static_assert(
    firmware_api_hashes_packable(elf_api_table),
    "Firmware API hashes are unsorted or a block delta exceeds 24 bits; adjust the private encoding");

static constexpr auto firmware_compact_api_table = firmware_api_pack(elf_api_table);

static bool firmware_api_resolve(
    const ElfApiInterface* interface,
    uint32_t hash,
    Elf32_Addr* address) {
    furi_check(interface);
    furi_check(address);
    const auto* compact_interface = static_cast<const FirmwareCompactApiInterface*>(interface);
    const bool result = firmware_api_find_symbol(compact_interface, hash, address);
    if(!result) {
        FURI_LOG_T(
            "ApiHashtable",
            "Can't find symbol with hash %lx @ %p!",
            hash,
            compact_interface->hashes);
    }
    return result;
}

constexpr FirmwareCompactApiInterface elf_api_interface{
    {
        .api_version_major = (elf_api_version >> 16),
        .api_version_minor = (elf_api_version & 0xFFFF),
        .resolver_callback = &firmware_api_resolve,
    },
    firmware_compact_api_table.hashes.data(),
    firmware_compact_api_table.addresses.data(),
    elf_api_table.size(),
};
const ElfApiInterface* const firmware_api_interface = &elf_api_interface;

extern "C" void furi_hal_info_get_api_version(uint16_t* major, uint16_t* minor) {
    *major = firmware_api_interface->api_version_major;
    *minor = firmware_api_interface->api_version_minor;
}
