#include "game_model.h"
#include "tuning.h"
void game_state_init(struct GameState *gs, uint32_t now) {
    struct PersistentGameState *p = &gs->persistent;
    p->stage = EGG;
    p->alignment = ALIGN_NONE;
    p->birth_timestamp = now;
    p->stage_entered_timestamp = now;
    p->hunger = MAX_HU;          p->last_hunger_update = now;
    p->happiness = MAX_HAPPINESS; p->last_happiness_update = now;
    p->health = MAX_HP;          p->last_health_update = now;
    p->poop = 0;  p->last_poop_update = now; p->poop_since = 0; p->poop_penalized = 0;
    p->sick = 0;  p->last_sick_update = now; p->sick_since = 0; p->sick_penalized = 0;
    p->lights_off = 0; p->last_sleep_update = now;
    p->care_score = CARE_START;
    p->discipline = DISCIPLINE_START;
    p->attention_call = 0; p->last_attention_update = now; p->last_oldage_update = now;
    p->hoard = 0; p->eggs_common = 0; p->eggs_rare = 0; p->last_forage_time = 0;
    p->treasure_small = 0; p->treasure_med = 0; p->treasure_large = 0;
    p->prey_caught = 0; p->eggs_caught = 0;
    p->on_expedition = 0; p->expedition_start = 0; p->expedition_minutes = 0;
    gs->journey_ready = 0;
    gs->next_animation_index = 0;
    gs->display_state = DISP_IDLE;
}
