#include "economy.h"
#include "game_model.h"
#include "tuning.h"

const char *hoard_rank(uint32_t hoard) {
    if(hoard >= RANK5_MIN) return "Dragon Sovereign";
    if(hoard >= RANK4_MIN) return "Wyrm of Wealth";
    if(hoard >= RANK3_MIN) return "Hoarder";
    if(hoard >= RANK2_MIN) return "Trinket Keeper";
    return "Nest Scrounger";
}

bool has_heir_egg(const struct GameState *gs) {
    return gs->persistent.eggs_storm > 0 || gs->persistent.eggs_rare > 0 ||
           gs->persistent.eggs_common > 0;
}

void hatch_heir(struct GameState *gs, uint32_t now) {
    uint32_t hoard = gs->persistent.hoard;
    uint16_t ec = gs->persistent.eggs_common;
    uint16_t er = gs->persistent.eggs_rare;
    uint16_t es = gs->persistent.eggs_storm;
    int32_t care;
    if(es > 0) { es--; care = HEIR_STORM_CARE; }       // storm egg: best lineage
    else if(er > 0) { er--; care = HEIR_RARE_CARE; }
    else if(ec > 0) { ec--; care = HEIR_COMMON_CARE; }
    else { return; } // nothing to hatch
    game_state_init(gs, now);        // fresh dragon (resets needs/stage/etc.)
    gs->persistent.hoard = hoard;    // collection persists across heirs
    gs->persistent.eggs_common = ec;
    gs->persistent.eggs_rare = er;
    gs->persistent.eggs_storm = es;
    gs->persistent.care_score = care; // head-start from the egg's lineage
}
