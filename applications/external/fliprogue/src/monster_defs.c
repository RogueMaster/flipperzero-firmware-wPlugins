#include "monster_defs.h"

static const FrMonsterDef monster_defs[FR_MON_MAX + 1] = {
    [FR_MON_RAT] = {3, 1, false},
    [FR_MON_BAT] = {4, 1, false},
    [FR_MON_SNAKE] = {3, 2, false},
    [FR_MON_GOBLIN] = {6, 3, false},
    [FR_MON_ARCHER] = {5, 2, false},
    [FR_MON_SLIME] = {10, 2, false},
    [FR_MON_WISP] = {3, 2, false},
    [FR_MON_KOBOLD] = {5, 2, false},
    [FR_MON_OGRE] = {14, 5, false},
    [FR_MON_CUBE] = {10, 2, false},
    [FR_MON_DRAGON] = {22, 6, false},
    [FR_MON_YONDER_WARDEN] = {199, 14, false},
    [FR_MON_WIGHT] = {8, 3, true},
    [FR_MON_EEL] = {5, 3, false},
    [FR_MON_MIMIC] = {14, 6, false},
    [FR_MON_LURKER] = {6, 3, true},
};

static const uint8_t monster_pool[][8] = {
    {FR_MON_RAT, FR_MON_RAT, FR_MON_RAT, FR_MON_RAT, FR_MON_KOBOLD, FR_MON_KOBOLD, FR_MON_BAT, FR_MON_BAT},
    {FR_MON_SNAKE, FR_MON_SNAKE, FR_MON_KOBOLD, FR_MON_KOBOLD, FR_MON_GOBLIN, FR_MON_GOBLIN, FR_MON_BAT, FR_MON_BAT},
    {FR_MON_GOBLIN, FR_MON_GOBLIN, FR_MON_ARCHER, FR_MON_ARCHER, FR_MON_WISP, FR_MON_WISP, FR_MON_BAT, FR_MON_BAT},
    {FR_MON_SLIME, FR_MON_SLIME, FR_MON_ARCHER, FR_MON_ARCHER, FR_MON_BAT, FR_MON_BAT, FR_MON_WIGHT, FR_MON_WIGHT},
    {FR_MON_OGRE, FR_MON_OGRE, FR_MON_CUBE, FR_MON_CUBE, FR_MON_WISP, FR_MON_WISP, FR_MON_WIGHT, FR_MON_WIGHT},
    {FR_MON_OGRE, FR_MON_OGRE, FR_MON_CUBE, FR_MON_CUBE, FR_MON_ARCHER, FR_MON_DRAGON, FR_MON_WIGHT, FR_MON_WIGHT},
};

const FrMonsterDef* fr_monster_def(uint8_t type) {
    if(type == 0 || type > FR_MON_MAX || monster_defs[type].hp == 0) return &monster_defs[FR_MON_RAT];
    return &monster_defs[type];
}

uint8_t fr_monster_tier_for_floor(uint8_t floor) {
    uint8_t tier = (uint8_t)((floor - 1) / 3);
    return tier > 5 ? 5 : tier;
}

uint8_t fr_monster_from_tier(uint8_t tier, uint8_t roll) {
    if(tier > 5) tier = 5;
    return monster_pool[tier][roll % 8];
}

uint8_t fr_monster_chase_chance(uint8_t type) {
    switch(type) {
    case FR_MON_RAT:
    case FR_MON_BAT:
    case FR_MON_SNAKE:
        return 30;
    case FR_MON_SLIME:
        return 60;
    case FR_MON_YONDER_WARDEN:
    case FR_MON_GOBLIN:
    case FR_MON_ARCHER:
    case FR_MON_KOBOLD:
    case FR_MON_OGRE:
    case FR_MON_CUBE:
    case FR_MON_WISP:
    case FR_MON_DRAGON:
    case FR_MON_WIGHT:
    case FR_MON_EEL:
        return 100;
    case FR_MON_MIMIC:
    case FR_MON_LURKER:
        return 100;
    default:
        return 0;
    }
}

bool fr_monster_can_pack(uint8_t type, uint8_t floor) {
    return type == FR_MON_KOBOLD || type == FR_MON_DRAGON ||
           (type == FR_MON_BAT && floor >= 7 && floor <= 12);
}

uint8_t fr_monster_pack_roll(uint8_t type) {
    if(type == FR_MON_BAT) return 2;
    if(type == FR_MON_DRAGON) return 1;
    return 4;
}

uint8_t fr_monster_pack_extra_count(uint8_t type, uint8_t roll_a, uint8_t roll_b) {
    if(type == FR_MON_DRAGON) return (uint8_t)((roll_a % 3) + (roll_b % 2));
    return (uint8_t)(1 + (roll_a % 2));
}

char fr_actor_glyph(uint8_t type) {
    switch(type) {
    case FR_MON_BAT:
        return 'b';
    case FR_MON_SNAKE:
        return 's';
    case FR_MON_GOBLIN:
        return 'g';
    case FR_MON_ARCHER:
        return 'a';
    case FR_MON_WIGHT:
        return 'G';
    case FR_MON_SLIME:
        return 'S';
    case FR_MON_OGRE:
        return 'o';
    case FR_MON_WISP:
        return 'w';
    case FR_MON_KOBOLD:
        return 'k';
    case FR_MON_CUBE:
        return 'C';
    case FR_MON_DRAGON:
        return 'D';
    case FR_MON_YONDER_WARDEN:
        return 'W';
    case FR_MON_EEL:
        return 'e';
    case FR_MON_MIMIC:
        return 'M';
    case FR_MON_LURKER:
        return 'L';
    case FR_MON_RAT:
    default:
        return 'r';
    }
}

const char* fr_actor_name(uint8_t type) {
    switch(type) {
    case FR_MON_BAT:
        return "Bat";
    case FR_MON_SNAKE:
        return "Snake";
    case FR_MON_GOBLIN:
        return "Goblin";
    case FR_MON_ARCHER:
        return "Skeleton Archer";
    case FR_MON_WIGHT:
        return "Wight";
    case FR_MON_SLIME:
        return "Slime";
    case FR_MON_OGRE:
        return "Ogre";
    case FR_MON_WISP:
        return "Wisp";
    case FR_MON_KOBOLD:
        return "Kobold";
    case FR_MON_CUBE:
        return "Cube";
    case FR_MON_DRAGON:
        return "Dragonling";
    case FR_MON_YONDER_WARDEN:
        return "Yonder Warden";
    case FR_MON_EEL:
        return "Eel";
    case FR_MON_MIMIC:
        return "Mimic";
    case FR_MON_LURKER:
        return "Lurker";
    case FR_MON_RAT:
    default:
        return "Rat";
    }
}

const char* fr_actor_log_name(uint8_t type) {
    if(type == FR_MON_ARCHER) return "S.Archer";
    return fr_actor_name(type);
}

const char* fr_actor_flavor(uint8_t type) {
    switch(type) {
    case FR_MON_BAT:
        return "Flaps. Judges.";
    case FR_MON_SNAKE:
        return "Tiny noodle. Bad mood.";
    case FR_MON_GOBLIN:
        return "Small. Armed. Rude.";
    case FR_MON_ARCHER:
        return "Clack. Nothing personal.";
    case FR_MON_WIGHT:
        return "Unseen until rude.";
    case FR_MON_SLIME:
        return "Jelly. No thoughts.";
    case FR_MON_OGRE:
        return "Big. Simple. Certain.";
    case FR_MON_WISP:
        return "Glows. Lies.";
    case FR_MON_KOBOLD:
        return "Pocket problem.";
    case FR_MON_CUBE:
        return "Too friendly. Avoid hugs.";
    case FR_MON_DRAGON:
        return "Small dragon. Large ego.";
    case FR_MON_YONDER_WARDEN:
        return "Slow. Patient. Yours.";
    case FR_MON_EEL:
        return "Wet bite. Worse attitude.";
    case FR_MON_MIMIC:
        return "Box with opinions.";
    case FR_MON_LURKER:
        return "Waited all day. For this.";
    case FR_MON_RAT:
    default:
        return "Weak. Bites. Hates you.";
    }
}
