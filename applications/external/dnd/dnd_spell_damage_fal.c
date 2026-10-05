#include "dnd_spell_damage_api.h"
static const DndSpellDamageApi dnd_spell_damage_api = {
    .size = sizeof(DndSpellDamageApi),
    .damage_spec = dndolphins_spell_combat_damage_spec,
};
static const FlipperAppPluginDescriptor dnd_spell_damage_descriptor = {
    .appid = DND_SPELL_DAMAGE_API_ID,
    .ep_api_version = DND_SPELL_DAMAGE_API_VERSION,
    .entry_point = &dnd_spell_damage_api,
};
const FlipperAppPluginDescriptor* dnd_spell_damage_ep(void) {
    return &dnd_spell_damage_descriptor;
}
