#pragma once
#include "dnd_plugin_loader.h"
#include "dndolphins_spell_combat.h"
#define DND_SPELL_DAMAGE_API_ID      "dnd_spell_damage"
#define DND_SPELL_DAMAGE_API_VERSION 1U
#define DND_SPELL_DAMAGE_COMBAT_PATH "/ext/apps_data/dndcombat/plugins/dnd_spell_damage.fal"
typedef struct {
    uint32_t size;
    bool (*damage_spec)(
        const DndSpell* spell,
        uint8_t cast_level,
        uint8_t character_level,
        int8_t modifier,
        DndSpellDamageSpec* output);
} DndSpellDamageApi;
