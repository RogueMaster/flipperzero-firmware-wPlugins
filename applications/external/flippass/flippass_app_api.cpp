#include "flippass_app_api.h"

#include <flipper_application/api_hashtable/api_hashtable.h>
#include <flipper_application/api_hashtable/compilesort.hpp>

/* Only each symbol's address goes into the table, so a placeholder declaration is
 * enough and the C headers never have to compile as C++. */
#define FLIPPASS_APP_API(name) extern "C" void name();
#include "flippass_app_api_symbols.h"
#undef FLIPPASS_APP_API

#define FLIPPASS_APP_API(name) \
    sym_entry{.hash = elf_gnu_hash(#name), .address = (uint32_t)(&name)},

static constexpr auto flippass_app_api_table = sort(create_array_t<sym_entry>(
#include "flippass_app_api_symbols.h"
    sym_entry{.hash = 0U, .address = 0U}));
#undef FLIPPASS_APP_API

static_assert(
    !has_hash_collisions(flippass_app_api_table),
    "FlipPass host API symbol hash collision");

constexpr HashtableApiInterface flippass_app_hashtable_api_interface{
    {0U, 0U, &elf_resolve_from_hashtable},
    flippass_app_api_table.cbegin(),
    flippass_app_api_table.cend(),
};

extern "C" const ElfApiInterface* const flippass_app_api_interface =
    &flippass_app_hashtable_api_interface;
