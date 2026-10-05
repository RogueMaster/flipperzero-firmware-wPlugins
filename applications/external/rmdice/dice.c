#include <datetime/datetime.h>
#include <dolphin/dolphin.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <input/input.h>
#include "dice_rm_icons.h"

#if __has_include(<cfw/cfw.h>)
#include <cfw/cfw.h>
#endif

#define TAG               "Dice Roller"
#define DICE_MAX_QUANTITY 6

typedef enum {
    DiceModeD2,
    DiceModeD3,
    DiceModeD4,
    DiceModeD6,
    DiceModeD8,
    DiceModeD10,
    DiceModeD12,
    DiceModeD20,
    DiceModeD100,
    DiceModeSex,
    DiceModeWar,
    DiceModeEightBall,
    DiceModeDevilBall,
    DiceModeWeed,
    DiceModeDrink,
    DiceModeD59,
    DiceModeD69,
    DiceModeCount,
} DiceMode;

typedef enum {
    DiceInputIgnored,
    DiceInputChanged,
    DiceInputRoll,
    DiceInputExit,
} DiceInputResult;

typedef enum {
    DiceFlagInput = 1 << 0,
    DiceFlagRedraw = 1 << 1,
    DiceFlagExit = 1 << 2,
} DiceThreadFlag;

typedef struct {
    uint8_t sides;
    bool hidden_in_game_mode;
    const char* button_label;
    const char* result_label;
} DiceModeInfo;

static const DiceModeInfo dice_modes[DiceModeCount] = {
    [DiceModeD2] = {2, false, "d2", "d2"},
    [DiceModeD3] = {3, false, "d3", "d3"},
    [DiceModeD4] = {4, false, "d4", "d4"},
    [DiceModeD6] = {6, false, "d6", "d6"},
    [DiceModeD8] = {8, false, "d8", "d8"},
    [DiceModeD10] = {10, false, "d10", "d10"},
    [DiceModeD12] = {12, false, "d12", "d12"},
    [DiceModeD20] = {20, false, "d20", "d20"},
    [DiceModeD100] = {100, false, "d100", "d100"},
    [DiceModeSex] = {0, true, "SEX", "SEX?"},
    [DiceModeWar] = {0, false, "WAR", "WAR!"},
    [DiceModeEightBall] = {0, false, "8BALL", "8BALL"},
    [DiceModeDevilBall] = {0, false, "DBALL", "Devil Ball"},
    [DiceModeWeed] = {0, true, "WEED", "WEED!"},
    [DiceModeDrink] = {0, true, "DRINK", "DRINK!"},
    [DiceModeD59] = {59, false, "d59", "d59"},
    [DiceModeD69] = {69, false, "d69", "d69"},
};

typedef struct {
    DiceMode mode;
    uint8_t quantity;
    uint32_t player_one_score;
    uint32_t player_two_score;
    bool has_result;
    char strings[5][45];
} DiceModel;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    FuriThreadId thread_id;
    DiceModel model;
} DiceState;

static const char* const dice_eight_ball[] = {
    "It is certain",
    "Without a doubt",
    "You may rely on it",
    "Yes definitely",
    "It is decidedly so",
    "As I see it, yes",
    "Most likely",
    "Yes",
    "Outlook good",
    "Signs point to yes",
    "Reply hazy try again",
    "Better not tell you now",
    "Ask again later",
    "Cannot predict now",
    "Concentrate and ask again",
    "Don't count on it",
    "Outlook not so good",
    "My sources say no",
    "Very doubtful",
    "My reply is no"};

static const char* const dice_devil_ball[] = {
    "I'd do it.",
    "Hell, yeah!",
    "You bet your life!",
    "What are you waiting for?",
    "You could do worse things.",
    "Sure, I won't tell.",
    "Yeah, u got this. Would I lie?",
    "Looks like fun to me. ",
    "Yeah, sure, why not?",
    "DO IT!!!",
    "Who's it gonna hurt?",
    "Can you blame someone else?",
    "Ask me again later.",
    "I can't tell right now.",
    "Are you the betting type? ",
    "Don't blame me if you caught.",
    "What have you got to lose?",
    "I wouldn't if I were you.",
    "My money's on the snowball.",
    "Oh Hell no!"};

static const char* const dice_sex_actions[] =
    {"Nibble", "Massage", "Touch", "Caress", "Pet", "Fondle", "Suck", "Lick", "Blow", "Kiss", "???"};

static const char* const dice_sex_parts[] =
    {"Navel", "Ears", "Lips", "Neck", "Hand", "Thigh", "Nipple", "Breasts", "???", "Genitals"};

static const char* const dice_cards[] = {"2H", "2C", "2D", "2S", "3H", "3C",  "3D",  "3S",  "4H",
                                         "4C", "4D", "4S", "5H", "5C", "5D",  "5S",  "6H",  "6C",
                                         "6D", "6S", "7H", "7C", "7D", "7S",  "8H",  "8C",  "8D",
                                         "8S", "9H", "9C", "9D", "9S", "10H", "10C", "10D", "10S",
                                         "JH", "JC", "JD", "JS", "QH", "QC",  "QD",  "QS",  "KH",
                                         "KC", "KD", "KS", "AH", "AC", "AD",  "AS"};

static const char* const dice_weed_players[] =
    {"You", "You choose", "Nobody", "Everyone", "Nose goes", "Player to your right"};

static const char* const dice_weed_actions[] = {
    "take a tiny toke",
    "just chill",
    "take 2 tokes",
    "take a huge hit",
    "bogart it",
    "take a puff"};

static const char* const dice_weed_styles[] = {
    "while humming a tune",
    "with your eyes closed",
    "on your knees",
    "while holding your nose",
    "while spinning in a circle",
    "in slow motion"};

static const char* const dice_weed_followups[] = {
    "twice",
    "then tell a joke",
    "then laugh as hard as you can",
    "with the player to your left",
    "then sing a song",
    "then do a dance"};

static const char* const dice_drink_players[] = {
    "YOU",
    "PICK A MATE",
    "PLAYER TO YOUR LEFT",
    "ALL PLAYERS",
    "PLAYER OF YOUR CHOICE",
    "PLAYER TO YOUR RIGHT"};

static const char* const dice_drink_actions[] =
    {"LIL Sip", "MAKE A RULE", "DRINK AT WILL", "DON'T DRINK", "BIG SWIG", "BOTTOMS UP"};

/* Reject the incomplete residue block in the full 32-bit RNG range. */
static uint16_t dice_random_bounded(uint16_t bound) {
    furi_assert(bound);
    const uint32_t threshold = (0U - (uint32_t)bound) % bound;
    uint32_t sample;
    do {
        sample = furi_hal_random_get();
    } while(sample < threshold);
    return sample % bound;
}

static uint8_t dice_second_card_index(uint8_t first, uint8_t remaining_index) {
    return remaining_index + (remaining_index >= first);
}

static DiceMode dice_next_mode(DiceMode mode, bool game_mode) {
    do {
        mode = (DiceMode)((mode + 1) % DiceModeCount);
    } while(game_mode && dice_modes[mode].hidden_in_game_mode);
    return mode;
}

static bool dice_game_mode(void) {
#if __has_include(<cfw/cfw.h>)
    return cfw_settings.game_mode;
#else
    return false;
#endif
}

static void dice_award_xp(void) {
#if __has_include(<cfw/cfw.h>)
    dolphin_deed(getRandomDeed());
#else
    dolphin_deed(DolphinDeedBadUsbPlayScript);
#endif
}

static DiceInputResult dice_handle_input(DiceModel* model, const InputEvent* input) {
    if(input->key == InputKeyBack && input->type == InputTypeLong) return DiceInputExit;
    if(input->type != InputTypeShort && input->type != InputTypeRepeat) return DiceInputIgnored;
    switch(input->key) {
    case InputKeyRight:
        model->mode = dice_next_mode(model->mode, dice_game_mode());
        model->has_result = false;
        if(model->mode == DiceModeWar) {
            model->player_one_score = 0;
            model->player_two_score = 0;
        }
        return DiceInputChanged;
    case InputKeyLeft:
        model->quantity = model->quantity % DICE_MAX_QUANTITY + 1;
        return DiceInputChanged;
    case InputKeyOk:
        return DiceInputRoll;
    case InputKeyBack:
        return DiceInputExit;
    default:
        return DiceInputIgnored;
    }
}

/* The app thread generates each result once; GUI redraws never reroll or award XP. */
static bool dice_roll(DiceModel* model) {
    const DiceModeInfo* info = &dice_modes[model->mode];
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    const unsigned hour = datetime.hour % 12 ? datetime.hour % 12 : 12;
    char roll_time[15];
    snprintf(
        roll_time,
        sizeof(roll_time),
        "%02u:%02u:%02u %s",
        hour,
        datetime.minute,
        datetime.second,
        datetime.hour >= 12 ? "PM" : "AM");
    memset(model->strings, 0, sizeof(model->strings));
    if(info->sides) {
        snprintf(
            model->strings[0],
            sizeof(model->strings[0]),
            "%u%s at %s",
            model->quantity,
            info->result_label,
            roll_time);
    } else {
        snprintf(
            model->strings[0],
            sizeof(model->strings[0]),
            "%s at %s",
            info->result_label,
            roll_time);
    }
    bool award_xp = false;
    switch(model->mode) {
    case DiceModeEightBall:
        snprintf(
            model->strings[1],
            sizeof(model->strings[1]),
            "%s",
            dice_eight_ball[dice_random_bounded(COUNT_OF(dice_eight_ball))]);
        break;
    case DiceModeDevilBall:
        snprintf(
            model->strings[1],
            sizeof(model->strings[1]),
            "%s",
            dice_devil_ball[dice_random_bounded(COUNT_OF(dice_devil_ball))]);
        break;
    case DiceModeSex: {
        const char* action = dice_sex_actions[dice_random_bounded(COUNT_OF(dice_sex_actions))];
        const char* part = dice_sex_parts[dice_random_bounded(COUNT_OF(dice_sex_parts))];
        snprintf(model->strings[1], sizeof(model->strings[1]), "%s %s", action, part);
        break;
    }
    case DiceModeWar: {
        const uint8_t first = dice_random_bounded(COUNT_OF(dice_cards));
        const uint8_t second =
            dice_second_card_index(first, dice_random_bounded(COUNT_OF(dice_cards) - 1));
        uint32_t* score = first > second ? &model->player_one_score : &model->player_two_score;
        if(*score != UINT32_MAX) (*score)++;
        snprintf(
            model->strings[1],
            sizeof(model->strings[1]),
            "%s %s %s",
            dice_cards[first],
            first > second ? ">" : "<",
            dice_cards[second]);
        break;
    }
    case DiceModeWeed:
        snprintf(
            model->strings[1],
            sizeof(model->strings[1]),
            "%s",
            dice_weed_players[dice_random_bounded(COUNT_OF(dice_weed_players))]);
        snprintf(
            model->strings[2],
            sizeof(model->strings[2]),
            "%s",
            dice_weed_actions[dice_random_bounded(COUNT_OF(dice_weed_actions))]);
        snprintf(
            model->strings[3],
            sizeof(model->strings[3]),
            "%s",
            dice_weed_styles[dice_random_bounded(COUNT_OF(dice_weed_styles))]);
        snprintf(
            model->strings[4],
            sizeof(model->strings[4]),
            "%s",
            dice_weed_followups[dice_random_bounded(COUNT_OF(dice_weed_followups))]);
        break;
    case DiceModeDrink:
        snprintf(
            model->strings[2],
            sizeof(model->strings[2]),
            "%s",
            dice_drink_players[dice_random_bounded(COUNT_OF(dice_drink_players))]);
        snprintf(
            model->strings[3],
            sizeof(model->strings[3]),
            "%s",
            dice_drink_actions[dice_random_bounded(COUNT_OF(dice_drink_actions))]);
        break;
    default: {
        size_t used = 0;
        for(uint8_t i = 0; i < model->quantity; i++) {
            const unsigned roll = dice_random_bounded(info->sides) + 1;
            if(i == 0 && info->sides >= 20 && roll >= info->sides - 1U) award_xp = true;
            const size_t remaining = sizeof(model->strings[1]) - used;
            const int written =
                snprintf(model->strings[1] + used, remaining, i ? " %u" : "%u", roll);
            if(written < 0 || (size_t)written >= remaining) break;
            used += written;
        }
        break;
    }
    }
    model->has_result = true;
    return award_xp;
}

static void dice_input_callback(InputEvent* input, void* context) {
    DiceState* state = context;
    if(input->type != InputTypeShort && input->type != InputTypeRepeat &&
       !(input->key == InputKeyBack && input->type == InputTypeLong))
        return;
    if(input->key == InputKeyBack) {
        furi_thread_flags_set(state->thread_id, DiceFlagExit);
        return;
    }
    const uint32_t timeout = input->type == InputTypeRepeat ? 0 : 10;
    if(furi_message_queue_put(state->event_queue, input, timeout) == FuriStatusOk) {
        furi_thread_flags_set(state->thread_id, DiceFlagInput);
    }
}

static void dice_render_callback(Canvas* canvas, void* context) {
    DiceState* state = context;
    DiceModel model;
    if(furi_mutex_acquire(state->mutex, 0) != FuriStatusOk) {
        furi_thread_flags_set(state->thread_id, DiceFlagRedraw);
        return;
    }
    model = state->model;
    furi_mutex_release(state->mutex);

    const DiceModeInfo* info = &dice_modes[model.mode];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_icon(canvas, 0, 0, &I_black);
    canvas_set_color(canvas, ColorWhite);
    if(info->sides) {
        char quantity[5];
        snprintf(quantity, sizeof(quantity), "x%u", model.quantity);
        elements_button_left(canvas, quantity);
    }
    if(model.has_result) {
        if(model.mode == DiceModeWeed) {
            static const uint8_t rows[] = {8, 18, 26, 34, 42};
            for(size_t i = 0; i < COUNT_OF(rows); i++) {
                canvas_draw_str_aligned(
                    canvas, 64, rows[i], AlignCenter, AlignCenter, model.strings[i]);
            }
        } else if(model.mode == DiceModeDrink) {
            canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, model.strings[0]);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, model.strings[2]);
            canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, model.strings[3]);
        } else {
            if(model.mode == DiceModeEightBall || model.mode == DiceModeDevilBall) {
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
                canvas_set_font(canvas, FontBatteryPercent);
#else
                canvas_set_font(canvas, FontSecondary);
#endif
            } else {
                canvas_set_font(canvas, FontPrimary);
            }
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, model.strings[1]);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, model.strings[0]);
        }
        if(model.mode == DiceModeWar && (model.player_one_score || model.player_two_score)) {
            char score[11];
            snprintf(score, sizeof(score), "%lu", (unsigned long)model.player_one_score);
            canvas_draw_str_aligned(canvas, 8, 34, AlignLeft, AlignCenter, score);
            snprintf(score, sizeof(score), "%lu", (unsigned long)model.player_two_score);
            canvas_draw_str_aligned(canvas, 120, 34, AlignRight, AlignCenter, score);
        }
    }
    const char* action = model.mode == DiceModeWar ? "Draw" :
                         (model.mode == DiceModeEightBall || model.mode == DiceModeDevilBall) ?
                                                     "Shake" :
                                                     "Roll";
    elements_button_center(canvas, action);
    elements_button_right(canvas, info->button_label);
}

int32_t dice_app(void* context) {
    UNUSED(context);
    DiceState* state = malloc(sizeof(DiceState));
    if(!state) return 255;
    memset(state, 0, sizeof(*state));
    state->thread_id = furi_thread_get_current_id();
    state->event_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state->event_queue || !state->mutex) {
        if(state->event_queue) furi_message_queue_free(state->event_queue);
        if(state->mutex) furi_mutex_free(state->mutex);
        free(state);
        return 255;
    }
    DiceModel model = {.mode = DiceModeD20, .quantity = 1};
    state->model = model;
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, dice_render_callback, state);
    view_port_input_callback_set(view_port, dice_input_callback, state);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    for(bool processing = true; processing;) {
        const uint32_t flags = furi_thread_flags_wait(
            DiceFlagInput | DiceFlagRedraw | DiceFlagExit, FuriFlagWaitAny, FuriWaitForever);
        if(flags & FuriFlagError) break;
        if(flags & DiceFlagExit) break;
        bool changed = false;
        InputEvent input;
        if(furi_message_queue_get(state->event_queue, &input, 0) == FuriStatusOk) {
            const DiceInputResult result = dice_handle_input(&model, &input);
            if(result == DiceInputExit) {
                processing = false;
            }
            if(result == DiceInputRoll && dice_roll(&model)) dice_award_xp();
            if(result == DiceInputChanged || result == DiceInputRoll) changed = true;
        }
        if(furi_message_queue_get_count(state->event_queue)) {
            furi_thread_flags_set(state->thread_id, DiceFlagInput);
        }
        if(changed) {
            furi_check(furi_mutex_acquire(state->mutex, FuriWaitForever) == FuriStatusOk);
            state->model = model;
            furi_mutex_release(state->mutex);
        }
        if(processing && (changed || (flags & DiceFlagRedraw))) view_port_update(view_port);
    }
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(state->event_queue);
    furi_mutex_free(state->mutex);
    free(state);
    return 0;
}
