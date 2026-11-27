/*
 * Game logic for Guess The Number.
 *
 * Everything here is plain C on top of GtnState: menus, both game modes,
 * scoring and stats. No Flipper calls on purpose, which keeps it easy to
 * follow next to the original Python version (original/guess_the_number.py).
 *
 * Time always comes in as `now` (ms), so pauses like the 0.85 s
 * "Starting game..." are just timestamps checked in gtn_tick().
 */

#include "gtn.h"

#include <stdio.h>
#include <string.h>

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

/* Python-style floor division by 2 (works for negative ranges too) */
static int32_t floor_half(int32_t v) {
    return (v >= 0) ? (v / 2) : -((-v + 1) / 2);
}

/* int.bit_length() */
static uint32_t bit_length(uint32_t v) {
    uint32_t n = 0;
    while(v) {
        n++;
        v >>= 1;
    }
    return n;
}

/* Switching screens also resets the clock most animations count from. */
static void go(GtnState* s, GtnScreen screen, uint32_t now) {
    s->screen = screen;
    s->screen_since = now;
}

static void prepare_text(GtnState* s, const char* title, const char* button, GtnNext next) {
    snprintf(s->title, sizeof(s->title), "%s", title);
    snprintf(s->button, sizeof(s->button), "%s", button);
    s->text_next = next;
    s->title_shake = false;
    s->scroll = 0;
    s->text_lines = 0;
}

static void celebrate(GtnState* s, const char* cheer, int32_t value, uint32_t now) {
    snprintf(s->cheer, sizeof(s->cheer), "%s", cheer);
    s->cheer_value = value;
    s->fx = GtnFxSuccess;
    go(s, GtnScreenCelebrate, now);
}

void gtn_stats_reset(GtnStats* st) {
    memset(st, 0, sizeof(*st));
    st->magic = GTN_STATS_MAGIC;
    st->version = GTN_STATS_VERSION;
}

/* inserts a score, returns its rank (1..GTN_HS_COUNT) or 0 */
static int
    hs_insert(GtnStats* st, uint16_t score, int32_t floor, int32_t ceiling, uint32_t tries) {
    for(int i = 0; i < GTN_HS_COUNT; i++) {
        if(score > st->hs[i].score) {
            for(int j = GTN_HS_COUNT - 1; j > i; j--)
                st->hs[j] = st->hs[j - 1];
            st->hs[i].score = score;
            st->hs[i].floor = (int16_t)floor;
            st->hs[i].ceiling = (int16_t)ceiling;
            st->hs[i].tries = (uint16_t)tries;
            return i + 1;
        }
    }
    return 0;
}

void gtn_init(GtnState* s, uint32_t now) {
    memset(s, 0, sizeof(*s));
    gtn_stats_reset(&s->stats);
    s->floor = GTN_DEFAULT_FLOOR;
    s->ceiling = GTN_DEFAULT_CEILING;
    go(s, GtnScreenSplash, now);
}

static void to_menu(GtnState* s, uint32_t now) {
    s->menu_prev = s->menu_index;
    go(s, GtnScreenMenu, now);
}

/* ---------- player guesses ---------- */

/* Same as the Python version: pick the secret, then wait a moment before
 * the game "starts". The guess starts in the middle of the range. */
static void player_start(GtnState* s, uint32_t now) {
    s->tries = 0;
    s->hint_dir = 0;
    s->wrong_at = 0;
    uint32_t span = (uint32_t)(s->ceiling - s->floor) + 1;
    s->secret = s->floor + (int32_t)(gtn_random() % span);
    s->guess = s->floor + floor_half(s->ceiling - s->floor);
    s->started = false;
    s->stats.p_played++;
    s->stats_dirty = true;
    if(s->floor == GTN_DEFAULT_FLOOR && s->ceiling == GTN_DEFAULT_CEILING) {
        snprintf(s->msg, sizeof(s->msg), "Playing with default range.");
    } else {
        snprintf(
            s->msg,
            sizeof(s->msg),
            "Playing with floor %ld and ceiling %ld.",
            (long)s->floor,
            (long)s->ceiling);
    }
    go(s, GtnScreenStarting, now);
}

static void player_submit(GtnState* s, uint32_t now) {
    int32_t g = s->guess;
    s->tries++;
    if(g == s->secret) {
        GtnStats* st = &s->stats;
        st->p_won++;
        st->p_guesses += s->tries;
        if(st->p_best_tries == 0 || s->tries < st->p_best_tries) st->p_best_tries = s->tries;
        if(s->tries == 1) st->p_psychic++;
        s->stats_dirty = true;

        /* score: 100 = as good as a perfect binary search on this range */
        uint32_t span = (uint32_t)(s->ceiling - s->floor) + 1;
        uint32_t score = bit_length(span) * 100 / s->tries;
        if(score > 65535) score = 65535;
        int rank = 0;
        if(span >= GTN_HS_MIN_SPAN) {
            rank = hs_insert(st, (uint16_t)score, s->floor, s->ceiling, s->tries);
        }
        char score_line[64];
        if(span < GTN_HS_MIN_SPAN) {
            snprintf(score_line, sizeof(score_line), "Range too small for the Hall of Fame.");
        } else if(rank == 1) {
            snprintf(
                score_line,
                sizeof(score_line),
                "Score: %lu pts. NEW HIGHSCORE!",
                (unsigned long)score);
        } else if(rank > 1) {
            snprintf(
                score_line,
                sizeof(score_line),
                "Score: %lu pts. #%d in the Hall of Fame!",
                (unsigned long)score,
                rank);
        } else {
            snprintf(score_line, sizeof(score_line), "Score: %lu pts.", (unsigned long)score);
        }

        if(s->tries != 1) {
            snprintf(
                s->body,
                sizeof(s->body),
                "You found out the secret number, it was indeed %ld!\nYou used %lu tries.\n%s",
                (long)s->secret,
                (unsigned long)s->tries,
                score_line);
        } else {
            snprintf(
                s->body,
                sizeof(s->body),
                "You found out the secret number, it was indeed %ld!\nYou used 1 try. Are you a psychic?\n%s",
                (long)s->secret,
                score_line);
        }
        prepare_text(s, "You got it!", "Menu", GtnNextMenu);
        celebrate(s, rank == 1 ? "NEW HIGHSCORE!" : "YOU GOT IT!", s->secret, now);
        return;
    }
    s->wrong_at = now;
    s->fx = GtnFxBlink;
    if(g < s->secret) {
        snprintf(s->msg, sizeof(s->msg), "%ld is too small. Try a bigger one.", (long)g);
        s->hint_dir = 1;
    } else {
        snprintf(s->msg, sizeof(s->msg), "%ld is too big. Try a smaller one.", (long)g);
        s->hint_dir = -1;
    }
}

/* ---------- computer guesses ---------- */

/* Max tries is the bit length of the range size, exactly like
 * (ceiling - floor + 1).bit_length() in Python. */
static void computer_intro(GtnState* s, uint32_t now) {
    s->tries = 0;
    s->lower = s->floor;
    s->upper = s->ceiling;
    s->max_tries = bit_length((uint32_t)(s->ceiling - s->floor) + 1);
    snprintf(
        s->body,
        sizeof(s->body),
        "Alright, let's get started. Think of any number between %ld and %ld"
        " and I will need a maximum of %lu %s\n"
        "Whenever you are ready, press OK and I'll start guessing.",
        (long)s->floor,
        (long)s->ceiling,
        (unsigned long)s->max_tries,
        s->max_tries != 1 ? "tries." : "try.");
    prepare_text(s, "Think of a number", "Ready", GtnNextComputerGuess);
    go(s, GtnScreenText, now);
}

/* starts the "thinking" pause; the outcome is resolved in gtn_tick */
static void computer_next(GtnState* s, uint32_t now) {
    s->thinking = true;
    s->think_ms = GTN_THINK_MIN_MS + gtn_random() % GTN_THINK_JITTER_MS;
    go(s, GtnScreenComputer, now);
}

/* Called once the thinking pause is over. If lower passed upper, one of
 * the answers was a lie (or a typo), and we call the player out. */
static void computer_resolve(GtnState* s, uint32_t now) {
    s->thinking = false;
    if(s->lower > s->upper) {
        snprintf(
            s->body,
            sizeof(s->body),
            "At least one of your answers must have been wrong. You either tried to"
            " trick me (ha ha) or you mistyped.\n"
            "Either way, let's wrap this up and start again. This time, don't try to"
            " trick me or watch your fingers ;)");
        s->stats.c_cheats++;
        s->stats_dirty = true;
        prepare_text(s, "Hey!", "Menu", GtnNextMenu);
        s->title_shake = true;
        s->fx = GtnFxError;
        go(s, GtnScreenText, now);
        return;
    }
    /* intentionally no lower == upper shortcut, for a funnier gameplay */
    s->guess = s->lower + floor_half(s->upper - s->lower);
    s->tries++;
    s->fx = GtnFxBlink;
}

static void computer_answer(GtnState* s, int answer, uint32_t now) {
    int32_t g = s->guess;
    if(answer > 0) {
        snprintf(
            s->msg, sizeof(s->msg), "Alright, it's not %ld. I'll try a higher number.", (long)g);
        s->lower = g + 1;
        computer_next(s, now);
    } else if(answer < 0) {
        snprintf(
            s->msg, sizeof(s->msg), "Alright, it's not %ld. I'll try a lower number.", (long)g);
        s->upper = g - 1;
        computer_next(s, now);
    } else {
        s->stats.c_found++;
        s->stats.c_tries += s->tries;
        if(s->tries == 1) s->stats.c_psychic++;
        s->stats_dirty = true;
        snprintf(
            s->body,
            sizeof(s->body),
            "Hah, found it ^^. It's %ld. Took me %lu %s\n"
            "Well, I am known for being a supercomputer, so this does not surprise"
            " the both of us. Let's play again.",
            (long)g,
            (unsigned long)s->tries,
            s->tries != 1 ? "tries." : "try. I am a psychic computer, haha!");
        prepare_text(s, "Found it ^^", "Menu", GtnNextMenu);
        celebrate(s, "FOUND IT ^^", g, now);
    }
}

/* ---------- input ---------- */

/* Holding a button speeds things up: after about 10 repeats the step
 * becomes 10x bigger, so getting to 9999 doesn't take forever. */
static int32_t step_for(GtnState* s, InputType type, int32_t base) {
    if(type == InputTypeRepeat) {
        if(s->repeat_count < 0xFFFF) s->repeat_count++;
        return (s->repeat_count > 10) ? base * 10 : base;
    }
    return base;
}

static bool is_step(InputType t) {
    return t == InputTypeShort || t == InputTypeRepeat;
}

static void menu_move(GtnState* s, int dir, uint32_t now) {
    s->menu_prev = s->menu_index;
    s->menu_index = (uint8_t)((s->menu_index + GTN_MENU_ITEMS + dir) % GTN_MENU_ITEMS);
    s->menu_changed_at = now;
}

static void input_menu(GtnState* s, InputKey key, InputType type, uint32_t now) {
    if(!is_step(type)) return;
    if(key == InputKeyUp) {
        menu_move(s, -1, now);
    } else if(key == InputKeyDown) {
        menu_move(s, 1, now);
    } else if(key == InputKeyOk && type == InputTypeShort) {
        switch(s->menu_index) {
        case 0:
        case 1:
            s->mode = s->menu_index == 0 ? GtnModePlayer : GtnModeComputer;
            s->range_row = 0;
            s->range_error = false;
            go(s, GtnScreenRange, now);
            break;
        case 2:
            s->page = 0;
            s->page_dir = 0;
            s->page_since = now;
            s->hold_ok = false;
            go(s, GtnScreenStats, now);
            break;
        case 3:
            go(s, GtnScreenAbout, now);
            break;
        default:
            go(s, GtnScreenBye, now);
            break;
        }
    } else if(key == InputKeyBack && type == InputTypeShort) {
        go(s, GtnScreenBye, now);
    }
}

static void input_range(GtnState* s, InputKey key, InputType type, uint32_t now) {
    if(!is_step(type)) return;
    int32_t* field = s->range_row == 0 ? &s->floor : &s->ceiling;
    switch(key) {
    case InputKeyUp:
    case InputKeyDown:
        if(type == InputTypeShort) s->range_row ^= 1;
        break;
    case InputKeyLeft:
    case InputKeyRight: {
        int dir = key == InputKeyRight ? 1 : -1;
        *field = clamp_i32(*field + dir * step_for(s, type, 1), GTN_LIMIT_MIN, GTN_LIMIT_MAX);
        s->range_error = false;
        s->nudge = (int8_t)dir;
        s->nudge_at = now;
        break;
    }
    case InputKeyOk:
        if(type != InputTypeShort) break;
        if(s->floor > s->ceiling) {
            s->range_error = true;
            s->nudge = 0;
            s->nudge_at = now;
            s->fx = GtnFxError;
        } else if(s->mode == GtnModePlayer) {
            player_start(s, now);
        } else {
            computer_intro(s, now);
        }
        break;
    case InputKeyBack:
        if(type == InputTypeShort) to_menu(s, now);
        break;
    default:
        break;
    }
}

static void input_starting(GtnState* s, InputKey key, InputType type, uint32_t now) {
    if(type != InputTypeShort) return;
    if(key == InputKeyBack) {
        to_menu(s, now);
    } else if(key == InputKeyOk && s->started) {
        snprintf(s->msg, sizeof(s->msg), "Pick a number and press OK.");
        go(s, GtnScreenPlayer, now);
    }
}

static void input_player(GtnState* s, InputKey key, InputType type, uint32_t now) {
    if(!is_step(type)) return;
    switch(key) {
    case InputKeyUp:
        s->guess = clamp_i32(s->guess + step_for(s, type, 1), s->floor, s->ceiling);
        break;
    case InputKeyDown:
        s->guess = clamp_i32(s->guess - step_for(s, type, 1), s->floor, s->ceiling);
        break;
    case InputKeyRight:
        s->guess = clamp_i32(s->guess + step_for(s, type, 10), s->floor, s->ceiling);
        break;
    case InputKeyLeft:
        s->guess = clamp_i32(s->guess - step_for(s, type, 10), s->floor, s->ceiling);
        break;
    case InputKeyOk:
        if(type == InputTypeShort) player_submit(s, now);
        break;
    case InputKeyBack:
        if(type == InputTypeShort) to_menu(s, now);
        break;
    default:
        break;
    }
}

static void input_computer(GtnState* s, InputKey key, InputType type, uint32_t now) {
    if(type != InputTypeShort) return;
    if(key == InputKeyBack) {
        s->thinking = false;
        to_menu(s, now);
        return;
    }
    if(s->thinking) return; /* let the supercomputer think */
    switch(key) {
    case InputKeyRight:
    case InputKeyUp:
        computer_answer(s, 1, now);
        break;
    case InputKeyLeft:
    case InputKeyDown:
        computer_answer(s, -1, now);
        break;
    case InputKeyOk:
        computer_answer(s, 0, now);
        break;
    default:
        break;
    }
}

static void input_text(GtnState* s, InputKey key, InputType type, uint32_t now) {
    if(!is_step(type)) return;
    size_t visible = 4;
    size_t max_scroll = s->text_lines > visible ? s->text_lines - visible : 0;
    switch(key) {
    case InputKeyUp:
        if(s->scroll > 0) s->scroll--;
        break;
    case InputKeyDown:
        if(s->scroll < max_scroll) s->scroll++;
        break;
    case InputKeyOk:
        if(type != InputTypeShort) break;
        if(s->text_next == GtnNextComputerGuess) {
            s->msg[0] = '\0';
            s->stats.c_played++;
            s->stats_dirty = true;
            computer_next(s, now);
        } else {
            to_menu(s, now);
        }
        break;
    case InputKeyBack:
        if(type == InputTypeShort) to_menu(s, now);
        break;
    default:
        break;
    }
}

void gtn_input(GtnState* s, InputKey key, InputType type, uint32_t now) {
    if(s->screen == GtnScreenStats && key == InputKeyOk) {
        if(type == InputTypePress) {
            s->hold_ok = true;
            s->hold_since = now;
        } else if(type == InputTypeRelease) {
            s->hold_ok = false;
        }
    }
    if(type == InputTypePress) {
        s->repeat_count = 0;
        return;
    }
    if(key == InputKeyBack && type == InputTypeLong) {
        go(s, GtnScreenBye, now);
        return;
    }
    switch(s->screen) {
    case GtnScreenSplash:
        if(type == InputTypeShort) to_menu(s, now);
        break;
    case GtnScreenMenu:
        input_menu(s, key, type, now);
        break;
    case GtnScreenRange:
        input_range(s, key, type, now);
        break;
    case GtnScreenStarting:
        input_starting(s, key, type, now);
        break;
    case GtnScreenPlayer:
        input_player(s, key, type, now);
        break;
    case GtnScreenComputer:
        input_computer(s, key, type, now);
        break;
    case GtnScreenText:
        input_text(s, key, type, now);
        break;
    case GtnScreenStats:
        if(type != InputTypeShort) break;
        if(key == InputKeyLeft || key == InputKeyRight) {
            int dir = key == InputKeyRight ? 1 : -1;
            s->page = (uint8_t)((s->page + GTN_STATS_PAGES + dir) % GTN_STATS_PAGES);
            s->page_dir = (int8_t)dir;
            s->page_since = now;
        } else if(key == InputKeyBack) {
            to_menu(s, now);
        }
        break;
    case GtnScreenAbout:
        if(type == InputTypeShort && (key == InputKeyOk || key == InputKeyBack)) to_menu(s, now);
        break;
    case GtnScreenCelebrate: /* the 2 seconds are part of the game */
    case GtnScreenBye:
    default:
        break;
    }
}

/* Runs every loop, button or not. Handles everything time-based: play time,
 * the hold-to-reset on the stats screen and the timed screens. */
void gtn_tick(GtnState* s, uint32_t now) {
    /* time played */
    if(s->last_tick) {
        uint32_t d = now - s->last_tick;
        if(d < 5000) s->ms_accum += d; /* ignore weird jumps */
        while(s->ms_accum >= 1000) {
            s->ms_accum -= 1000;
            s->stats.play_seconds++;
        }
    }
    s->last_tick = now;

    if(s->screen == GtnScreenStats && s->hold_ok && now - s->hold_since >= GTN_RESET_HOLD_MS) {
        s->hold_ok = false;
        gtn_stats_reset(&s->stats);
        s->stats_dirty = true;
        s->toast_at = now;
        s->page_since = now; /* replay the count-up from zero */
        s->page_dir = 0;
        s->fx = GtnFxError;
    }

    uint32_t elapsed = now - s->screen_since;
    switch(s->screen) {
    case GtnScreenSplash:
        if(elapsed >= GTN_SPLASH_MS) to_menu(s, now);
        break;
    case GtnScreenStarting:
        if(!s->started && elapsed >= GTN_STARTING_MS) {
            s->started = true;
            s->fx = GtnFxBlink;
        }
        break;
    case GtnScreenComputer:
        if(s->thinking && elapsed >= s->think_ms) computer_resolve(s, now);
        break;
    case GtnScreenCelebrate:
        if(elapsed >= GTN_CELEBRATE_MS) go(s, GtnScreenText, now);
        break;
    case GtnScreenBye:
        if(elapsed >= GTN_BYE_MS) s->exit = true;
        break;
    default:
        break;
    }
}
