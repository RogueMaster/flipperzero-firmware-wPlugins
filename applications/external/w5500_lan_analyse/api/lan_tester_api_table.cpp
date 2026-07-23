#include <flipper_application/api_hashtable/api_hashtable.h>
#include <flipper_application/api_hashtable/compilesort.hpp>

/*
 * Symbol table of the LAN Tester app's private APIs, used by the composite
 * API resolver so embedded plugins can call the shared app/W5500 functions.
 */
#include "lan_tester_api_table_i.h"

static_assert(!has_hash_collisions(lan_tester_api_table), "Detected API method hash collision!");

constexpr HashtableApiInterface lan_tester_hashtable_api_interface{
    {
        .api_version_major = 0,
        .api_version_minor = 0,
        .resolver_callback = &elf_resolve_from_hashtable,
    },
    lan_tester_api_table.cbegin(),
    lan_tester_api_table.cend(),
};

extern "C" const ElfApiInterface* const lan_tester_api_interface =
    &lan_tester_hashtable_api_interface;
