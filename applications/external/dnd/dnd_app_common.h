#pragma once

/* Declaration dependencies needed by the common app core. These headers do
   not allocate runtime memory; final FAP ownership is controlled by each
   manifest source list plus linker section GC. Mode-only UI headers stay in
   dnd_app_hub.h / dnd_app_combat.h / dnd_app_grants.h. */
#include "dnd_data.h"
#include "dnd_character_collections.h"
#include "dnd_profile_handoff.h"
#include "dnd_settings.h"
#include "dnd_extra_items.h"
#include "dnd_fs.h"
#include "dnd_rules.h"
#include "dndolphins_rules_character.h"
#include "dndolphins_dice.h"
#include "dndolphins_weapon_combat.h"
#include "dndolphins_spells.h"
#include "dndolphins_spell_combat.h"
#include "dnd_storage.h"
#include "dndolphins_progression_store.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
