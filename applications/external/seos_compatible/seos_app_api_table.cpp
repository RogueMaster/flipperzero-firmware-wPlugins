/* Symbol table of the app's own API, for plugins to resolve against.
 *
 * The composite resolver tries the firmware's table first and this one after,
 * so a plugin can call both.
 */
#include <flipper_application/api_hashtable/api_hashtable.h>
#include <flipper_application/api_hashtable/compilesort.hpp>

#include "seos_app_api_table_i.h"

static_assert(!has_hash_collisions(app_api_table), "Detected API method hash collision!");

constexpr HashtableApiInterface application_hashtable_api_interface{
    {
        .api_version_major = 0,
        .api_version_minor = 0,
        .resolver_callback = &elf_resolve_from_hashtable,
    },
    app_api_table.cbegin(),
    app_api_table.cend(),
};

extern "C" const ElfApiInterface* const application_api_interface =
    &application_hashtable_api_interface;
