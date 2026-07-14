#pragma once

#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of distinct placeholders tracked for a single payload.
#define PLACEHOLDER_MAX       16
// Maximum length of a placeholder name (between the brackets).
#define PLACEHOLDER_NAME_MAX  32
// Maximum length of a substituted value.
#define PLACEHOLDER_VALUE_MAX 128

typedef struct {
    FuriString* name; // placeholder name, e.g. "host" (without brackets)
    FuriString* value; // resolved value, empty until set
    bool resolved; // true once a value has been assigned
} PlaceholderEntry;

typedef struct {
    PlaceholderEntry entries[PLACEHOLDER_MAX];
    size_t count;
} PlaceholderMap;

// Lifecycle ----------------------------------------------------------------
void placeholder_map_init(PlaceholderMap* map);
void placeholder_map_reset(PlaceholderMap* map); // frees strings, count -> 0

// Returns the number of placeholders, and how many are still unresolved.
size_t placeholder_map_unresolved_count(const PlaceholderMap* map);

// Marks every placeholder unresolved and clears its value, keeping the names.
void placeholder_map_clear_values(PlaceholderMap* map);

// Scanning -----------------------------------------------------------------
// Scans the payload file for unique [name] tokens (first-seen order) and
// populates `map`. Existing entries are cleared first. Returns the number of
// distinct placeholders found (0 if none / file unreadable).
size_t placeholder_scan_file(Storage* storage, const char* path, PlaceholderMap* map);

// Substitution -------------------------------------------------------------
// Replaces every [name] in `line` with its resolved value, in place.
// Unresolved or unknown placeholders are left untouched.
void placeholder_apply(FuriString* line, const PlaceholderMap* map);

// Value access -------------------------------------------------------------
// Sets the value for the placeholder at `index` (marks it resolved).
void placeholder_set_value(PlaceholderMap* map, size_t index, const char* value);

// Config persistence (per-payload) -----------------------------------------
// Configs live at: /ext/apps_data/badusb_ph/configs/<payload_basename>/<name>.cfg
// Each file is a FlipperFormat with one `name: value` line per placeholder.

// Lists saved config names for `payload_basename` into `out_names` (allocated
// FuriStrings appended by the caller's FuriString array is not used here; we
// take a callback-free simple approach via a FuriString list). Returns count.
size_t placeholder_config_list(
    Storage* storage,
    const char* payload_basename,
    FuriString* out_names[],
    size_t max_names);

// Loads a named config into `map`, marking matching placeholders resolved.
// Placeholders present in `map` but absent from the config stay unresolved.
// Returns true if the config file was read.
bool placeholder_config_load(
    Storage* storage,
    const char* payload_basename,
    const char* config_name,
    PlaceholderMap* map);

// Saves all currently-resolved placeholders in `map` as a named config.
// Returns true on success.
bool placeholder_config_save(
    Storage* storage,
    const char* payload_basename,
    const char* config_name,
    const PlaceholderMap* map);

#ifdef __cplusplus
}
#endif
