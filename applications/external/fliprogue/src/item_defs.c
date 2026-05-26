#include "item_defs.h"

#include <stdio.h>

static bool fr_item_knows_potion(const FrGame* game, uint8_t potion) {
    return potion < 16 && (game->player.known_potions & (uint16_t)(1u << potion)) != 0;
}

static bool fr_item_knows_scroll(const FrGame* game, uint8_t scroll) {
    return scroll < 16 && (game->player.known_scrolls & (uint16_t)(1u << scroll)) != 0;
}

static bool fr_item_knows_trinket(const FrGame* game, uint8_t trinket) {
    return trinket < 16 && (game->player.known_trinkets & (uint16_t)(1u << trinket)) != 0;
}

char fr_item_glyph(uint8_t type) {
    switch(type) {
    case FR_ITEM_POTION:
        return '!';
    case FR_ITEM_SCROLL:
        return '?';
    case FR_ITEM_GOLD:
        return '*';
    case FR_ITEM_FOOD:
        return '%';
    case FR_ITEM_GEAR:
        return ')';
    case FR_ITEM_ARROWS:
        return ';';
    case FR_ITEM_WAND:
        return '/';
    case FR_ITEM_ORB:
        return '&';
    case FR_ITEM_THROWABLE:
        return ':';
    case FR_ITEM_TRINKET:
        return '=';
    case FR_ITEM_CHEST:
        return 'H';
    case FR_ITEM_KEY:
        return 'K';
    default:
        return ';';
    }
}

const char* fr_potion_label(const FrGame* game, uint8_t potion) {
    static const char* known[] = {
        "No Potion",
        "Healing Potion",
        "Strength Potion",
        "Antidote Potion",
        "Fire Ward Potion",
        "Quickness Potion",
        "Venom Potion",
        "Flame Potion",
        "Frost Potion",
        "Blindness Potion",
        "Smoke Potion",
    };
    if(potion == 0 || potion >= FR_POTION_MAX) return "Potion";
    if(fr_item_knows_potion(game, potion)) return known[potion];
    return game->potion_labels[potion];
}

const char* fr_scroll_label(const FrGame* game, uint8_t scroll) {
    static char unknown[24];
    static const char* known[] = {
        "No Scroll",
        "Identify Scroll",
        "Enchant Weapon",
        "Enchant Armor",
        "Fire Bloom",
        "Blink Scroll",
        "Mapping Scroll",
        "Fear Scroll",
        "Charge Scroll",
        "Phase Scroll",
        "Reveal Secrets",
        "Decurse Scroll",
        "Call Scroll",
    };
    if(scroll == 0 || scroll >= FR_SCROLL_MAX) return "Scroll";
    if(fr_player_knows_scrolls(game) || fr_item_knows_scroll(game, scroll)) return known[scroll];
    snprintf(unknown, sizeof(unknown), "Scroll %s", game->scroll_labels[scroll]);
    return unknown;
}

const char* fr_trinket_label(const FrGame* game, uint8_t trinket) {
    static const char* known[] = {
        "No Charm",
        "Dew Charm",
        "Ash Bead",
        "Scout Loop",
        "Fang Charm",
        "Quartz Ring",
        "Empty Locket",
        "Cinder Charm",
        "Glass Charm",
        "Hungry Charm",
    };
    if(trinket == 0 || trinket >= FR_TRINKET_MAX) return "Charm";
    if(fr_item_knows_trinket(game, trinket)) return known[trinket];
    return game->trinket_labels[trinket];
}

const char* fr_item_name(const FrInvSlot* slot) {
    if(!slot) return "Nothing";
    if(slot->type == FR_ITEM_POTION) {
        static const char* names[] = {
            "Potion",
            "Healing Potion",
            "Strength Potion",
            "Antidote Potion",
            "Fire Ward Potion",
            "Quickness Potion",
            "Venom Potion",
            "Flame Potion",
            "Frost Potion",
            "Blindness Potion",
            "Smoke Potion",
        };
        return slot->subtype < FR_POTION_MAX ? names[slot->subtype] : "Potion";
    }
    if(slot->type == FR_ITEM_SCROLL) {
        static const char* names[] = {
            "Scroll",
            "Identify Scroll",
            "Enchant Weapon",
            "Enchant Armor",
            "Fire Bloom",
            "Blink Scroll",
            "Mapping Scroll",
            "Fear Scroll",
            "Charge Scroll",
            "Phase Scroll",
            "Reveal Secrets",
            "Decurse Scroll",
            "Call Scroll",
        };
        return slot->subtype < FR_SCROLL_MAX ? names[slot->subtype] : "Scroll";
    }
    if(slot->type == FR_ITEM_THROWABLE) return slot->subtype == FR_THROW_DART ? "Darts" : "Stones";
    if(slot->type == FR_ITEM_TRINKET) return "Charm";
    if(slot->type == FR_ITEM_ORB) return "Orb of Yonder";
    if(slot->type == FR_ITEM_CHEST) return "Chest";
    if(slot->type == FR_ITEM_KEY) return "Key";
    return "Thing";
}

const char* fr_inventory_label(const FrGame* game, const FrInvSlot* slot) {
    if(!slot) return "Nothing";
    if(slot->type == FR_ITEM_POTION) {
        static char potion[32];
        const char* label = fr_potion_label(game, slot->subtype);
        if(slot->amount > 1) {
            snprintf(potion, sizeof(potion), "%s (%u)", label, slot->amount);
            return potion;
        }
        return label;
    }
    if(slot->type == FR_ITEM_SCROLL) {
        static char scroll[32];
        const char* label = fr_scroll_label(game, slot->subtype);
        if(slot->amount > 1) {
            snprintf(scroll, sizeof(scroll), "%s (%u)", label, slot->amount);
            return scroll;
        }
        return label;
    }
    if(slot->type == FR_ITEM_WAND) return fr_wand_label(slot->subtype, slot->amount, slot->flags);
    if(slot->type == FR_ITEM_THROWABLE) {
        static char thrown[24];
        const char* name = slot->subtype == FR_THROW_DART ? "Darts" : "Stones";
        snprintf(thrown, sizeof(thrown), "%s (%u)", name, slot->amount);
        return thrown;
    }
    if(slot->type == FR_ITEM_TRINKET) {
        static char charm[28];
        snprintf(
            charm,
            sizeof(charm),
            "%s%s",
            (slot->flags & FR_INV_EQUIPPED) != 0 ? "* " : "",
            fr_trinket_label(game, slot->subtype));
        return charm;
    }
    if(slot->type == FR_ITEM_ORB) return "Orb of Yonder";
    return fr_item_name(slot);
}

const char* fr_inventory_hint(const FrGame* game, const FrInvSlot* slot) {
    if(!slot) return "";
    if(slot->type == FR_ITEM_POTION) {
        if(!fr_item_knows_potion(game, slot->subtype)) return "Unknown Potion";
        switch(slot->subtype) {
        case FR_POTION_HEALING:
            return "Warm flesh.";
        case FR_POTION_STRENGTH:
            return "Iron wakes.";
        case FR_POTION_FIRE_WARD:
            return "Fire says no.";
        case FR_POTION_FROST:
            return "Cold answer.";
        case FR_POTION_FLAME:
            return "Fire blooms.";
        default:
            return "Drink or throw.";
        }
    }
    if(slot->type == FR_ITEM_SCROLL) {
        if(!fr_player_knows_scrolls(game) && !fr_item_knows_scroll(game, slot->subtype)) return "Unknown Scroll";
        switch(slot->subtype) {
        case FR_SCROLL_IDENTIFY:
            return "Names one lie.";
        case FR_SCROLL_FIRE:
            return "Bloom of fire.";
        case FR_SCROLL_MAPPING:
            return "Walls confess.";
        case FR_SCROLL_DECURSE:
            return "Curse cracks.";
        default:
            return "Read. Gone.";
        }
    }
    if(slot->type == FR_ITEM_TRINKET) {
        if(!fr_item_knows_trinket(game, slot->subtype)) return "Unknown Charm";
        switch(slot->subtype) {
        case FR_TRINKET_DEW:
            return "Dew drinks back.";
        case FR_TRINKET_ASH:
            return "Fire shrugs.";
        case FR_TRINKET_SCOUT:
            return "Finds shy traps.";
        case FR_TRINKET_FANG:
            return "Bites fear back.";
        case FR_TRINKET_QUARTZ:
            return "Luck sleeps.";
        case FR_TRINKET_LOCKET:
            return "Hunger slows.";
        case FR_TRINKET_CINDER:
            return "Burns all. Stuck.";
        case FR_TRINKET_GLASS:
            return "Walls go thin.";
        case FR_TRINKET_HUNGRY:
            return "Belly clock runs.";
        default:
            return "Small old magic.";
        }
    }
    if(slot->type == FR_ITEM_WAND) return "Point. Spend zap.";
    if(slot->type == FR_ITEM_THROWABLE) return slot->subtype == FR_THROW_DART ? "Sharp. One-way." : "Heavy answer.";
    if(slot->type == FR_ITEM_ARROWS) return "Bow food.";
    if(slot->type == FR_ITEM_ORB) return "The Warden hears.";
    if(slot->type == FR_ITEM_FOOD) return "Eat. Keep going.";
    if(slot->type == FR_ITEM_GOLD) return "Score glitter.";
    return "";
}

const char* fr_wand_label(uint8_t wand, uint8_t charges, uint8_t max_charges) {
    static char label[24];
    const char* name = "Wand";
    switch(wand) {
    case FR_WAND_FIRE:
        name = "Wand Fire";
        break;
    case FR_WAND_FROST:
        name = "Wand Frost";
        break;
    case FR_WAND_FORCE:
        name = "Wand Force";
        break;
    case FR_WAND_BLINK:
        name = "Wand Blink";
        break;
    case FR_WAND_VENOM:
        name = "Wand Venom";
        break;
    case FR_WAND_FEAR:
        name = "Wand Fear";
        break;
    case FR_WAND_ARC:
        name = "Wand Arc";
        break;
    case FR_WAND_REVEAL:
        name = "Wand Reveal";
        break;
    default:
        break;
    }
    snprintf(label, sizeof(label), "%s (%u/%u)", name, charges, max_charges);
    return label;
}
