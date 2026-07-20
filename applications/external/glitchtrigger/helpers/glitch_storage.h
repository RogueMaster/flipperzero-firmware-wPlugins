#pragma once

#include "glitch_config.h"

/* Persist named parameter "profiles" to the SD card, and append a CSV log of
 * glitch hits. Every call is best-effort: a missing SD card or a corrupt file
 * just fails cleanly, never crashes. */

#define GLITCH_PROFILE_NAME_MAX 33 // 32 chars + NUL
#define GLITCH_PROFILE_MAX 40 // most profiles we list

bool glitch_storage_save_profile(const char* name, const GlitchParams* p);
bool glitch_storage_load_profile(const char* name, GlitchParams* p);
bool glitch_storage_delete_profile(const char* name);

/* Fill `names` (each GLITCH_PROFILE_NAME_MAX bytes) with profile names, sans
 * extension. Returns how many were written (<= max). */
size_t glitch_storage_list_profiles(char names[][GLITCH_PROFILE_NAME_MAX], size_t max);

/* Append one hit row to hits.csv (writes the header if the file is new).
 * `source` is a short tag, e.g. "auto" or "mark". */
void glitch_storage_log_hit(const GlitchParams* p, uint32_t shot_index, const char* source);
