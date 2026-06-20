#include "ui_help.h"

static const char* const help_goal[] = {
    "Go down to F18.",
    "Take Orb of Yonder.",
    "The Warden wakes.",
    "Climb to F1.",
    "Use F1 up stairs.",
    "Victory: Orb in daylight.",
};

static const char* const help_controls[] = {
    "D-pad: move/attack.",
    "Hold D-pad to walk.",
    "Ranger/Mage shoot lines.",
    "OK: rest/interact.",
    "OK long: Look.",
    "Back: game menu.",
    "Inventory: Left/Right tabs.",
};

static const char* const help_symbols[] = {
    "@ you    # grate",
    ". floor  + door",
    "- open   < up",
    "> down   ! potion",
    "? scroll / wand",
    ": throw  = charm",
    "% food   * gold",
    "\" grass  H chest",
    "& Orb    T shrine",
    "e eel M mimic L lurker",
    "letters: other foes",
};

static const char* const help_potions[] = {
    "Unknown: color label.",
    "Healing: +8 HP.",
    "Strength: +1 STR.",
    "Antidote clears poison.",
    "Ward blocks new fire.",
    "Flame bursts 3x3.",
    "Frost freezes water.",
    "Smoke blinds/confuses.",
};

static const char* const help_scrolls[] = {
    "Unknown: rune label.",
    "Identify: one item.",
    "Enchant weapon/armor.",
    "Fire Bloom is 3x3.",
    "Blink jumps to target.",
    "Mapping reveals floor.",
    "Reveal opens secrets.",
    "Charge fills a wand.",
    "Call wakes the level.",
};

static const char* const help_wands[] = {
    "Labels show charges.",
    "Empty wand stays held.",
    "Fire/Frost/Venom rays.",
    "Force pushes.",
    "Blink/Fear/Arc/Reveal.",
};

static const char* const help_classes[] = {
    "Warrior: HP, shield.",
    "Ranger: sees traps.",
    "Ranger uses arrows.",
    "Mage knows scrolls.",
    "Mage uses charges.",
    "Level 3/6: perks.",
};

static const char* const help_perks[] = {
    "L3 and L6: choose one.",
    "Auto bonuses still stay.",
    "5 class perks appear.",
    "OK opens perk card.",
    "Back returns to list.",
    "You must choose one.",
};

static const char* const help_gear[] = {
    "Gear tab holds gear.",
    "Stones/Darts stack.",
    "Throwables use one slot.",
    "One Charm can be worn.",
    "Charm names are unknown.",
    "Wear or Identify to learn.",
    "Chest: bump/OK nearby.",
    "Key/button opens grate.",
};

static const char* const help_hunger[] = {
    "HNGR weakens melee.",
    "STRV hurts over time.",
    "Start has 3 rations.",
    "Food restores hunger.",
    "Orb slows hunger.",
    "Empty Locket helps.",
    "Menu: Eat food.",
};

const char* help_topic_name(uint8_t topic) {
    static const char* names[HELP_TOPIC_COUNT] = {
        "Goal",
        "Controls",
        "Symbols",
        "Potions",
        "Scrolls",
        "Wands",
        "Classes",
        "Perks",
        "Gear",
        "Hunger",
    };
    return topic < HELP_TOPIC_COUNT ? names[topic] : "Help";
}

uint8_t help_line_count(uint8_t topic) {
    switch(topic) {
    case 0:
        return sizeof(help_goal) / sizeof(help_goal[0]);
    case 1:
        return sizeof(help_controls) / sizeof(help_controls[0]);
    case 2:
        return sizeof(help_symbols) / sizeof(help_symbols[0]);
    case 3:
        return sizeof(help_potions) / sizeof(help_potions[0]);
    case 4:
        return sizeof(help_scrolls) / sizeof(help_scrolls[0]);
    case 5:
        return sizeof(help_wands) / sizeof(help_wands[0]);
    case 6:
        return sizeof(help_classes) / sizeof(help_classes[0]);
    case 7:
        return sizeof(help_perks) / sizeof(help_perks[0]);
    case 8:
        return sizeof(help_gear) / sizeof(help_gear[0]);
    case 9:
        return sizeof(help_hunger) / sizeof(help_hunger[0]);
    default:
        return 0;
    }
}

const char* help_line(uint8_t topic, uint8_t line) {
    switch(topic) {
    case 0:
        return line < help_line_count(topic) ? help_goal[line] : "";
    case 1:
        return line < help_line_count(topic) ? help_controls[line] : "";
    case 2:
        return line < help_line_count(topic) ? help_symbols[line] : "";
    case 3:
        return line < help_line_count(topic) ? help_potions[line] : "";
    case 4:
        return line < help_line_count(topic) ? help_scrolls[line] : "";
    case 5:
        return line < help_line_count(topic) ? help_wands[line] : "";
    case 6:
        return line < help_line_count(topic) ? help_classes[line] : "";
    case 7:
        return line < help_line_count(topic) ? help_perks[line] : "";
    case 8:
        return line < help_line_count(topic) ? help_gear[line] : "";
    case 9:
        return line < help_line_count(topic) ? help_hunger[line] : "";
    default:
        return "";
    }
}
