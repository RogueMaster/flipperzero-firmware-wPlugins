#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

/* Resolve DNDolphins' persisted active character reference for companion FAPs.
   This reads only the canonical active-profile metadata and validates that the
   referenced primary character file exists. */
bool dnd_profile_ref_active(Storage* storage, uint32_t* profile);

/* Resolve the canonical primary character file for a profile. Collection files
   such as inventory_<id>.txt and spellbook_<id>.txt are deliberately excluded. */
bool dnd_profile_ref_path(Storage* storage, uint32_t profile, char* output, size_t size);
