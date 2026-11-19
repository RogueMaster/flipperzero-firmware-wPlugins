// Optimal Blackjack - a Flipper Zero app.
// Play blackjack against the dealer. Nothing is wagered: your SCORE is how often
// your play matches BASIC STRATEGY. Correct plays build a streak; a miss shows
// the right play and resets it. Settings: vibro/sound/LED each on-off, S17/H17.

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_random.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <storage/storage.h>
#include "aa_data.h" // Hi-Lo true-count frequency + edge tables (blackjack statistics)
#include "qr_data.h" // QR bitmap for the Google Play listing (pre-generated)

#define MAX_CARDS    12
#define MAX_HANDS    4
#define CT_MAX_LEVEL 20 // Count Trainer regular-mode ladder

typedef enum {
    StateMenu,
    StateChart,
    StateHelp,
    StateSettings,
    StatePlayer,
    StateResult,
    StateInsurance,
    StateCount,
    StateStats,
    StateAdvantage,
    StateFlash
} GameState;
typedef enum {
    ActHit,
    ActStand,
    ActDouble,
    ActSplit
} Action;

typedef struct {
    uint8_t cards[MAX_CARDS];
    uint8_t suit[MAX_CARDS];
    uint8_t n;
    bool done;
    bool split_ace;
} Hand;

typedef struct {
    Hand hands[MAX_HANDS];
    uint8_t num_hands;
    uint8_t cur;
    uint8_t dealer[MAX_CARDS];
    uint8_t dsuit[MAX_CARDS];
    uint8_t dn;
    GameState state;
    uint8_t chart_page;
    uint8_t set_row;
    bool vibro, sound, led, h17;
    uint32_t correct, total, streak, best;
    uint16_t shoe_dealt;
    int16_t rc; // Hi-Lo running count (reset each shuffle)
    uint8_t count_mode; // 0 off, 1 RC, 2 RC+TC
    // ── main-menu 2D selector + Count Trainer mode ──
    uint8_t menu_sel; // mode row cursor: 0 Table, 1 Count, 2 Endless (Left/Right)
    uint8_t menu_tool; // tool row cursor: 0 Charts, 1 Settings, 2 Help (Up/Down)
    uint8_t menu_focus; // which row OK acts on: 0 modes, 1 tools
    uint8_t ct_phase; // 0 flashing, 1 answering, 2 feedback
    bool ct_endless; // endless variant: speeds up, ends on a miss
    bool ct_gameover; // endless run just ended
    uint8_t ct_level; // regular mode: current level (1..CT_MAX_LEVEL)
    uint8_t ct_bestlevel; // regular mode: highest level reached
    uint8_t ct_shown; // cards flashed so far this round
    uint8_t ct_cards; // cards to flash this round
    uint8_t ct_card; // card currently on screen
    uint8_t ct_suit;
    int16_t ct_rc; // true running count of the flashed cards
    int16_t ct_guess; // player's entered count
    uint16_t ct_speed; // ms per flashed card
    uint32_t ct_last; // tick of the last flip (animation clock)
    uint32_t ct_streak, ct_best, ct_correct, ct_total;
    // ── lifetime stats (accumulate in the background; survive a session Reset) ──
    uint32_t lt_rounds; // table rounds completed
    uint32_t lt_wins, lt_losses, lt_pushes; // outcome per settled hand
    uint32_t lt_bj, lt_busts, lt_dbusts; // player blackjacks, player busts, dealer busts
    uint32_t lt_dec, lt_dec_ok; // basic-strategy play decisions / correct
    uint32_t lt_doubles, lt_splits; // doubles & splits taken
    uint32_t lt_ins, lt_ins_ok; // insurance decisions / correct (decline)
    uint32_t lt_beststreak; // best play streak ever (survives Reset)
    // ── Advantage Analysis tool: indices into preset tables + toggles (persisted) ──
    uint8_t aa_decks_i, aa_pen_i, aa_bank_i, aa_unit_i, aa_spread_i, aa_hph_i;
    uint8_t aa_double_i; // 0 any, 1 nine-eleven, 2 ten-eleven, 3 eleven-only
    uint8_t aa_surr_i; // 0 none, 1 late, 2 early-v-10, 3 early-v-ace, 4 full early
    uint8_t aa_bj_i; // 0 3:2, 1 6:5, 2 1:1
    uint8_t aa_dev_set_i; // deviations: 0 off, 1 Top18, 2 Advanced50, 3 complete
    uint8_t aa_skill_i; // play-accuracy preset (AA_SKILL)
    bool aa_h17, aa_das, aa_nopeek;
    bool aa_rsa, aa_hsa, aa_dsa; // resplit / hit / double split aces
    bool aa_dah, aa_nosoftdbl; // double after hit, soft doubling OFF
    bool aa_push22, aa_bjtie; // dealer 22 pushes, player wins BJ ties
    uint8_t aa_cat; // settings category: 0 table, 1 bankroll, 2 rare
    uint8_t aa_page; // 0 inputs, 1 results
    uint8_t aa_row; // 0 = category tab, 1..N = setting in the category
    // ── Strategy Flashcards ──
    uint8_t fc_cards[4]; // the drilled player hand
    uint8_t fc_n;
    uint8_t fc_up; // dealer upcard rank (1..13)
    bool fc_canD, fc_canY; // double / split offered for this hand
    uint8_t fc_phase; // 0 asking, 1 feedback
    uint8_t fc_correct; // the right Action, for feedback
    bool fc_right; // was the last answer correct
    uint32_t fc_streak, fc_best, fc_cok, fc_ctot;
    char fb[40];
    char result[64];
    uint8_t notify;
    bool exit;
    FuriMutex* mutex;
} App;

// ── Advantage tool: settings categories (the top Left/Right tab) ───────────
// Each category is a column of adjustable rows. Row counts + first setting id.
// Declared up here so handle_input (above the Advantage section) can see them.
#define AA_NCAT 5
static const char* AA_CATNAME[AA_NCAT] =
    {"TABLE RULES", "SPLIT & SURR", "RARE RULES", "BANKROLL", "COUNT+SKILL"};
static const uint8_t AA_CATCOUNT[AA_NCAT] = {5, 5, 4, 3, 4}; // <=5 rows so each is readable
static const uint8_t AA_CATSTART[AA_NCAT] = {0, 5, 10, 14, 17};
#define AA_NSETTING 21
static void aa_set_edit(App* a, int id, int dir); // defined in the Advantage section

static uint8_t cval(uint8_t rank) {
    if(rank == 1) return 11;
    if(rank >= 10) return 10;
    return rank;
}
static void hand_total(const uint8_t* c, uint8_t n, int* total, bool* soft) {
    int t = 0, aces = 0;
    for(uint8_t i = 0; i < n; i++) {
        uint8_t v = cval(c[i]);
        t += v;
        if(v == 11) aces++;
    }
    while(t > 21 && aces > 0) {
        t -= 10;
        aces--;
    }
    *total = t;
    *soft = (aces > 0);
}
static uint8_t draw_card(void) {
    return (uint8_t)(furi_hal_random_get() % 13) + 1;
}
// A flash card whose RANK differs from the previous one, so a repeated card is
// never shown as an unchanging glyph (you could miss it and miscount).
static uint8_t draw_card_diff(uint8_t prev) {
    uint8_t c;
    do {
        c = draw_card();
    } while(c == prev);
    return c;
}
static uint8_t rand_suit(void) {
    return (uint8_t)(furi_hal_random_get() % 4);
}
// A dealt card out of a 6-deck shoe (312 cards); bumps the penetration counter
// that drives the shoe + discard-tray meters.
static uint8_t deal(App* a) {
    a->shoe_dealt++;
    return draw_card();
}
// Hi-Lo tag: 2-6 = +1, 7-9 = 0, tens & Ace = -1.
static int hilo(uint8_t rank) {
    if(rank >= 2 && rank <= 6) return 1;
    if(rank >= 7 && rank <= 9) return 0;
    return -1;
}
static const char* act_word(Action a) {
    switch(a) {
    case ActHit:
        return "Hit";
    case ActStand:
        return "Stand";
    case ActDouble:
        return "Double";
    default:
        return "Split";
    }
}
static void rank_str(uint8_t rank, char* out) {
    static const char* R = "A23456789TJQK";
    out[0] = R[rank - 1];
    out[1] = 0;
}

static Action basic_strategy(
    const uint8_t* c,
    uint8_t n,
    uint8_t up,
    bool can_double,
    bool can_split,
    bool h17) {
    bool pair = (n == 2 && cval(c[0]) == cval(c[1]));
    if(can_split && pair) {
        uint8_t r = cval(c[0]);
        switch(r) {
        case 11:
            return ActSplit;
        case 10:
            return ActStand;
        case 9:
            return (up == 7 || up == 10 || up == 11) ? ActStand : ActSplit;
        case 8:
            return ActSplit;
        case 7:
            return (up >= 2 && up <= 7) ? ActSplit : ActHit;
        case 6:
            return (up >= 2 && up <= 6) ? ActSplit : ActHit;
        case 5:
            return (up >= 2 && up <= 9) ? (can_double ? ActDouble : ActHit) : ActHit;
        case 4:
            return (up == 5 || up == 6) ? ActSplit : ActHit;
        case 3:
            return (up >= 2 && up <= 7) ? ActSplit : ActHit;
        case 2:
            return (up >= 2 && up <= 7) ? ActSplit : ActHit;
        default:
            break;
        }
    }
    int t;
    bool soft;
    hand_total(c, n, &t, &soft);
    if(soft) {
        Action d = can_double ? ActDouble : ActHit;
        switch(t) {
        case 13:
        case 14:
            return (up == 5 || up == 6) ? d : ActHit;
        case 15:
        case 16:
            return (up >= 4 && up <= 6) ? d : ActHit;
        case 17:
            return (up >= 3 && up <= 6) ? d : ActHit;
        case 18:
            if(up >= 3 && up <= 6) return can_double ? ActDouble : ActStand;
            if(up == 2) return (h17 && can_double) ? ActDouble : ActStand;
            if(up == 7 || up == 8) return ActStand;
            return ActHit;
        case 19:
            return (h17 && up == 6) ? (can_double ? ActDouble : ActStand) : ActStand;
        default:
            return ActStand;
        }
    }
    if(t <= 8) return ActHit;
    if(t == 9) return (up >= 3 && up <= 6) ? (can_double ? ActDouble : ActHit) : ActHit;
    if(t == 10) return (up >= 2 && up <= 9) ? (can_double ? ActDouble : ActHit) : ActHit;
    if(t == 11)
        return ((up >= 2 && up <= 10) || (h17 && up == 11)) ? (can_double ? ActDouble : ActHit) :
                                                              ActHit;
    if(t == 12) return (up >= 4 && up <= 6) ? ActStand : ActHit;
    if(t >= 13 && t <= 16) return (up >= 2 && up <= 6) ? ActStand : ActHit;
    return ActStand;
}

static void dealer_and_resolve(App* a) {
    int t;
    bool s;
    a->rc += hilo(a->dealer[1]); // the hole card flips face-up now
    for(;;) {
        hand_total(a->dealer, a->dn, &t, &s);
        if(t > 21) break;
        if(t >= 18) break;
        if(t == 17 && !(a->h17 && s)) break;
        if(a->dn >= MAX_CARDS) break;
        a->dealer[a->dn] = deal(a);
        a->dsuit[a->dn] = rand_suit();
        a->dn++;
        a->rc += hilo(a->dealer[a->dn - 1]);
    }
    int dt = t;
    // lifetime tally: one round, dealer bust, and each settled hand's outcome
    a->lt_rounds++;
    if(dt > 21) a->lt_dbusts++;
    for(uint8_t i = 0; i < a->num_hands; i++) {
        int lpt;
        bool lps;
        hand_total(a->hands[i].cards, a->hands[i].n, &lpt, &lps);
        if(lpt > 21)
            a->lt_losses++;
        else if(dt > 21)
            a->lt_wins++;
        else if(lpt > dt)
            a->lt_wins++;
        else if(lpt < dt)
            a->lt_losses++;
        else
            a->lt_pushes++;
    }
    if(a->num_hands == 1) {
        int pt;
        bool ps;
        hand_total(a->hands[0].cards, a->hands[0].n, &pt, &ps);
        if(pt > 21)
            snprintf(a->result, sizeof(a->result), "Bust! Dealer %d", dt);
        else if(dt > 21)
            snprintf(a->result, sizeof(a->result), "Dealer bust - Win");
        else if(pt > dt)
            snprintf(a->result, sizeof(a->result), "Win %d-%d", pt, dt);
        else if(pt < dt)
            snprintf(a->result, sizeof(a->result), "Lose %d-%d", pt, dt);
        else
            snprintf(a->result, sizeof(a->result), "Push %d", pt);
    } else {
        int win = 0, lose = 0, push = 0;
        for(uint8_t i = 0; i < a->num_hands; i++) {
            int pt;
            bool ps;
            hand_total(a->hands[i].cards, a->hands[i].n, &pt, &ps);
            if(pt > 21)
                lose++;
            else if(dt > 21)
                win++;
            else if(pt > dt)
                win++;
            else if(pt < dt)
                lose++;
            else
                push++;
        }
        snprintf(a->result, sizeof(a->result), "W%d L%d P%d  Dlr %d", win, lose, push, dt);
    }
    a->state = StateResult;
}

static void advance(App* a) {
    for(uint8_t i = 0; i < a->num_hands; i++) {
        if(!a->hands[i].done) {
            a->cur = i;
            return;
        }
    }
    dealer_and_resolve(a);
}

// Begin the player's turn (or auto-resolve a natural). Split out of new_round so
// the insurance step can run first and then hand off here.
static void start_player_phase(App* a) {
    int t;
    bool s;
    hand_total(a->hands[0].cards, 2, &t, &s);
    if(t == 21) { // player natural
        a->hands[0].done = true;
        a->lt_bj++;
        snprintf(a->fb, sizeof(a->fb), "Blackjack!");
        a->notify = 1;
        dealer_and_resolve(a);
    } else {
        a->state = StatePlayer;
    }
}

static void new_round(App* a) {
    a->num_hands = 1;
    a->cur = 0;
    if(a->shoe_dealt >= 234) { // cut card at ~75% of a 6-deck shoe
        a->shoe_dealt = 0;
        a->rc = 0; // reshuffle resets the running count
    }
    memset(a->hands, 0, sizeof(a->hands));
    a->hands[0].cards[0] = deal(a);
    a->hands[0].suit[0] = rand_suit();
    a->hands[0].cards[1] = deal(a);
    a->hands[0].suit[1] = rand_suit();
    a->hands[0].n = 2;
    a->dealer[0] = deal(a);
    a->dsuit[0] = rand_suit();
    a->dealer[1] = deal(a);
    a->dsuit[1] = rand_suit();
    a->dn = 2;
    // count the three face-up cards; the dealer hole is counted when it flips
    a->rc += hilo(a->hands[0].cards[0]) + hilo(a->hands[0].cards[1]) + hilo(a->dealer[0]);
    a->fb[0] = 0;
    a->result[0] = 0;
    // Dealer showing an Ace: the player must settle insurance first. Basic
    // strategy (no counting) always declines, so "No" is the graded-correct play.
    if(cval(a->dealer[0]) == 11) {
        a->state = StateInsurance;
    } else {
        start_player_phase(a);
    }
}

// ── Count Trainer ─────────────────────────────────────────────────────────
// Cards flash one at a time; afterwards the player enters the Hi-Lo running
// count. Endless speeds up and grows the batch on each success, ending on a
// miss. Separate from the table game (its own count, no shoe meter).
// Regular-mode difficulty curve: higher levels flash more cards, faster.
static void ct_level_params(uint8_t lvl, uint8_t* cards, uint16_t* speed) {
    if(lvl < 1) lvl = 1;
    int c = 5 + lvl; // L1 = 6 cards, climbing
    if(c > 26) c = 26;
    int s = 1000 - (int)lvl * 45; // L1 = 955 ms, getting faster
    if(s < 300) s = 300;
    *cards = (uint8_t)c;
    *speed = (uint16_t)s;
}

static void start_count(App* a, bool endless) {
    a->state = StateCount;
    a->ct_endless = endless;
    a->ct_gameover = false;
    a->ct_phase = 0;
    if(endless) {
        a->ct_cards = 6;
        a->ct_speed = 850;
        a->ct_streak = 0; // a fresh endless run starts at zero (ct_next re-preserves it)
    } else {
        if(a->ct_level < 1) a->ct_level = 1;
        if(a->ct_bestlevel < a->ct_level) a->ct_bestlevel = a->ct_level;
        ct_level_params(a->ct_level, &a->ct_cards, &a->ct_speed);
    }
    a->ct_rc = 0;
    a->ct_guess = 0;
    a->fb[0] = 0;
    a->ct_card = draw_card();
    a->ct_suit = rand_suit();
    a->ct_rc += hilo(a->ct_card);
    a->ct_shown = 1;
    a->ct_last = furi_get_tick();
}

// Advance the flash on the animation clock; returns true if the frame changed.
static bool ct_flip(App* a) {
    if(furi_get_tick() - a->ct_last < furi_ms_to_ticks(a->ct_speed)) return false;
    a->ct_last = furi_get_tick();
    if(a->ct_shown >= a->ct_cards) {
        a->ct_phase = 1; // done flashing -> ask for the count
        return true;
    }
    a->ct_card = draw_card_diff(a->ct_card); // never the same rank twice in a row
    a->ct_suit = rand_suit();
    a->ct_rc += hilo(a->ct_card);
    a->ct_shown++;
    return true;
}

static void ct_submit(App* a) {
    a->ct_total++;
    if(a->ct_guess == a->ct_rc) {
        a->ct_correct++;
        a->notify = 1;
        if(a->ct_endless) { // streak + ramp, no end until a miss
            a->ct_streak++;
            if(a->ct_streak > a->ct_best) a->ct_best = a->ct_streak;
            snprintf(a->fb, sizeof(a->fb), "Correct!  RC %+d", a->ct_rc);
            if(a->ct_speed > 320) a->ct_speed -= 45;
            if(a->ct_cards < 20 && (a->ct_streak % 2 == 0)) a->ct_cards++;
        } else { // regular: climb the ladder
            if(a->ct_level < CT_MAX_LEVEL) {
                a->ct_level++;
                snprintf(a->fb, sizeof(a->fb), "Stage up!  RC %+d", a->ct_rc);
            } else {
                snprintf(a->fb, sizeof(a->fb), "Maxed out!  RC %+d", a->ct_rc);
            }
            if(a->ct_level > a->ct_bestlevel) a->ct_bestlevel = a->ct_level;
        }
    } else {
        a->notify = 2;
        snprintf(a->fb, sizeof(a->fb), "RC was %+d, you %+d", a->ct_rc, a->ct_guess);
        if(a->ct_endless) a->ct_gameover = true; // endless ends on the first miss
        // regular: stay on this level and try again
    }
    a->ct_phase = 2; // show feedback; OK continues
}

// Start the next batch, carrying the endless ramp (speed + batch size).
static void ct_next(App* a) {
    bool e = a->ct_endless;
    uint16_t sp = a->ct_speed;
    uint8_t nc = a->ct_cards;
    uint32_t stk = a->ct_streak;
    start_count(a, e);
    if(e) { // keep the progression that a fresh start_count would have reset
        a->ct_speed = sp;
        a->ct_cards = nc;
        a->ct_streak = stk;
    }
}

// ── Strategy Flashcards: one chart decision, graded by basic_strategy ──
static void flash_deal(App* a) {
    a->fc_cards[0] = draw_card();
    a->fc_cards[1] = draw_card();
    a->fc_n = 2;
    a->fc_up = draw_card();
    a->fc_canD = true; // two cards: double OK
    a->fc_canY = (cval(a->fc_cards[0]) == cval(a->fc_cards[1])); // pair: split OK
    a->fc_phase = 0;
}
static void start_flash(App* a) {
    a->state = StateFlash;
    flash_deal(a);
}
static void flash_action(App* a, Action act) {
    if(act == ActDouble && !a->fc_canD) return;
    if(act == ActSplit && !a->fc_canY) return;
    Action correct =
        basic_strategy(a->fc_cards, a->fc_n, cval(a->fc_up), a->fc_canD, a->fc_canY, a->h17);
    a->fc_correct = (uint8_t)correct;
    a->fc_right = (act == correct);
    a->fc_ctot++;
    a->lt_dec++; // counts toward lifetime play decisions too
    if(a->fc_right) {
        a->fc_cok++;
        a->lt_dec_ok++;
        a->fc_streak++;
        if(a->fc_streak > a->fc_best) a->fc_best = a->fc_streak;
        if(a->fc_streak > a->lt_beststreak) a->lt_beststreak = a->fc_streak;
        a->notify = 1;
    } else {
        a->fc_streak = 0;
        a->notify = 2;
    }
    a->fc_phase = 1;
}

static void player_action(App* a, Action act) {
    Hand* h = &a->hands[a->cur];
    bool pair = (h->n == 2 && cval(h->cards[0]) == cval(h->cards[1]));
    bool can_double = (h->n == 2 && !h->split_ace);
    bool can_split = (h->n == 2 && pair && a->num_hands < MAX_HANDS && !h->split_ace);
    if(act == ActDouble && !can_double) return;
    if(act == ActSplit && !can_split) return;

    Action correct =
        basic_strategy(h->cards, h->n, cval(a->dealer[0]), can_double, can_split, a->h17);
    a->total++;
    a->lt_dec++;
    if(act == correct) {
        a->correct++;
        a->streak++;
        if(a->streak > a->best) a->best = a->streak;
        if(a->streak > a->lt_beststreak) a->lt_beststreak = a->streak;
        a->lt_dec_ok++;
        snprintf(a->fb, sizeof(a->fb), "Correct");
        a->notify = 1;
    } else {
        a->streak = 0;
        snprintf(a->fb, sizeof(a->fb), "X Should: %s", act_word(correct));
        a->notify = 2;
    }

    if(act == ActHit) {
        h->cards[h->n] = deal(a);
        h->suit[h->n] = rand_suit();
        h->n++;
        a->rc += hilo(h->cards[h->n - 1]);
        int t;
        bool s;
        hand_total(h->cards, h->n, &t, &s);
        if(t > 21) a->lt_busts++;
        if(t > 21 || h->n >= MAX_CARDS) {
            h->done = true;
            advance(a);
        }
    } else if(act == ActStand) {
        h->done = true;
        advance(a);
    } else if(act == ActDouble) {
        a->lt_doubles++;
        h->cards[h->n] = deal(a);
        h->suit[h->n] = rand_suit();
        h->n++;
        a->rc += hilo(h->cards[h->n - 1]);
        {
            int t;
            bool s;
            hand_total(h->cards, h->n, &t, &s);
            if(t > 21) a->lt_busts++;
        }
        h->done = true;
        advance(a);
    } else if(act == ActSplit) {
        a->lt_splits++;
        Hand* nh = &a->hands[a->num_hands];
        memset(nh, 0, sizeof(Hand));
        nh->cards[0] = h->cards[1];
        nh->suit[0] = h->suit[1];
        nh->n = 1;
        h->n = 1;
        a->num_hands++;
        bool aces = (cval(h->cards[0]) == 11);
        h->cards[h->n] = deal(a);
        h->suit[h->n] = rand_suit();
        h->n++;
        nh->cards[nh->n] = deal(a);
        nh->suit[nh->n] = rand_suit();
        nh->n++;
        a->rc += hilo(h->cards[h->n - 1]) + hilo(nh->cards[nh->n - 1]);
        if(aces) {
            h->split_ace = nh->split_ace = true;
            h->done = nh->done = true;
            advance(a);
        }
    }
}

static void handle_input(App* a, InputKey key) {
    if(key == InputKeyBack) {
        if(a->state == StateMenu)
            a->exit = true;
        else
            a->state = StateMenu;
        return;
    }
    switch(a->state) {
    case StateMenu:
        // Up/Down move between the MODE row (above) and the TOOL row (below);
        // Left/Right pick within the focused row. OK acts on the focused row.
        if(key == InputKeyUp) {
            a->menu_focus = 0; // go up to the modes row
        } else if(key == InputKeyDown) {
            a->menu_focus = 1; // go down to the tools row
        } else if(key == InputKeyLeft) {
            if(a->menu_focus == 0)
                a->menu_sel = (a->menu_sel + 4) % 5;
            else
                a->menu_tool = (a->menu_tool + 3) % 4;
        } else if(key == InputKeyRight) {
            if(a->menu_focus == 0)
                a->menu_sel = (a->menu_sel + 1) % 5;
            else
                a->menu_tool = (a->menu_tool + 1) % 4;
        } else if(key == InputKeyOk) {
            if(a->menu_focus == 0) { // launch the selected mode
                if(a->menu_sel == 0) {
                    a->fb[0] = 0;
                    new_round(a);
                } else if(a->menu_sel == 1 || a->menu_sel == 2) {
                    start_count(a, a->menu_sel == 2); // 1 Count, 2 Endless
                } else if(a->menu_sel == 3) { // Advantage Analysis
                    a->state = StateAdvantage;
                    a->aa_page = 0;
                    a->aa_row = 0;
                } else { // 4 Flashcards
                    start_flash(a);
                }
            } else { // open the selected tool
                if(a->menu_tool == 0) {
                    a->state = StateChart;
                    a->chart_page = 0;
                } else if(a->menu_tool == 1) {
                    a->state = StateStats;
                    a->chart_page = 0;
                } else if(a->menu_tool == 2) {
                    a->state = StateSettings;
                    a->set_row = 0;
                } else {
                    a->state = StateHelp;
                    a->chart_page = 0;
                }
            }
        }
        break;
    case StateChart:
        if(key == InputKeyDown || key == InputKeyRight)
            a->chart_page = (a->chart_page + 1) % 7;
        else if(key == InputKeyUp || key == InputKeyLeft)
            a->chart_page = (a->chart_page + 6) % 7;
        break;
    case StateHelp:
        if(key == InputKeyRight || key == InputKeyDown)
            a->chart_page = (a->chart_page + 1) % 3; // page 3 is the Play QR
        else if(key == InputKeyLeft || key == InputKeyUp)
            a->chart_page = (a->chart_page + 2) % 3;
        break;
    case StateStats:
        if(key == InputKeyUp || key == InputKeyDown || key == InputKeyLeft || key == InputKeyRight)
            a->chart_page ^= 1;
        break;
    case StateAdvantage:
        if(a->aa_page == 0) { // editing inputs
            int count = AA_CATCOUNT[a->aa_cat]; // rows below the tab (tab = row 0)
            if(key == InputKeyUp)
                a->aa_row = (a->aa_row == 0) ? count : a->aa_row - 1;
            else if(key == InputKeyDown)
                a->aa_row = (a->aa_row >= count) ? 0 : a->aa_row + 1;
            else if(key == InputKeyLeft || key == InputKeyRight) {
                int d = (key == InputKeyRight) ? 1 : -1;
                if(a->aa_row == 0) { // on the tab: switch category (wraps)
                    int cc = (int)a->aa_cat + d;
                    if(cc < 0) cc = AA_NCAT - 1;
                    if(cc >= AA_NCAT) cc = 0;
                    a->aa_cat = (uint8_t)cc;
                } else { // on a setting: adjust its value
                    aa_set_edit(a, AA_CATSTART[a->aa_cat] + (a->aa_row - 1), d);
                }
            } else if(key == InputKeyOk)
                a->aa_page = 1; // show the results
        } else {
            a->aa_page = 0; // any key returns to the inputs to tweak
        }
        break;
    case StateSettings:
        if(key == InputKeyUp)
            a->set_row = (a->set_row + 5) % 6;
        else if(key == InputKeyDown)
            a->set_row = (a->set_row + 1) % 6;
        else if(key == InputKeyOk || key == InputKeyLeft || key == InputKeyRight) {
            if(a->set_row == 0)
                a->vibro = !a->vibro;
            else if(a->set_row == 1)
                a->sound = !a->sound;
            else if(a->set_row == 2)
                a->led = !a->led;
            else if(a->set_row == 3)
                a->h17 = !a->h17;
            else if(a->set_row == 4)
                a->count_mode = (a->count_mode + 1) % 3;
            else if(key == InputKeyOk) // Reset stats: OK only, never a scroll
                a->correct = a->total = a->streak = a->best = 0;
        }
        break;
    case StatePlayer:
        if(key == InputKeyLeft)
            player_action(a, ActHit);
        else if(key == InputKeyRight)
            player_action(a, ActStand);
        else if(key == InputKeyUp)
            player_action(a, ActDouble);
        else if(key == InputKeyDown)
            player_action(a, ActSplit);
        break;
    case StateFlash:
        if(a->fc_phase == 0) { // same controls as the table
            if(key == InputKeyLeft)
                flash_action(a, ActHit);
            else if(key == InputKeyRight)
                flash_action(a, ActStand);
            else if(key == InputKeyUp)
                flash_action(a, ActDouble);
            else if(key == InputKeyDown)
                flash_action(a, ActSplit);
        } else if(key == InputKeyOk) {
            flash_deal(a); // next card
        }
        break;
    case StateInsurance:
        if(key == InputKeyLeft || key == InputKeyRight) {
            bool took = (key == InputKeyRight);
            a->total++;
            a->lt_ins++;
            if(!took) { // decline insurance = correct basic-strategy play
                a->correct++;
                a->streak++;
                if(a->streak > a->best) a->best = a->streak;
                if(a->streak > a->lt_beststreak) a->lt_beststreak = a->streak;
                a->lt_ins_ok++;
                snprintf(a->fb, sizeof(a->fb), "Correct: no insurance");
                a->notify = 1;
            } else {
                a->streak = 0;
                snprintf(a->fb, sizeof(a->fb), "X Insurance is -EV");
                a->notify = 2;
            }
            // dealer peeks under the Ace: a ten in the hole is a dealer natural
            if(cval(a->dealer[1]) == 10)
                dealer_and_resolve(a);
            else
                start_player_phase(a);
        }
        break;
    case StateResult:
        if(key == InputKeyOk) new_round(a);
        break;
    case StateCount:
        if(a->ct_phase == 1) { // entering the count
            if(key == InputKeyUp || key == InputKeyRight)
                a->ct_guess++;
            else if(key == InputKeyDown || key == InputKeyLeft)
                a->ct_guess--;
            else if(key == InputKeyOk)
                ct_submit(a);
        } else if(a->ct_phase == 2) { // feedback
            if(key == InputKeyOk) {
                if(a->ct_endless && a->ct_gameover)
                    start_count(a, true); // fresh endless run
                else
                    ct_next(a);
            }
        }
        // phase 0 (flashing) ignores input
        break;
    }
}

static void play_feedback(App* a, NotificationApp* notif, bool good) {
    const NotificationMessage* seq[16];
    size_t i = 0;
    if(a->led) seq[i++] = good ? &message_green_255 : &message_red_255;
    if(a->vibro) seq[i++] = &message_vibro_on;
    if(a->sound) seq[i++] = good ? &message_note_g5 : &message_note_c4;
    if(i == 0) return;
    seq[i++] = &message_delay_100;
    if(a->sound) seq[i++] = &message_sound_off;
    if(a->vibro) seq[i++] = &message_vibro_off;
    seq[i++] = NULL;
    notification_message(notif, (const NotificationSequence*)&seq);
}

static void draw_tiny_c(Canvas* c, int cx, int cy, char ch); // defined with the tiny font below
static const uint8_t* tiny_glyph(char ch); // ditto

// Suit pip as a crisp 5-wide bitmap, top-left corner at (sx, sy), each pixel an
// s*s box so the pip can be blown up for the Count Trainer flash card.
// 0 spade 1 heart 2 diamond 3 club. Each row's low 5 bits are the columns
// (bit4 = left). Explicit pixels read far cleaner than overlapping discs did.
static void draw_suit_s(Canvas* c, int sx, int sy, uint8_t suit, int s) {
    static const uint8_t spade[6] = {0x04, 0x0E, 0x1F, 0x1F, 0x04, 0x0E};
    static const uint8_t heart[5] = {0x0A, 0x1F, 0x1F, 0x0E, 0x04};
    static const uint8_t diam[5] = {0x04, 0x0E, 0x1F, 0x0E, 0x04};
    static const uint8_t club[6] = {0x0E, 0x0E, 0x1F, 0x15, 0x04, 0x0E};
    const uint8_t* g;
    int n;
    switch(suit) {
    case 1:
        g = heart;
        n = 5;
        break;
    case 2:
        g = diam;
        n = 5;
        break;
    case 3:
        g = club;
        n = 6;
        break;
    default:
        g = spade;
        n = 6;
        break;
    }
    for(int r = 0; r < n; r++) {
        uint8_t row = g[r];
        for(int ci = 0; ci < 5; ci++)
            if(row & (1 << (4 - ci))) canvas_draw_box(c, sx + ci * s, sy + r * s, s, s);
    }
}
static void draw_suit(Canvas* c, int sx, int sy, uint8_t suit) {
    draw_suit_s(c, sx, sy, suit, 1); // a 1x box is one pixel
}
// A tiny-font glyph scaled up s* and centred on (cx, cy) — used for the big
// flash card, where a guaranteed-correct 'A' matters.
static void draw_big_c(Canvas* c, int cx, int cy, char ch, int s) {
    const uint8_t* g = tiny_glyph(ch);
    if(!g) return;
    int x0 = cx - (3 * s) / 2, y0 = cy - (5 * s) / 2;
    for(int r = 0; r < 5; r++) {
        uint8_t row = g[r];
        if(row & 4) canvas_draw_box(c, x0 + 0 * s, y0 + r * s, s, s);
        if(row & 2) canvas_draw_box(c, x0 + 1 * s, y0 + r * s, s, s);
        if(row & 1) canvas_draw_box(c, x0 + 2 * s, y0 + r * s, s, s);
    }
}

static void draw_card_box(Canvas* c, int x, int y, uint8_t rank, uint8_t suit) {
    canvas_draw_rframe(c, x, y, 13, 16, 2);
    char r[3];
    rank_str(rank, r);
    draw_tiny_c(c, x + 4, y + 5, r[0]); // rank, tiny font (A is distinct from H)
    draw_suit(c, x + 4, y + 8, suit); // suit pip, centred lower, inside the frame
}
static void draw_hidden_box(Canvas* c, int x, int y) {
    canvas_draw_rframe(c, x, y, 13, 16, 2);
    canvas_draw_line(c, x + 3, y + 3, x + 9, y + 12);
    canvas_draw_line(c, x + 9, y + 3, x + 3, y + 12);
}
static void draw_action(Canvas* c, int x, int y, CanvasDirection dir, const char* label) {
    canvas_draw_triangle(c, x, y, 6, 5, dir);
    canvas_draw_str(c, x + 6, y + 3, label);
}

// Bottom-right page indicator: "x/n" + a REAL drawn down-arrow + "next".
// (A typed 'v' renders like a 'u' on this font, so the arrow is drawn.)
// back=true also prints "Back menu" at bottom-left.
static void draw_page_footer(Canvas* c, int page, int npages, bool back) {
    canvas_set_font(c, FontSecondary);
    if(back) canvas_draw_str(c, 1, 63, "Back menu");
    char f[24];
    snprintf(f, sizeof(f), "%d/%d", page + 1, npages);
    int wnext = canvas_string_width(c, "next");
    int xnext = 128 - wnext;
    canvas_draw_str(c, xnext, 63, "next");
    int xa = xnext - 10;
    canvas_draw_triangle(c, xa, 58, 6, 5, CanvasDirectionTopToBottom);
    int wf = canvas_string_width(c, f);
    canvas_draw_str(c, xa - 5 - wf, 63, f);
}

// Superseded by the home-screen carousel (StateMenu draw). Kept for reference.
// Centered main-menu nav row: drawn arrows + labels for Chart / Help / Setup.
// static void draw_menu_nav(Canvas* c) {
//     canvas_set_font(c, FontSecondary);
//     const char* labels[3] = {"Chart", "Help", "Setup"};
//     const CanvasDirection dirs[3] = {
//         CanvasDirectionLeftToRight, CanvasDirectionBottomToTop, CanvasDirectionTopToBottom};
//     int w[3], total = 0;
//     const int gap = 6;
//     for(int i = 0; i < 3; i++) {
//         w[i] = 9 + canvas_string_width(c, labels[i]);
//         total += w[i];
//     }
//     total += gap * 2;
//     int x = (128 - total) / 2;
//     if(x < 1) x = 1;
//     const int y = 58;
//     for(int i = 0; i < 3; i++) {
//         int axc = x + 3;
//         if(dirs[i] == CanvasDirectionLeftToRight)
//             canvas_draw_triangle(c, axc, y - 3, 5, 5, dirs[i]);
//         else if(dirs[i] == CanvasDirectionBottomToTop)
//             canvas_draw_triangle(c, axc, y - 1, 6, 5, dirs[i]);
//         else
//             canvas_draw_triangle(c, axc, y - 6, 6, 5, dirs[i]);
//         canvas_draw_str(c, x + 9, y, labels[i]);
//         x += w[i] + gap;
//     }
// }

// Basic-strategy grid. Each row string is 10 chars, one per dealer upcard
// 2 3 4 5 6 7 8 9 T A.  H hit  S stand  D double  P split.
static const char* hard_lab[] = {"9", "10", "11", "12", "13-16", "17+"};
static const char* hard_row[] =
    {"HDDDDHHHHH", "DDDDDDDDHH", "DDDDDDDDDH", "HHSSSHHHHH", "SSSSSHHHHH", "SSSSSSSSSS"};
static const char* soft_lab[] = {"A2-3", "A4-5", "A6", "A7", "A8", "A9"};
static const char* soft_row[] =
    {"HHHDDHHHHH", "HHDDDHHHHH", "HDDDDHHHHH", "SDDDDSSHHH", "SSSSSSSSSS", "SSSSSSSSSS"};
static const char* pra_lab[] = {"AA", "TT", "99", "88", "77"};
static const char* pra_row[] =
    {"PPPPPPPPPP", "SSSSSSSSSS", "PPPPPSPPSS", "PPPPPPPPPP", "PPPPPPHHHH"};
static const char* prb_lab[] = {"66", "55", "44", "33", "22"};
static const char* prb_row[] =
    {"PPPPPHHHHH", "DDDDDDDDHH", "HHHPPHHHHH", "PPPPPPHHHH", "PPPPPPHHHH"};

static const char* chart_h17[] = {
    "S17 vs H17",
    "(H17: hits soft 17)",
    "11 vA   H17 Double",
    "A7 v2   H17 Double",
    "A8 v6   H17 Double",
    "Else both identical.",
};

static const char* help0[] = {
    "OPTIMAL BLACKJACK",
    "Match basic strategy",
    "to score - not money,",
    "no count deviations.",
    "Hit:Left  Stand:Right",
    "Dbl:Up    Split:Down",
    "OK deal    Back menu",
};
static const char* help1[] = {
    "CHART  ( > on menu )",
    "H Hit     S Stand",
    "D Double  P Split",
    "Rows = your hand,",
    "cols = dealer 2 to A.",
    "Full app: Google Play",
    "Next page: scan QR",
};

// A tiny 3x5 pixel font, just for the chart cells and dealer header — the
// firmware's smallest font is still too tall to sit cleanly inside a 10x8 cell.
// Each glyph is 5 rows; the low 3 bits are the columns (bit2 left .. bit0 right).
static const uint8_t* tiny_glyph(char ch) {
    static const uint8_t gH[5] = {5, 5, 7, 5, 5};
    static const uint8_t gS[5] = {7, 4, 7, 1, 7};
    static const uint8_t gD[5] = {6, 5, 5, 5, 6};
    static const uint8_t gP[5] = {7, 5, 7, 4, 4};
    static const uint8_t gY[5] = {5, 5, 2, 2, 2};
    static const uint8_t gT[5] = {7, 2, 2, 2, 2};
    static const uint8_t gA[5] = {7, 5, 7, 5, 5};
    static const uint8_t g2[5] = {7, 1, 7, 4, 7};
    static const uint8_t g3[5] = {7, 1, 7, 1, 7};
    static const uint8_t g4[5] = {5, 5, 7, 1, 1};
    static const uint8_t g5[5] = {7, 4, 7, 1, 7};
    static const uint8_t g6[5] = {7, 4, 7, 5, 7};
    static const uint8_t g7[5] = {7, 1, 2, 2, 2};
    static const uint8_t g8[5] = {7, 5, 7, 5, 7};
    static const uint8_t g9[5] = {7, 5, 7, 1, 7};
    static const uint8_t g0[5] = {7, 5, 5, 5, 7};
    static const uint8_t g1[5] = {2, 6, 2, 2, 7};
    static const uint8_t gF[5] = {7, 4, 7, 4, 4};
    static const uint8_t gI[5] = {7, 2, 2, 2, 7};
    static const uint8_t gO[5] = {7, 5, 5, 5, 7};
    static const uint8_t gR[5] = {7, 5, 7, 6, 5};
    static const uint8_t gJ[5] = {1, 1, 1, 5, 2};
    static const uint8_t gQ[5] = {7, 5, 5, 7, 1}; // O loop + bottom-right tail
    static const uint8_t gK[5] = {5, 5, 6, 5, 5};
    static const uint8_t gPl[5] = {0, 2, 7, 2, 0};
    static const uint8_t gMi[5] = {0, 0, 7, 0, 0};
    switch(ch) {
    case 'H':
        return gH;
    case 'S':
        return gS;
    case 'D':
        return gD;
    case 'P':
        return gP;
    case 'Y':
        return gY;
    case 'T':
        return gT;
    case 'A':
        return gA;
    case '2':
        return g2;
    case '3':
        return g3;
    case '4':
        return g4;
    case '5':
        return g5;
    case '6':
        return g6;
    case '7':
        return g7;
    case '8':
        return g8;
    case '9':
        return g9;
    case '0':
        return g0;
    case '1':
        return g1;
    case 'F':
        return gF;
    case 'I':
        return gI;
    case 'O':
        return gO;
    case 'R':
        return gR;
    case 'J':
        return gJ;
    case 'Q':
        return gQ;
    case 'K':
        return gK;
    case '+':
        return gPl;
    case '-':
        return gMi;
    default:
        return NULL;
    }
}
// Draw a tiny glyph centred on (cx, cy).
static void draw_tiny_c(Canvas* c, int cx, int cy, char ch) {
    const uint8_t* g = tiny_glyph(ch);
    if(!g) {
        char s[2] = {ch, 0};
        canvas_draw_str_aligned(c, cx, cy + 3, AlignCenter, AlignBottom, s);
        return;
    }
    int x0 = cx - 1, y0 = cy - 2;
    for(int r = 0; r < 5; r++) {
        uint8_t row = g[r];
        if(row & 4) canvas_draw_dot(c, x0 + 0, y0 + r);
        if(row & 2) canvas_draw_dot(c, x0 + 1, y0 + r);
        if(row & 1) canvas_draw_dot(c, x0 + 2, y0 + r);
    }
}
// Draw a string in the tiny font, left edge at x, vertically centred on cy.
static void draw_tiny_str(Canvas* c, int x, int cy, const char* s) {
    for(const char* p = s; *p; p++) {
        if(*p != ' ') draw_tiny_c(c, x + 1, cy, *p);
        x += 4;
    }
}

static void draw_grid(
    Canvas* c,
    const char* title,
    const char** labels,
    const char** rows,
    int nrows,
    int page) {
    const int lw = 27, cw = 10, cols = 10;
    const int right = lw + cols * cw; // 127
    const int hdr = 8; // line under the dealer header
    const int rh = 7; // row height (was 8) — keeps the footer clear below
    const int bot = hdr + nrows * rh;
    static const char dc[10] = {'2', '3', '4', '5', '6', '7', '8', '9', 'T', 'A'};

    // title (label header cell) + dealer upcard header — all tiny, so they match
    draw_tiny_str(c, 2, 4, title);
    for(int j = 0; j < cols; j++)
        draw_tiny_c(c, lw + j * cw + cw / 2, 4, dc[j]);

    // full grid: outer frame, header rule, row separators, label divider,
    // and the vertical column dividers (the tiny letters leave room for them).
    canvas_draw_frame(c, 0, 0, right + 1, bot + 1);
    canvas_draw_line(c, 0, hdr, right, hdr);
    for(int r = 1; r < nrows; r++)
        canvas_draw_line(c, 0, hdr + r * rh, right, hdr + r * rh);
    canvas_draw_line(c, lw - 1, 0, lw - 1, bot);
    for(int j = 1; j < cols; j++)
        canvas_draw_line(c, lw + j * cw, 0, lw + j * cw, bot);

    // row labels + cells — all tiny glyphs
    for(int i = 0; i < nrows; i++) {
        int ym = hdr + i * rh + 4;
        draw_tiny_str(c, 2, ym, labels[i]);
        for(int j = 0; j < cols; j++)
            draw_tiny_c(c, lw + j * cw + cw / 2, ym, rows[i][j]);
    }

    draw_page_footer(c, page, 7, true);
}

// ── Deviations (Illustrious 18 + insurance), 6-deck indices ──
// Format: hand 'v' dealer (T=ten, A=ace), action, true-count index.
// Action: S stand, D double, P split, H hit. A negative/zero index on a hit
// means "hit at that count or below"; otherwise deviate at the count or above.
static const char* kDevS17[18] = {
    "16vT S+1",
    "15vT S+4",
    "TTv5 P+5",
    "TTv6 P+4",
    "10vT D+4",
    "12v3 S+2",
    "12v2 S+4",
    "11vA D+1",
    "9v2 D+1",
    "10vA D+3",
    "9v7 D+3",
    "16v9 S+5",
    "13v2 H-1",
    "12v4 H0",
    "12v5 H-1",
    "12v6 H-1",
    "13v3 H-2",
    "INS +3"};
static const char* kDevH17[18] = {
    "16vT S+1",
    "15vT S+4",
    "TTv5 P+5",
    "TTv6 P+4",
    "10vT D+4",
    "12v3 S+1",
    "12v2 S+3",
    "11vA H-1",
    "9v2 D+1",
    "10vA D+3",
    "9v7 D+3",
    "16v9 S+5",
    "13v2 H-1",
    "12v4 H0",
    "12v5 H-2",
    "12v6 H-3",
    "13v3 H-2",
    "INS +3"};

static void draw_dev_page(Canvas* c, bool h17, int page) {
    canvas_set_font(c, FontSecondary);
    const char** d = h17 ? kDevH17 : kDevS17;
    canvas_draw_str(c, 2, 8, h17 ? "DEVIATIONS  H17" : "DEVIATIONS  S17");
    canvas_draw_line(c, 0, 10, 127, 10);
    int start = (page == 5) ? 0 : 10;
    int end = (page == 5) ? 10 : 18;
    for(int i = start; i < end; i++) {
        int k = i - start;
        int x = 3 + (k / 5) * 64;
        int y = 19 + (k % 5) * 9;
        canvas_draw_str(c, x, y, d[i]);
    }
    draw_page_footer(c, page, 7, true);
}

// A menu label centred on cx at baseline ybase: boxed when it is its row's
// current pick, and filled/inverted when that row currently has focus.
static void menu_chip(Canvas* c, int cx, int ybase, const char* s, bool selected, bool focused) {
    int w = canvas_string_width(c, s);
    int x = cx - w / 2;
    if(selected && focused) {
        canvas_draw_box(c, x - 3, ybase - 9, w + 6, 12);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str(c, x, ybase, s);
        canvas_set_color(c, ColorBlack);
    } else if(selected) {
        canvas_draw_rframe(c, x - 3, ybase - 9, w + 6, 12, 2);
        canvas_draw_str(c, x, ybase, s);
    } else {
        canvas_draw_str(c, x, ybase, s);
    }
}

// Lifetime stats screen (reached from the menu's Stats tool). Two pages:
// 0 = the table game, 1 = counting + personal records. Up/Dn/L/R flips pages.
static void draw_stats(Canvas* c, App* a) {
    char line[40];
    if(a->chart_page == 0) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 2, 9, "TABLE STATS");
        canvas_set_font(c, FontSecondary);
        const int yr[4] = {20, 29, 38, 47};
        snprintf(line, sizeof(line), "Rounds %lu", (unsigned long)a->lt_rounds);
        canvas_draw_str(c, 2, yr[0], line);
        snprintf(line, sizeof(line), "Win %lu", (unsigned long)a->lt_wins);
        canvas_draw_str(c, 2, yr[1], line);
        snprintf(line, sizeof(line), "Lose %lu", (unsigned long)a->lt_losses);
        canvas_draw_str(c, 2, yr[2], line);
        snprintf(line, sizeof(line), "Push %lu", (unsigned long)a->lt_pushes);
        canvas_draw_str(c, 2, yr[3], line);
        snprintf(line, sizeof(line), "BJ %lu", (unsigned long)a->lt_bj);
        canvas_draw_str(c, 66, yr[0], line);
        snprintf(line, sizeof(line), "Bust %lu", (unsigned long)a->lt_busts);
        canvas_draw_str(c, 66, yr[1], line);
        snprintf(line, sizeof(line), "DlrBst %lu", (unsigned long)a->lt_dbusts);
        canvas_draw_str(c, 66, yr[2], line);
        snprintf(
            line,
            sizeof(line),
            "D%lu S%lu",
            (unsigned long)a->lt_doubles,
            (unsigned long)a->lt_splits);
        canvas_draw_str(c, 66, yr[3], line);
        unsigned long p =
            a->lt_dec ? (unsigned long)((a->lt_dec_ok * 100 + a->lt_dec / 2) / a->lt_dec) : 0;
        snprintf(
            line,
            sizeof(line),
            "Play %lu/%lu  %lu%%",
            (unsigned long)a->lt_dec_ok,
            (unsigned long)a->lt_dec,
            p);
        canvas_draw_str(c, 2, 56, line);
    } else {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 2, 9, "COUNT STATS");
        canvas_set_font(c, FontSecondary);
        unsigned long p =
            a->ct_total ? (unsigned long)((a->ct_correct * 100 + a->ct_total / 2) / a->ct_total) :
                          0;
        snprintf(
            line,
            sizeof(line),
            "Trainer %lu/%lu  %lu%%",
            (unsigned long)a->ct_correct,
            (unsigned long)a->ct_total,
            p);
        canvas_draw_str(c, 2, 20, line);
        snprintf(line, sizeof(line), "Best stage %u / %u", a->ct_bestlevel, CT_MAX_LEVEL);
        canvas_draw_str(c, 2, 29, line);
        snprintf(line, sizeof(line), "Best endless %lu", (unsigned long)a->ct_best);
        canvas_draw_str(c, 2, 38, line);
        snprintf(line, sizeof(line), "Best streak %lu", (unsigned long)a->lt_beststreak);
        canvas_draw_str(c, 2, 47, line);
        snprintf(
            line,
            sizeof(line),
            "Insurance %lu/%lu",
            (unsigned long)a->lt_ins_ok,
            (unsigned long)a->lt_ins);
        canvas_draw_str(c, 2, 56, line);
    }
    draw_page_footer(c, a->chart_page, 2, true);
}

// ── Advantage Analysis tool ────────────────────────────────────────────────
// A card counter's edge calculator. The house-edge rule deltas are standard
// published values (Wizard of Odds / Griffin); the Hi-Lo true-count frequency +
// mean-shoe-edge tables (aa_data.h) and the deviation-value curve are measured
// blackjack statistics. Combined per bucket for a single spot. Integers only for
// display -- newlib-nano's snprintf has no %f.
static const uint8_t AA_DECKS[5] = {1, 2, 4, 6, 8};
static const uint8_t AA_PEN[9] = {50, 55, 60, 65, 70, 75, 80, 85, 90};
static const uint16_t AA_BANK[9] = {500, 1000, 2000, 3000, 5000, 7500, 10000, 15000, 25000};
static const uint16_t AA_UNIT[8] = {5, 10, 15, 25, 50, 100, 200, 500};
static const uint8_t AA_SPREAD[7] = {4, 6, 8, 10, 12, 15, 20};
static const uint16_t AA_HPH[6] = {50, 100, 150, 200, 300, 400}; // hands per hour
static const uint8_t AA_SKILL[5] = {100, 99, 97, 94, 90}; // play accuracy %
static const char* AA_DEVSET[4] = {"Off", "I18", "Nifty 50", "All"};

static int aa_round(float x) {
    return (int)(x < 0 ? x - 0.5f : x + 0.5f);
}

// Piecewise-linear interpolation over DESCENDING x anchors (100..10), from the
// app's bsAccuracyPenaltyPct / devAccuracyMultiplier.
static float aa_interp(const float* xs, const float* ys, int n, float x) {
    if(x >= xs[0]) return ys[0];
    if(x <= xs[n - 1]) return ys[n - 1];
    for(int i = 0; i < n - 1; i++) {
        if(x <= xs[i] && x >= xs[i + 1]) {
            float frac = (xs[i] - x) / (xs[i] - xs[i + 1]);
            return ys[i] + frac * (ys[i + 1] - ys[i]);
        }
    }
    return ys[n - 1];
}
// House-edge penalty for imperfect basic strategy (added to house edge).
static float aa_bs_penalty(float acc) {
    static const float X[5] = {100, 90, 60, 30, 10}, Y[5] = {0.0f, 0.40f, 0.80f, 1.20f, 1.80f};
    return aa_interp(X, Y, 5, acc);
}
// How much of the deviation value you capture at a given play accuracy
// (negative at very low accuracy: misplayed indices cost more than they earn).
static float aa_dev_acc_mult(float acc) {
    static const float X[5] = {100, 90, 60, 30, 10}, Y[5] = {1.0f, 0.95f, 0.75f, 0.40f, -0.15f};
    return aa_interp(X, Y, 5, acc);
}
// Fraction of total index value by how many deviations you know (devSetMultiplier).
static float aa_dev_set_mult(int s) {
    static const float M[4] = {0.0f, 0.428f, 0.851f, 1.0f};
    return M[s];
}

// Off-the-top house edge (%): a measured deck base + resplit-to-4 (-0.008) and
// standard published rule deltas (Wizard of Odds / Griffin).
static float aa_house_edge(App* a) {
    static const float base[5] = {0.017f, 0.167f, 0.328f, 0.394f, 0.434f}; // {1,2,4,6,8} decks
    float he = base[a->aa_decks_i] - 0.008f; // resplitEdgeDelta(4 hands)
    if(a->aa_h17) he += 0.22f; // hit soft 17
    if(!a->aa_das) he += 0.14f; // no double after split
    // doubling restriction: any / 9-11 / 10-11 / 11-only
    if(a->aa_double_i == 1)
        he += 0.09f;
    else if(a->aa_double_i == 2)
        he += 0.18f;
    else if(a->aa_double_i == 3)
        he += 0.28f;
    // surrender: none / late / early-v-10 / early-v-ace / full early
    if(a->aa_surr_i == 1)
        he -= 0.08f;
    else if(a->aa_surr_i == 2)
        he -= 0.24f;
    else if(a->aa_surr_i == 3)
        he -= 0.39f;
    else if(a->aa_surr_i == 4)
        he -= 0.63f;
    // blackjack payout: 3:2 / 6:5 / 1:1
    if(a->aa_bj_i == 1)
        he += 1.39f;
    else if(a->aa_bj_i == 2)
        he += 2.26f;
    if(a->aa_nopeek) he += 0.11f; // no hole card / ENHC
    // rare rules (all measured by the exact engine / simulator)
    if(a->aa_rsa) he -= 0.08f; // resplit aces
    if(a->aa_hsa) he -= 0.152f; // hit split aces
    if(a->aa_dsa) he -= a->aa_hsa ? 0.008f : 0.072f; // double split aces
    if(a->aa_dah) he -= 0.229f; // double after hit
    if(a->aa_nosoftdbl) he += 0.115f; // no soft-hand doubling
    if(a->aa_push22) he += 6.974f; // dealer 22 pushes
    if(a->aa_bjtie) he -= 0.217f; // player wins blackjack ties
    he += aa_bs_penalty((float)AA_SKILL[a->aa_skill_i]); // imperfect-play penalty
    if(he < -10.0f) he = -10.0f; // app clamps to [-10, 10]
    if(he > 10.0f) he = 10.0f;
    return he;
}

// Per-unit variance (bet^2), measured by rule set.
static float aa_variance(App* a) {
    float v = 1.35f;
    if(a->aa_surr_i != 0) v -= 0.030f; // any surrender
    if(a->aa_double_i == 3)
        v -= 0.119f; // 11-only
    else if(a->aa_double_i == 2)
        v -= 0.037f; // 10-11
    else if(a->aa_double_i == 1)
        v -= 0.010f; // 9-11
    if(v < 1.10f) v = 1.10f;
    if(v > 1.35f) v = 1.35f;
    return v;
}

// Measured deviation (index-play) value by true count (complete index set,
// expert play). Added as a flat per-hand term, frequency-weighted only
// (NOT bet-weighted, which would double-count the high-count tilt).
static float aa_dev_value(int tc) {
    static const float D[22] = {// tc -6 .. +15
                                1.06467f, 0.75946f, 0.51429f, 0.30228f, 0.13414f, 0.03084f,
                                0.00000f, 0.02826f, 0.14103f, 0.30634f, 0.54461f, 0.85858f,
                                1.22805f, 1.64973f, 2.10773f, 2.62236f, 3.17126f, 3.77928f,
                                4.39341f, 5.01836f, 5.65468f, 6.29236f};
    if(tc < -6) tc = -6;
    if(tc > 15) tc = 15;
    return D[tc + 6];
}

typedef struct {
    int house_bp; // off-the-top house edge, hundredths of a percent
    int edge_bp; // your overall edge (% of avg bet), hundredths of a percent
    int per100; // $ per 100 hands (signed)
    int perhr; // $ per hour (ev x hands/hour, signed)
    int avgbet; // $ average bet
    uint32_t n0; // hands to overcome one SD of variance (0 when edge <= 0)
    int ror; // lifetime risk of ruin %, -1 means ">99"
} AaResult;

static void aa_compute(App* a, AaResult* r) {
    float he = aa_house_edge(a);
    float unit = (float)AA_UNIT[a->aa_unit_i];
    int spr = AA_SPREAD[a->aa_spread_i];
    // Measured Hi-Lo tables: our deck/pen presets ARE the table's keys, so the
    // row index is the setting index -- no interpolation.
    const float* freq = AA_FREQ[a->aa_decks_i][a->aa_pen_i];
    const float* sedge = AA_EDGE[a->aa_decks_i][a->aa_pen_i];

    float sf = 0, sbet = 0, sev = 0, sbet2 = 0; // count edge, bet-weighted
    float devnum = 0, devden = 0; // deviation value, freq-weighted
    for(int i = 0; i < AA_NB; i++) {
        float f = freq[i];
        if(f <= 0.0f) continue;
        int tc = AA_TCMIN + i;
        float bet = unit; // flat min bet at TC <= 1; ramp up with the count
        if(tc >= 2) {
            float b = unit * (tc - 1), cap = unit * spr;
            bet = b > cap ? cap : b;
        }
        float edgefrac = (sedge[i] - he) / 100.0f; // count edge minus house edge
        sf += f;
        sbet += f * bet;
        sev += f * bet * edgefrac;
        sbet2 += f * bet * bet;
        if(tc >= -6 && tc <= 15) {
            devnum += f * aa_dev_value(tc);
            devden += f;
        }
    }
    if(sf <= 0.0f) sf = 1.0f;
    float avgbet = sbet / sf;
    float countEdgePct = sbet > 0 ? sev / sbet * 100.0f : 0.0f;
    // Deviation value scaled by which indices you know x how well you play them.
    float devmult =
        aa_dev_set_mult(a->aa_dev_set_i) * aa_dev_acc_mult((float)AA_SKILL[a->aa_skill_i]);
    float devEvPct = (devden > 0 ? devnum / devden : 0.0f) * devmult;
    float edgePct = countEdgePct + devEvPct; // player edge (app formula)
    float varhand = aa_variance(a) * (sbet2 / sf); // variance per hand
    float ev = edgePct / 100.0f * avgbet; // $ per hand

    r->house_bp = aa_round(he * 100.0f);
    r->edge_bp = aa_round(edgePct * 100.0f);
    r->per100 = aa_round(ev * 100.0f);
    r->perhr = aa_round(ev * (float)AA_HPH[a->aa_hph_i]);
    r->avgbet = aa_round(avgbet);
    if(ev > 0.0f) {
        float n0f = varhand / (ev * ev);
        if(n0f > 9.9e8f) n0f = 9.9e8f;
        r->n0 = (uint32_t)(n0f + 0.5f);
        float ror = expf(-2.0f * ev * (float)AA_BANK[a->aa_bank_i] / varhand) * 100.0f;
        if(ror < 0.0f) ror = 0.0f;
        if(ror > 100.0f) ror = 100.0f;
        r->ror = aa_round(ror);
    } else {
        r->n0 = 0;
        r->ror = -1;
    }
}

// Settings by global id 0..20 (grouped into categories by AA_CATSTART).
static const char* aa_set_label(int id) {
    static const char* N[AA_NSETTING] = {
        "Decks",      "Dealer",   "DAS",       "Double",    "BJ pay", // table 0-4
        "Surr",       "Peek",     "Resplit A", "Hit spl A", "Dbl spl A", // split&surr 5-9
        "Dbl on hit", "Soft dbl", "22 pushes", "BJ ties", // rare 10-13
        "Bankroll",   "Unit",     "Spread", // bankroll 14-16
        "Pen %",      "Hands/hr", "Deviate",   "Play skill"}; // count+skill 17-20
    return N[id];
}
static void aa_set_value(App* a, int id, char* v, size_t n) {
    static const char* DBL[4] = {"Any", "9-11", "10-11", "11"};
    static const char* SUR[5] = {"No", "Late", "ES10", "ESA", "Full"};
    static const char* BJ[3] = {"3:2", "6:5", "1:1"};
    switch(id) {
    case 0:
        snprintf(v, n, "%u", AA_DECKS[a->aa_decks_i]);
        break;
    case 1:
        snprintf(v, n, "%s", a->aa_h17 ? "H17" : "S17");
        break;
    case 2:
        snprintf(v, n, "%s", a->aa_das ? "On" : "Off");
        break;
    case 3:
        snprintf(v, n, "%s", DBL[a->aa_double_i]);
        break;
    case 4:
        snprintf(v, n, "%s", BJ[a->aa_bj_i]);
        break;
    case 5:
        snprintf(v, n, "%s", SUR[a->aa_surr_i]);
        break;
    case 6:
        snprintf(v, n, "%s", a->aa_nopeek ? "No" : "Yes");
        break; // dealer peeks?
    case 7:
        snprintf(v, n, "%s", a->aa_rsa ? "On" : "Off");
        break;
    case 8:
        snprintf(v, n, "%s", a->aa_hsa ? "On" : "Off");
        break;
    case 9:
        snprintf(v, n, "%s", a->aa_dsa ? "On" : "Off");
        break;
    case 10:
        snprintf(v, n, "%s", a->aa_dah ? "On" : "Off");
        break;
    case 11:
        snprintf(v, n, "%s", a->aa_nosoftdbl ? "Off" : "On");
        break; // soft doubling
    case 12:
        snprintf(v, n, "%s", a->aa_push22 ? "On" : "Off");
        break;
    case 13:
        snprintf(v, n, "%s", a->aa_bjtie ? "On" : "Off");
        break;
    case 14:
        snprintf(v, n, "$%u", (unsigned)AA_BANK[a->aa_bank_i]);
        break;
    case 15:
        snprintf(v, n, "$%u", (unsigned)AA_UNIT[a->aa_unit_i]);
        break;
    case 16:
        snprintf(v, n, "1-%u", AA_SPREAD[a->aa_spread_i]);
        break;
    case 17:
        snprintf(v, n, "%u%%", AA_PEN[a->aa_pen_i]);
        break;
    case 18:
        snprintf(v, n, "%u", AA_HPH[a->aa_hph_i]);
        break;
    case 19:
        snprintf(v, n, "%s", AA_DEVSET[a->aa_dev_set_i]);
        break;
    default:
        snprintf(v, n, "%u%%", AA_SKILL[a->aa_skill_i]);
        break;
    }
}
static void aa_set_edit(App* a, int id, int dir) {
#define AA_CLAMP(fld, mx)           \
    do {                            \
        int _v = (int)a->fld + dir; \
        if(_v < 0) _v = 0;          \
        if(_v > (mx)) _v = (mx);    \
        a->fld = (uint8_t)_v;       \
    } while(0)
    switch(id) {
    case 0:
        AA_CLAMP(aa_decks_i, 4);
        break;
    case 1:
        a->aa_h17 = !a->aa_h17;
        break;
    case 2:
        a->aa_das = !a->aa_das;
        break;
    case 3:
        AA_CLAMP(aa_double_i, 3);
        break;
    case 4:
        AA_CLAMP(aa_bj_i, 2);
        break;
    case 5:
        AA_CLAMP(aa_surr_i, 4);
        break;
    case 6:
        a->aa_nopeek = !a->aa_nopeek;
        break;
    case 7:
        a->aa_rsa = !a->aa_rsa;
        break;
    case 8:
        a->aa_hsa = !a->aa_hsa;
        break;
    case 9:
        a->aa_dsa = !a->aa_dsa;
        break;
    case 10:
        a->aa_dah = !a->aa_dah;
        break;
    case 11:
        a->aa_nosoftdbl = !a->aa_nosoftdbl;
        break;
    case 12:
        a->aa_push22 = !a->aa_push22;
        break;
    case 13:
        a->aa_bjtie = !a->aa_bjtie;
        break;
    case 14:
        AA_CLAMP(aa_bank_i, 8);
        break;
    case 15:
        AA_CLAMP(aa_unit_i, 7);
        break;
    case 16:
        AA_CLAMP(aa_spread_i, 6);
        break;
    case 17:
        AA_CLAMP(aa_pen_i, 8);
        break;
    case 18:
        AA_CLAMP(aa_hph_i, 5);
        break;
    case 19:
        AA_CLAMP(aa_dev_set_i, 3);
        break;
    default:
        AA_CLAMP(aa_skill_i, 4);
        break;
    }
#undef AA_CLAMP
}

// Two pages: 0 = editable inputs (a category tab on top, settings below), 1 =
// computed results. OK toggles; on results, any key returns. Back -> menu.
static void draw_advantage(Canvas* c, App* a) {
    char line[40], val[16];
    if(a->aa_page == 0) {
        canvas_set_font(c, FontSecondary);
        int cat = a->aa_cat, count = AA_CATCOUNT[cat], start = AA_CATSTART[cat];
        // category tab (Left/Right), highlighted when the cursor is on it
        const char* cn = AA_CATNAME[cat];
        menu_chip(c, 64, 9, cn, true, a->aa_row == 0);
        int cw = canvas_string_width(c, cn);
        canvas_draw_triangle(c, 64 - cw / 2 - 7, 6, 5, 6, CanvasDirectionRightToLeft);
        canvas_draw_triangle(c, 64 + cw / 2 + 7, 6, 5, 6, CanvasDirectionLeftToRight);
        canvas_draw_line(c, 0, 12, 127, 12);
        for(int i = 0; i < count; i++) { // <=5 rows at 8 px: readable, no overlap
            int id = start + i, y = 21 + i * 8;
            if(a->aa_row == i + 1) canvas_draw_str(c, 2, y, ">");
            canvas_draw_str(c, 8, y, aa_set_label(id));
            aa_set_value(a, id, val, sizeof(val));
            canvas_draw_str_aligned(c, 124, y, AlignRight, AlignBottom, val);
        }
        canvas_draw_str(c, 1, 63, "Back menu");
        canvas_draw_str_aligned(c, 127, 63, AlignRight, AlignBottom, "OK results");
    } else {
        AaResult r;
        aa_compute(a, &r);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 2, 9, "ADVANTAGE");
        canvas_set_font(c, FontSecondary);
        snprintf(line, sizeof(line), "House edge  %d.%02d%%", r.house_bp / 100, r.house_bp % 100);
        canvas_draw_str(c, 2, 18, line);
        {
            int v = r.edge_bp < 0 ? -r.edge_bp : r.edge_bp;
            snprintf(
                line,
                sizeof(line),
                "Your edge  %s%d.%02d%%",
                r.edge_bp < 0 ? "-" : "+",
                v / 100,
                v % 100);
        }
        canvas_draw_str(c, 2, 27, line);
        snprintf(
            line,
            sizeof(line),
            "Per hour  %s$%d",
            r.perhr < 0 ? "-" : "+",
            r.perhr < 0 ? -r.perhr : r.perhr);
        canvas_draw_str(c, 2, 36, line);
        if(r.n0 == 0)
            snprintf(line, sizeof(line), "Avg bet $%d  N0 -", r.avgbet);
        else if(r.n0 >= 10000)
            snprintf(
                line, sizeof(line), "Avg bet $%d  N0 %luk", r.avgbet, (unsigned long)(r.n0 / 1000));
        else
            snprintf(line, sizeof(line), "Avg bet $%d  N0 %lu", r.avgbet, (unsigned long)r.n0);
        canvas_draw_str(c, 2, 45, line);
        if(r.ror < 0)
            snprintf(line, sizeof(line), "Risk of ruin  >99%%");
        else if(r.ror == 0)
            snprintf(line, sizeof(line), "Risk of ruin  <1%%");
        else
            snprintf(line, sizeof(line), "Risk of ruin  %d%%", r.ror);
        canvas_draw_str(c, 2, 54, line);
        canvas_draw_str(c, 1, 63, "Back menu");
        canvas_draw_str_aligned(c, 127, 63, AlignRight, AlignBottom, "any:edit");
    }
}

// Strategy Flashcards: laid out exactly like the training table (D/Y rows,
// centred cards, one action row). Phase 0 asks the play, phase 1 shows the verdict.
static void draw_flash(Canvas* c, App* a) {
    char buf[32];
    canvas_set_font(c, FontSecondary);
    // header — same shape as the table
    snprintf(buf, sizeof(buf), "Str %lu", (unsigned long)a->fc_streak);
    canvas_draw_str(c, 2, 8, buf);
    canvas_draw_str_aligned(c, 64, 8, AlignCenter, AlignBottom, a->h17 ? "H17" : "S17");
    snprintf(buf, sizeof(buf), "Best %lu", (unsigned long)a->fc_best);
    canvas_draw_str_aligned(c, 126, 8, AlignRight, AlignBottom, buf);

    const int labelw = 9;
    int t;
    bool s;
    hand_total(a->fc_cards, a->fc_n, &t, &s);
    char ptot[8];
    snprintf(ptot, sizeof(ptot), "%d", t);
    int ptotw = 3 + canvas_string_width(c, ptot);
    int dgroup = labelw + 2 * 15 - 2; // dealer: upcard + hidden
    int pgroup = labelw + a->fc_n * 15 - 2 + ptotw; // player: cards + total
    int groupw = dgroup > pgroup ? dgroup : pgroup;
    int lx = 6 + (121 - 6 - groupw) / 2;
    if(lx < 6) lx = 6;

    // dealer row (upcard + face-down), like the table mid-hand
    canvas_draw_str(c, lx, 22, "D");
    int x = lx + labelw;
    draw_card_box(c, x, 11, a->fc_up, a->fc_up % 4);
    x += 15;
    draw_hidden_box(c, x, 11);

    // player row (your cards + total)
    canvas_draw_str(c, lx, 40, "Y");
    x = lx + labelw;
    for(int i = 0; i < a->fc_n; i++) {
        draw_card_box(c, x, 29, a->fc_cards[i], (a->fc_cards[i] + i) % 4);
        x += 15;
    }
    canvas_draw_str(c, x - 1, 40, ptot);

    if(a->fc_phase == 0) {
        // one centred action row — the table's Hit / Stay / x2 / Spl
        CanvasDirection adir[4];
        const char* alab[4];
        int na = 0;
        adir[na] = CanvasDirectionRightToLeft;
        alab[na] = "Hit";
        na++;
        adir[na] = CanvasDirectionLeftToRight;
        alab[na] = "Stay";
        na++;
        adir[na] = CanvasDirectionBottomToTop;
        alab[na] = "x2";
        na++;
        if(a->fc_canY) {
            adir[na] = CanvasDirectionTopToBottom;
            alab[na] = "Spl";
            na++;
        }
        int atotal = 0;
        const int agap = 6;
        for(int i = 0; i < na; i++)
            atotal += 7 + canvas_string_width(c, alab[i]);
        atotal += agap * (na - 1);
        int ax = (128 - atotal) / 2;
        if(ax < 6) ax = 6;
        for(int i = 0; i < na; i++) {
            draw_action(c, ax, 59, adir[i], alab[i]);
            ax += 7 + canvas_string_width(c, alab[i]) + agap;
        }
    } else {
        // verdict + the correct play, then OK to continue
        snprintf(
            buf,
            sizeof(buf),
            "%s  %s",
            a->fc_right ? "Correct!" : "No -",
            act_word((Action)a->fc_correct));
        canvas_draw_str_aligned(c, 64, 55, AlignCenter, AlignBottom, buf);
        canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "OK: next    Back menu");
    }
}

// Help page 3: the QR code to the Google Play listing (dark modules on the
// light screen; centred with a quiet zone so a phone can scan it).
static void draw_qr(Canvas* c) {
    int x0 = (128 - QR_SIZE) / 2, y0 = 11;
    for(int r = 0; r < QR_SIZE; r++)
        for(int col = 0; col < QR_SIZE; col++)
            if(QR_BITS[r][col >> 3] & (0x80 >> (col & 7))) canvas_draw_dot(c, x0 + col, y0 + r);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 54, AlignCenter, AlignBottom, "Scan: get the app");
    draw_page_footer(c, 2, 3, false);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* a = ctx;
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    canvas_clear(canvas);
    char buf[64];

    if(a->state == StateMenu) {
        static const char* modes[5] = {
            "Play Table", "Count Trainer", "Endless Count", "Advantage", "Flashcards"};
        static const char* tools[4] = {"Chart", "Stats", "Setup", "Help"};
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignBottom, "OPTIMAL BLACKJACK");

        // MODE row — Left/Right, boxed, with chevrons
        const char* m = modes[a->menu_sel];
        int mw = canvas_string_width(canvas, m);
        menu_chip(canvas, 64, 24, m, true, a->menu_focus == 0);
        // chevrons on the MODE row only while it is the focused row (Left/Right)
        if(a->menu_focus == 0) {
            int bl = 64 - mw / 2 - 3, br = 64 + mw / 2 + 3;
            canvas_draw_triangle(canvas, bl - 5, 21, 5, 7, CanvasDirectionRightToLeft);
            canvas_draw_triangle(canvas, br + 5, 21, 5, 7, CanvasDirectionLeftToRight);
        }

        // status for the selected mode: your current stage / best, so you see it
        // before starting
        char status[40];
        if(a->menu_sel == 0) {
            if(a->total > 0) {
                unsigned long pct = (a->correct * 100 + a->total / 2) / a->total;
                snprintf(
                    status,
                    sizeof(status),
                    "Streak %lu    Acc %lu%%",
                    (unsigned long)a->streak,
                    pct);
            } else {
                snprintf(status, sizeof(status), "Basic-strategy game");
            }
        } else if(a->menu_sel == 1) {
            uint8_t lv = a->ct_level < 1 ? 1 : a->ct_level;
            snprintf(
                status,
                sizeof(status),
                "Stage %u / %u   Best %u",
                lv,
                CT_MAX_LEVEL,
                a->ct_bestlevel);
        } else if(a->menu_sel == 2) {
            snprintf(status, sizeof(status), "Best streak %lu", (unsigned long)a->ct_best);
        } else if(a->menu_sel == 3) { // Advantage: live edge for the current settings
            AaResult r;
            aa_compute(a, &r);
            int v = r.edge_bp < 0 ? -r.edge_bp : r.edge_bp;
            snprintf(
                status,
                sizeof(status),
                "Edge %s%d.%02d%%",
                r.edge_bp < 0 ? "-" : "+",
                v / 100,
                v % 100);
        } else { // Flashcards
            snprintf(
                status,
                sizeof(status),
                "Streak %lu   Best %lu",
                (unsigned long)a->fc_streak,
                (unsigned long)a->fc_best);
        }
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignBottom, status);

        // TOOL row — four labels; Left/Right pick when this row is focused. The
        // gap is derived from the measured widths so the row always fits 128 px.
        const int ntools = 4;
        int tw[4], twsum = 0;
        for(int i = 0; i < ntools; i++) {
            tw[i] = canvas_string_width(canvas, tools[i]);
            twsum += tw[i];
        }
        int tgap = (124 - twsum) / (ntools - 1);
        if(tgap > 7) tgap = 7;
        if(tgap < 2) tgap = 2;
        int tx = (128 - (twsum + tgap * (ntools - 1))) / 2;
        if(tx < 1) tx = 1;
        for(int i = 0; i < ntools; i++) {
            int cx = tx + tw[i] / 2;
            menu_chip(canvas, cx, 46, tools[i], i == a->menu_tool, a->menu_focus == 1);
            tx += tw[i] + tgap;
        }

        canvas_draw_str_aligned(
            canvas,
            64,
            63,
            AlignCenter,
            AlignBottom,
            a->menu_focus == 0 ? "L/R mode   Dn: tools" : "L/R tool   Up: modes");
    } else if(a->state == StateChart) {
        if(a->chart_page == 0)
            draw_grid(canvas, "HARD", hard_lab, hard_row, 6, 0);
        else if(a->chart_page == 1)
            draw_grid(canvas, "SOFT", soft_lab, soft_row, 6, 1);
        else if(a->chart_page == 2)
            draw_grid(canvas, "PAIRS", pra_lab, pra_row, 5, 2);
        else if(a->chart_page == 3)
            draw_grid(canvas, "PAIRS", prb_lab, prb_row, 5, 3);
        else if(a->chart_page == 4) {
            canvas_set_font(canvas, FontSecondary);
            for(int i = 0; i < 6; i++)
                canvas_draw_str(canvas, 2, 9 + i * 9, chart_h17[i]);
            draw_page_footer(canvas, 4, 7, true);
        } else {
            draw_dev_page(canvas, a->h17, a->chart_page);
        }
    } else if(a->state == StateHelp) {
        if(a->chart_page == 2) {
            draw_qr(canvas);
        } else {
            canvas_set_font(canvas, FontSecondary);
            const char** pg = a->chart_page ? help1 : help0;
            for(int i = 0; i < 7; i++)
                canvas_draw_str(canvas, 2, 8 + i * 8, pg[i]);
            draw_page_footer(canvas, a->chart_page, 3, false);
        }
    } else if(a->state == StateSettings) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 9, "SETTINGS");
        canvas_set_font(canvas, FontSecondary);
        const char* names[6] = {"Vibro", "Sound", "LED", "Dealer", "Count", "Reset"};
        char resetv[24];
        snprintf(
            resetv, sizeof(resetv), "%lu/%lu", (unsigned long)a->correct, (unsigned long)a->total);
        const char* vals[6];
        vals[0] = a->vibro ? "On" : "Off";
        vals[1] = a->sound ? "On" : "Off";
        vals[2] = a->led ? "On" : "Off";
        vals[3] = a->h17 ? "H17" : "S17";
        vals[4] = a->count_mode == 0 ? "Off" : (a->count_mode == 1 ? "RC" : "RC+TC");
        vals[5] = resetv;
        for(int i = 0; i < 6; i++) {
            int y = 18 + i * 7;
            if(i == a->set_row) canvas_draw_str(canvas, 2, y, ">");
            canvas_draw_str(canvas, 12, y, names[i]);
            canvas_draw_str(canvas, 58, y, vals[i]);
        }
        canvas_draw_str(canvas, 2, 63, "OK sets    Back menu");
    } else if(a->state == StateStats) {
        draw_stats(canvas, a);
    } else if(a->state == StateAdvantage) {
        draw_advantage(canvas, a);
    } else if(a->state == StateFlash) {
        draw_flash(canvas, a);
    } else if(a->state == StateCount) {
        canvas_set_font(canvas, FontSecondary);
        char hdr[20], hb[20];
        if(a->ct_endless) {
            snprintf(hdr, sizeof(hdr), "ENDLESS");
            snprintf(hb, sizeof(hb), "Best %lu", (unsigned long)a->ct_best);
        } else {
            snprintf(hdr, sizeof(hdr), "STAGE %u", a->ct_level);
            snprintf(hb, sizeof(hb), "Best %u", a->ct_bestlevel);
        }
        canvas_draw_str(canvas, 2, 8, hdr);
        canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, hb);

        if(a->ct_phase == 0) { // flashing a card
            int cw = 30, chh = 40, x0 = (128 - cw) / 2, y0 = 12;
            canvas_draw_rframe(canvas, x0, y0, cw, chh, 4);
            char rr[3];
            rank_str(a->ct_card, rr);
            draw_big_c(canvas, 64, y0 + 14, rr[0], 3);
            draw_suit_s(canvas, 64 - 5, y0 + 24, a->ct_suit, 2);
            char pg[16];
            snprintf(pg, sizeof(pg), "%u/%u", a->ct_shown, a->ct_cards);
            canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, pg);
        } else if(a->ct_phase == 1) { // enter the running count
            canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignBottom, "Running count?");
            char g[12];
            snprintf(g, sizeof(g), "%+d", a->ct_guess);
            int s = 4, n = (int)strlen(g);
            int gw = n * (3 * s) + (n - 1) * 2;
            int gx = 64 - gw / 2 + (3 * s) / 2;
            for(char* p = g; *p; p++) {
                draw_big_c(canvas, gx, 40, *p, s);
                gx += 3 * s + 2;
            }
            canvas_draw_str_aligned(
                canvas, 64, 62, AlignCenter, AlignBottom, "Up/Dn set   OK submit");
        } else { // feedback
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, a->fb);
            char sb[28];
            if(a->ct_endless)
                snprintf(sb, sizeof(sb), "Streak %lu", (unsigned long)a->ct_streak);
            else
                snprintf(sb, sizeof(sb), "Stage %u", a->ct_level);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, sb);
            canvas_draw_str_aligned(
                canvas,
                64,
                62,
                AlignCenter,
                AlignBottom,
                (a->ct_endless && a->ct_gameover) ? "Game over - OK retry" : "OK: next");
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        // header: streak (left), mode/prompt (center), best (right)
        snprintf(buf, sizeof(buf), "Str %lu", (unsigned long)a->streak);
        canvas_draw_str(canvas, 2, 8, buf);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignBottom, a->h17 ? "H17" : "S17");
        snprintf(buf, sizeof(buf), "Best %lu", (unsigned long)a->best);
        canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, buf);

        // 6-deck shoe on the RIGHT (depletes) + discard tray on the LEFT (builds up)
        {
            const int topY = 12, botY = 47, innerH = botY - topY;
            int dealt = a->shoe_dealt > 312 ? 312 : a->shoe_dealt;
            int remain = 312 - dealt;
            // shoe: an outlined box filled from the bottom with the cards left
            int sx0 = 123, sx1 = 127;
            canvas_draw_frame(canvas, sx0, topY, sx1 - sx0 + 1, innerH + 1);
            int fillH = remain * (innerH - 1) / 312;
            if(fillH > 0) canvas_draw_box(canvas, sx0 + 1, botY - fillH, sx1 - sx0 - 1, fillH);
            int yc = botY - (innerH - 1) / 4; // cut card (~75% penetration)
            canvas_draw_line(canvas, sx0 - 2, yc, sx1, yc);
            // discard tray: a pile of card-edges growing up with the cards dealt
            int dx0 = 0, dx1 = 4;
            int pileH = dealt * (innerH - 1) / 312;
            canvas_draw_line(canvas, dx0, botY, dx1, botY);
            for(int yy = botY - 1; yy >= botY - pileH && yy >= topY; yy -= 2)
                canvas_draw_line(canvas, dx0, yy, dx1, yy);
        }

        // running / true count readout on the right, per the Count setting
        if(a->count_mode > 0) {
            canvas_draw_str(canvas, 102, 20, "RC");
            snprintf(buf, sizeof(buf), "%+d", (int)a->rc);
            canvas_draw_str(canvas, 102, 29, buf);
            if(a->count_mode == 2) {
                int cl = 312 - (a->shoe_dealt > 312 ? 312 : a->shoe_dealt);
                if(cl < 1) cl = 1;
                int tc = (int)a->rc * 52 / cl;
                canvas_draw_str(canvas, 102, 41, "TC");
                snprintf(buf, sizeof(buf), "%+d", tc);
                canvas_draw_str(canvas, 102, 50, buf);
            }
        }

        Hand* h = &a->hands[a->cur];
        int t;
        bool s;
        const int labelw = 9;

        // measure dealer row
        bool dhidden = (a->state == StatePlayer || a->state == StateInsurance);
        int dnc = dhidden ? 2 : (a->dn > 6 ? 6 : a->dn);
        char dtot[12];
        dtot[0] = 0;
        int dtotw = 0;
        if(!dhidden) {
            hand_total(a->dealer, a->dn, &t, &s);
            snprintf(dtot, sizeof(dtot), "%d", t);
            dtotw = 3 + canvas_string_width(canvas, dtot);
        }
        int dgroup = labelw + dnc * 15 - 2 + dtotw;

        // measure player row
        int pnc = (h->n > 6) ? 6 : h->n;
        hand_total(h->cards, h->n, &t, &s);
        char ptot[12];
        snprintf(ptot, sizeof(ptot), "%d", t);
        int ptotw = 3 + canvas_string_width(canvas, ptot);
        int pgroup = labelw + pnc * 15 - 2 + ptotw;

        // centre the rows in the gap left by the discard tray, the count and the shoe
        int groupw = dgroup > pgroup ? dgroup : pgroup;
        int leftB = 6;
        int rightB = (a->count_mode > 0) ? 100 : 121;
        int lx = leftB + (rightB - leftB - groupw) / 2;
        if(lx < leftB) lx = leftB;

        // dealer row
        canvas_draw_str(canvas, lx, 22, "D");
        int x = lx + labelw;
        if(dhidden) {
            draw_card_box(canvas, x, 11, a->dealer[0], a->dsuit[0]);
            x += 15;
            draw_hidden_box(canvas, x, 11);
        } else {
            for(uint8_t i = 0; i < a->dn && i < 6; i++) {
                draw_card_box(canvas, x, 11, a->dealer[i], a->dsuit[i]);
                x += 15;
            }
            canvas_draw_str(canvas, x - 1, 22, dtot);
        }

        // player row
        char lbl[12];
        if(a->num_hands > 1)
            snprintf(lbl, sizeof(lbl), "%d", a->cur + 1);
        else {
            lbl[0] = 'Y';
            lbl[1] = 0;
        }
        canvas_draw_str(canvas, lx, 40, lbl);
        x = lx + labelw;
        for(uint8_t i = 0; i < h->n && i < 6; i++) {
            draw_card_box(canvas, x, 29, h->cards[i], h->suit[i]);
            x += 15;
        }
        canvas_draw_str(canvas, x - 1, 40, ptot);

        if(a->state == StateResult) {
            canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignBottom, a->result);
            char sess[48];
            if(a->total > 0) {
                unsigned long pct = (unsigned long)((a->correct * 100 + a->total / 2) / a->total);
                snprintf(
                    sess,
                    sizeof(sess),
                    "%lu%% (%lu/%lu)  OK:next",
                    pct,
                    (unsigned long)a->correct,
                    (unsigned long)a->total);
            } else {
                snprintf(sess, sizeof(sess), "Press OK to continue");
            }
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, sess);
        } else if(a->state == StateInsurance) {
            canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, "Insurance?");
            const char* ilab[2] = {"No", "Yes"};
            const CanvasDirection idir[2] = {
                CanvasDirectionRightToLeft, CanvasDirectionLeftToRight};
            int it = 0;
            const int ig = 12;
            for(int i = 0; i < 2; i++)
                it += 7 + canvas_string_width(canvas, ilab[i]);
            it += ig;
            int ix = (128 - it) / 2;
            if(ix < 6) ix = 6;
            for(int i = 0; i < 2; i++) {
                draw_action(canvas, ix, 59, idir[i], ilab[i]);
                ix += 7 + canvas_string_width(canvas, ilab[i]) + ig;
            }
        } else {
            // feedback line for the last play
            if(a->fb[0]) canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, a->fb);
            bool pair = (h->n == 2 && cval(h->cards[0]) == cval(h->cards[1]));
            bool cd = (h->n == 2 && !h->split_ace);
            bool cs = (h->n == 2 && pair && a->num_hands < MAX_HANDS && !h->split_ace);
            CanvasDirection adir[4];
            const char* alab[4];
            int na = 0;
            adir[na] = CanvasDirectionRightToLeft;
            alab[na] = "Hit";
            na++;
            adir[na] = CanvasDirectionLeftToRight;
            alab[na] = "Stay";
            na++;
            if(cd) {
                adir[na] = CanvasDirectionBottomToTop;
                alab[na] = "x2";
                na++;
            }
            if(cs) {
                adir[na] = CanvasDirectionTopToBottom;
                alab[na] = "Spl";
                na++;
            }
            int atotal = 0;
            const int agap = 6;
            for(int i = 0; i < na; i++)
                atotal += 7 + canvas_string_width(canvas, alab[i]);
            atotal += agap * (na - 1);
            int ax = (128 - atotal) / 2;
            if(ax < 6) ax = 6;
            for(int i = 0; i < na; i++) {
                draw_action(canvas, ax, 59, adir[i], alab[i]);
                ax += 7 + canvas_string_width(canvas, alab[i]) + agap;
            }
        }
    }
    furi_mutex_release(a->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, FuriWaitForever);
}

// ── Persistence ──────────────────────────────────────────────────────────
// The whole game state (hand in progress, shoe, count, stats, settings) is
// written to /data/save.bin after every action, so leaving the app or powering
// the Flipper off resumes exactly where you left. Guarded by a magic+version.
#define SAVE_MAGIC 0x424A3231u
#define SAVE_VER   9u // bumped: Advantage categories + rare rules + deviations/skill

static void save_state(App* a) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, APP_DATA_PATH("save.bin"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = SAVE_MAGIC, ver = SAVE_VER;
        storage_file_write(f, &magic, sizeof(magic));
        storage_file_write(f, &ver, sizeof(ver));
        storage_file_write(f, a, sizeof(App));
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void load_state(App* a) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, APP_DATA_PATH("save.bin"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0, ver = 0;
        storage_file_read(f, &magic, sizeof(magic));
        storage_file_read(f, &ver, sizeof(ver));
        if(magic == SAVE_MAGIC && ver == SAVE_VER) {
            FuriMutex* m = a->mutex;
            storage_file_read(f, a, sizeof(App));
            a->mutex = m; // keep the live mutex, not the saved pointer
            a->exit = false;
            a->notify = 0;
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

int32_t optimal_blackjack_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->state = StateMenu;
    app->vibro = true;
    app->sound = true;
    app->led = true;
    app->h17 = false;
    app->count_mode = 2; // RC + TC on by default
    // Advantage defaults: 6D S17 DAS 3:2, 75% pen, $2000 bank, $10 unit, 1-12,
    // 100 hands/hr, full deviations, perfect play (matches the verified baseline).
    app->aa_decks_i = 3;
    app->aa_das = true;
    app->aa_pen_i = 5;
    app->aa_bank_i = 2;
    app->aa_unit_i = 1;
    app->aa_spread_i = 4;
    app->aa_hph_i = 1; // 100 hands/hour
    app->aa_dev_set_i = 3; // complete index set
    app->aa_skill_i = 0; // 100% play accuracy
    load_state(app); // resume exactly where we left off, if a save exists

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);

    view_port_update(vp);

    InputEvent event;
    while(!app->exit) {
        FuriStatus st = furi_message_queue_get(queue, &event, 50);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool input = (st == FuriStatusOk && event.type == InputTypePress);
        bool redraw = false;
        if(input) {
            handle_input(app, event.key);
            redraw = true;
        }
        // drive the Count Trainer flash animation off the frame timer
        if(app->state == StateCount && app->ct_phase == 0) {
            if(ct_flip(app)) redraw = true;
        }
        uint8_t n = app->notify;
        app->notify = 0;
        furi_mutex_release(app->mutex);
        if(n) play_feedback(app, notif, n == 1);
        if(redraw) view_port_update(vp);
        if(input) save_state(app); // persist on real input, not every anim frame
    }

    save_state(app);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
