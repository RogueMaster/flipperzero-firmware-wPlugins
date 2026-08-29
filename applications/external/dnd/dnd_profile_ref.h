#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

/* Resolve DNDolphins' persisted active character reference for companion FAPs.
   This reads only the canonical active-profile metadata. Callers should fall
   back to character 0 when it is unavailable or invalid. */
bool dnd_profile_ref_active(Storage* storage, uint32_t* profile);
