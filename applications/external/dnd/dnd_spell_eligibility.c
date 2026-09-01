#include "dnd_spell_eligibility.h"

#include <string.h>

uint8_t dnd_spell_eligibility_class_max_spell_level(const PocketClassLevel* class_level) {
    if(!class_level) return 0U;
    uint8_t level = class_level->level;
    const char* name = class_level->name;
    if(!strcmp(name, "Bard") || !strcmp(name, "Cleric") || !strcmp(name, "Druid") ||
       !strcmp(name, "Sorcerer") || !strcmp(name, "Wizard")) {
        uint8_t maximum = (level + 1U) / 2U;
        return maximum > 9U ? 9U : maximum;
    }
    if(!strcmp(name, "Artificer") || !strcmp(name, "Paladin") || !strcmp(name, "Ranger")) {
        uint8_t maximum = (level + 3U) / 4U;
        return maximum > 5U ? 5U : maximum;
    }
    if(!strcmp(name, "Warlock")) {
        uint8_t maximum = (level + 1U) / 2U;
        return maximum > 5U ? 5U : maximum;
    }
    if(class_level->spellcasting_mode == PocketSpellcastingThird) {
        if(level < 3U) return 0U;
        uint8_t maximum = (level + 5U) / 6U;
        return maximum > 4U ? 4U : maximum;
    }
    return 0U;
}
