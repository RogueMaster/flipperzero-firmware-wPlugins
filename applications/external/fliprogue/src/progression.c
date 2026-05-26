#include "progression.h"

#include "game_core.h"

const char* fr_player_class_name(uint8_t class_id) {
    switch(class_id) {
    case FR_CLASS_RANGER:
        return "Ranger";
    case FR_CLASS_MAGE:
        return "Mage";
    case FR_CLASS_WARRIOR:
    default:
        return "Warrior";
    }
}

const char* fr_perk_name(uint8_t class_id, uint8_t choice) {
    static const char* warrior[] = {"Iron Heart", "Shield Bash", "Riposte", "Cleave", "Hold Line"};
    static const char* ranger[] = {"Pin Shot", "Hamstring", "Trap Eye", "Vantage", "Dart Hand"};
    static const char* mage[] = {"Overcharge", "Ember Staff", "Frost Rune", "Quartz Mind", "Veil"};
    if(choice >= 5) return "Perk";
    if(class_id == FR_CLASS_RANGER) return ranger[choice];
    if(class_id == FR_CLASS_MAGE) return mage[choice];
    return warrior[choice];
}

const char* fr_perk_desc(uint8_t class_id, uint8_t choice) {
    static const char* warrior[] = {
        "+20% max HP",
        "melee bash chance",
        "counter after hit",
        "kill splash",
        "block when swarmed",
    };
    static const char* ranger[] = {
        "shots push",
        "shots slow",
        "better searching",
        "+far shot dmg",
        "+throw dmg",
    };
    static const char* mage[] = {
        "crit staff burst",
        "staff may burn",
        "staff may slow",
        "rest recharges",
        "magic hides you",
    };
    if(choice >= 5) return "";
    if(class_id == FR_CLASS_RANGER) return ranger[choice];
    if(class_id == FR_CLASS_MAGE) return mage[choice];
    return warrior[choice];
}

bool fr_apply_perk(FrGame* game, uint8_t choice) {
    if(game->player.pending_perks == 0 || choice >= 5) return false;
    uint8_t bit = (uint8_t)(1u << choice);
    if((game->player.perks & bit) != 0) {
        fr_log(game, "Already known.");
        return false;
    }

    game->player.perks |= bit;
    game->player.pending_perks--;
    if(game->player.class_id == FR_CLASS_WARRIOR) {
        if(choice == 0) {
            uint8_t gain = (uint8_t)((game->player.max_hp + 4) / 5);
            if(gain == 0) gain = 1;
            game->player.max_hp = (uint8_t)(game->player.max_hp + gain);
            game->player.hp = (uint8_t)(game->player.hp + gain);
        }
    } else if(game->player.class_id == FR_CLASS_RANGER) {
        if(choice == 2) game->player.dex++;
    } else {
        if(choice == 3) game->player.charges = (uint8_t)(game->player.charges + 2);
    }
    fr_log(game, "%s learned.", fr_perk_name(game->player.class_id, choice));
    return true;
}

void fr_award_xp(FrGame* game, uint8_t amount) {
    static const uint8_t xp_thresholds[] = {0, 0, 10, 26, 50, 80, 115};
    uint16_t xp = (uint16_t)game->player.xp + amount;
    game->player.xp = xp > 255u ? 255u : (uint8_t)xp;
    while(game->player.level < 6 && game->player.xp >= xp_thresholds[game->player.level + 1]) {
        game->player.level++;
        game->player.max_hp++;
        game->player.hp++;
        if(game->player.level == 3) {
            game->player.skills |= FR_SKILL_SLOT1;
            game->player.pending_perks++;
            if(game->player.class_id == FR_CLASS_WARRIOR) game->player.shield_lvl++;
            else if(game->player.class_id == FR_CLASS_RANGER) game->player.arrows += 3;
            else game->player.charges += 2;
            fr_log(game, "Skill learned.");
        } else if(game->player.level == 6) {
            game->player.skills |= FR_SKILL_SLOT2;
            game->player.pending_perks++;
            if(game->player.class_id == FR_CLASS_WARRIOR) game->player.sword_lvl++;
            else if(game->player.class_id == FR_CLASS_RANGER) game->player.bow_lvl++;
            else game->player.staff_lvl++;
            fr_log(game, "Deep skill.");
        }
    }
}
