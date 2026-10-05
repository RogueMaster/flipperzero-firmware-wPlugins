#include "dndolphins_menu_graphics.h"
#include "dndolphins_icons.h"

#include <stddef.h>

static const Icon* const dndolphins_menu_icons[] = {
    &I_menu_characters_25x25,
    &I_menu_character_25x25,
    &I_menu_vitals_25x25,
    &I_menu_abilities_saves_25x25,
    &I_menu_skills_25x25,
    &I_menu_features_perks_25x25,
    &I_menu_inventory_25x25,
    &I_menu_magic_spells_25x25,
    &I_menu_bestiary_25x25,
    &I_menu_initiative_25x25,
    &I_menu_combat_25x25,
    &I_menu_dice_roller_25x25,
    &I_menu_adventure_25x25,
    &I_menu_journal_25x25,
    &I_menu_settings_25x25,
};

void dndolphins_menu_graphics_draw_icon(Canvas* canvas, uint16_t index, int32_t x, int32_t y) {
    if(!canvas) return;
    size_t count = sizeof(dndolphins_menu_icons) / sizeof(dndolphins_menu_icons[0]);
    const Icon* icon = index < count ? dndolphins_menu_icons[index] : &I_menu_settings_25x25;
    canvas_draw_icon(canvas, x, y, icon);
}
