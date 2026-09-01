#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

/* Shared character-selection and cross-FAP launch contract. All seven FAPs use
   this module so active-character resolution, data roots, launch paths and
   parent-return behavior have one source of truth. */
#define POCKET_D20_HANDOFF_LAUNCH_ARG         "initiative"
#define POCKET_D20_HANDOFF_LAUNCH_PREFIX      "initiative;"
#define POCKET_D20_HANDOFF_ADVENTURE_CONTINUE ";continue"
#define POCKET_D20_RETURN_FOCUS_INVENTORY     "focus=inventory"
#define POCKET_D20_RETURN_FOCUS_SPELLBOOK     "focus=spellbook"
#define POCKET_D20_RETURN_FOCUS_ADVENTURE     "focus=adventure"
#define POCKET_D20_RETURN_FOCUS_JOURNAL       "focus=journal"
#define POCKET_D20_RETURN_FOCUS_INITIATIVE    "focus=initiative"
#define POCKET_D20_RETURN_FOCUS_BESTIARY      "focus=bestiary"
#define DNDOLPHINS_FAP_PATH                   "/ext/apps/Games/dndolphins.fap"
#define DNDJOURNAL_FAP_PATH                   "/ext/apps/Games/dndjournal.fap"
#define DNDADVENTURE_FAP_PATH                 "/ext/apps/Games/dndadventure.fap"
#define DNDINITIATIVE_FAP_PATH                "/ext/apps/Games/dndinitiative.fap"
#define DNDINVENTORY_FAP_PATH                 "/ext/apps/Games/dndinventory.fap"
#define DNDSPELLBOOK_FAP_PATH                 "/ext/apps/Games/dndspellbook.fap"
#define DNDBESTIARY_FAP_PATH                  "/ext/apps/Games/dndbestiary.fap"
#define POCKET_D20_LAUNCH_ARGS_MAX            1536U
#define POCKET_D20_TRANSFER_MAX               23U
#define POCKET_D20_CHARACTER_DATA_ROOT        "/ext/apps_data/dndolphins"
#define POCKET_D20_JOURNAL_DATA_ROOT          "/ext/apps_data/dndjournal"

/* Read only DNDolphins' persisted Active= ID from custom_active_profile.txt.
   This lightweight reader does not scan, infer, validate or switch profiles. */
bool dnd_profile_ref_active_id(Storage* storage, uint32_t* profile);

/* Resolve only the persisted active character. No cross-character fallback. */
bool dnd_profile_ref_active_exact(Storage* storage, uint32_t* profile);

/* Resolve the canonical primary character file for a profile. Collection
   sidecars such as inventory_<id>.txt and spellbook_<id>.txt are excluded. */
bool dnd_profile_ref_path(Storage* storage, uint32_t profile, char* output, size_t size);

/* True only when this exact profile ID has a canonical primary character file. */
bool dnd_profile_ref_exists(Storage* storage, uint32_t profile);

/* Launch an already-known absolute FAP path after caller teardown. */
bool dnd_handoff_launch(const char* fap_path, const char* args);

/* Best-effort return/companion target: launch only when the target FAP exists. */
bool dnd_handoff_launch_if_present(const char* fap_path, const char* args);
