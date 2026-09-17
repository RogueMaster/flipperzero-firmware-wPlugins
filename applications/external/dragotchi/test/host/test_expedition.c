#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "tuning.h"
void run_expedition_defaults_tests(void) {
    struct GameState s = {0};
    game_state_init(&s, 100);
    CHECK(s.persistent.on_expedition == 0);
    CHECK(s.persistent.expedition_minutes == 0);
    CHECK(s.journey_ready == 0);
}

#include "expedition.h"
#include "hunt.h"
#include "rng_control.h"
void run_expedition_logic_tests(void) {
    char log[96];
    rng_seed(3);
    struct GameState s = {0}; game_state_init(&s, 0);
    expedition_start(&s, EXPED_SHORT_MIN, 1000);
    CHECK(s.persistent.on_expedition == 1);
    CHECK(expedition_done(&s, 1000 + EXPED_SHORT_MIN*EXPED_SEC_PER_MIN) == true);
    CHECK(expedition_done(&s, 1000 + EXPED_SHORT_MIN*EXPED_SEC_PER_MIN - 1) == false);
    CHECK(expedition_remaining_sec(&s, 1000) == EXPED_SHORT_MIN*EXPED_SEC_PER_MIN);

    // epic yields more hoard than short (averaged)
    uint32_t hs=0, he=0;
    for(int i=0;i<40;i++){ rng_seed(100+i); struct GameState a={0}; game_state_init(&a,0);
        a.persistent.on_expedition=1; a.persistent.expedition_minutes=EXPED_SHORT_MIN;
        expedition_resolve(&a,log,sizeof(log)); hs+=a.persistent.hoard; }
    for(int i=0;i<40;i++){ rng_seed(100+i); struct GameState a={0}; game_state_init(&a,0);
        a.persistent.on_expedition=1; a.persistent.expedition_minutes=EXPED_EPIC_MIN;
        expedition_resolve(&a,log,sizeof(log)); he+=a.persistent.hoard; }
    CHECK(he > hs);

    // resolve clears flag and never kills
    struct GameState r={0}; game_state_init(&r,0);
    r.persistent.on_expedition=1; r.persistent.expedition_minutes=EXPED_EPIC_MIN; r.persistent.health=3;
    for(int i=0;i<50;i++){ rng_seed(i); struct GameState x=r; expedition_resolve(&x,log,sizeof(log));
        CHECK(x.persistent.on_expedition==0); CHECK(x.persistent.health>=1); }
}

#include "game_logic.h"
void run_expedition_advance_tests(void) {
    char *unused; (void)unused;
    // paused while away
    struct GameState s={0}; game_state_init(&s,0);
    s.persistent.stage=WYRMLING; s.persistent.hunger=80;
    s.persistent.last_hunger_update=0;
    expedition_start(&s, EXPED_LONG_MIN, 0);
    uint32_t midway = (EXPED_LONG_MIN*EXPED_SEC_PER_MIN)/2;
    advance_state(&s, midway);
    CHECK(s.persistent.on_expedition==1);
    CHECK(s.persistent.hunger==80);          // needs paused, no decay
    // resolves at end
    rng_seed(7);
    uint32_t end = EXPED_LONG_MIN*EXPED_SEC_PER_MIN;
    GameEventFlags f = advance_state(&s, end);
    CHECK(f & EVT_EXPED_RETURN);
    CHECK(s.persistent.on_expedition==0);
    CHECK(s.journey_ready==1);
    CHECK(s.persistent.last_hunger_update==end); // cursor resynced (no retro-decay)
}
