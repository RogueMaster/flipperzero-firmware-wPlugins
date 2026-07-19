/**
 * Gauntlet - SD-card storage layer.
 *
 * Discovers .pack files under /ext/apps_data/gauntlet/packs, loads one on
 * demand, and persists progress (score + which challenge ids are solved) to
 * /ext/apps_data/gauntlet/progress.save. On first run it drops a bundled
 * starter pack onto the card so the app is playable out of the box and shows
 * authors the file format by example.
 */
#pragma once

#include <storage/storage.h>
#include "ctf_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAUNTLET_BASE_DIR     EXT_PATH("apps_data/gauntlet")
#define GAUNTLET_PACKS_DIR    EXT_PATH("apps_data/gauntlet/packs")
#define GAUNTLET_PROGRESS     EXT_PATH("apps_data/gauntlet/progress.save")

#define CTF_MAX_PACKS  24
#define CTF_MAX_SOLVED 256

typedef struct {
    uint32_t score;
    uint32_t solved[CTF_MAX_SOLVED];
    uint16_t solved_count;
} CtfProgress;

/** Make sure the packs dir exists and, if empty, write the starter pack. */
void ctf_store_ensure_starter(Storage* storage);

/** Enumerate .pack files into infos[] (up to max). Returns how many found. */
uint8_t ctf_store_scan(Storage* storage, CtfPackInfo* infos, uint8_t max);

/** Read one pack file (base name inside the packs dir) and parse it. */
bool ctf_store_load_pack(Storage* storage, const char* filename, CtfPack* out);

/* --- progress ------------------------------------------------------------ */
void ctf_store_progress_load(Storage* storage, CtfProgress* p);
void ctf_store_progress_save(Storage* storage, const CtfProgress* p);
bool ctf_progress_is_solved(const CtfProgress* p, uint32_t id);
/** Record a solve. Returns true and adds points only if it was not solved. */
bool ctf_progress_mark_solved(CtfProgress* p, uint32_t id, uint16_t points);
void ctf_progress_reset(CtfProgress* p);
/** How many of a pack's challenges are already solved. */
uint8_t ctf_progress_pack_solved(const CtfProgress* p, const CtfPack* pack);

/** A rank title for a score ("Rookie" ... "Legend"). Pure. */
const char* ctf_rank(uint32_t score);

#ifdef __cplusplus
}
#endif
