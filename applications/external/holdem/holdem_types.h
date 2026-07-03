#ifndef HOLDEM_TYPES_H
#define HOLDEM_TYPES_H

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <furi_hal.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define HOLDEM_MAX_PLAYERS       5
#define HOLDEM_MIN_PLAYERS       2
#define HOLDEM_DEFAULT_PLAYERS   HOLDEM_MAX_PLAYERS
#define HOLDEM_LEGACY_V1_PLAYERS 3
#define HOLDEM_LEGACY_V3_PLAYERS 4
#define HOLDEM_BOARD_MAX         5
#define HOLDEM_DECK_SIZE         52
#define HOLDEM_MAX_WINNERS       HOLDEM_MAX_PLAYERS

#define HOLDEM_SAVE_PATH                        "/ext/apps_data/holdem/save.bin"
#define HOLDEM_SAVE_DIR                         "/ext/apps_data/holdem"
#define HOLDEM_SAVE_MAGIC                       0x48444D31u
#define HOLDEM_BACK_HOLD_MS                     1500u
#define HOLDEM_RIGHT_HOLD_MS                    1000u
#define HOLDEM_AI_EASY_LEVEL                    20u
#define HOLDEM_AI_MEDIUM_LEVEL                  50u
#define HOLDEM_AI_HARD_LEVEL                    80u
#define HOLDEM_AI_EXTREME_LEVEL                 110u
#define HOLDEM_AI_DEFAULT_LEVEL                 HOLDEM_AI_MEDIUM_LEVEL
#define HOLDEM_BOT_DELAY_MS                     1450u
#define HOLDEM_BLIND_STEP_SB                    5
#define HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS 5u
#define HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB      40
#define HOLDEM_PROGRESSIVE_MIN_PERIOD_HANDS     5u
#define HOLDEM_PROGRESSIVE_MAX_PERIOD_HANDS     99u
#define HOLDEM_PROGRESSIVE_PERIOD_STEP_HANDS    5u
#define HOLDEM_PROGRESSIVE_MIN_STEP_SB          5
#define HOLDEM_PROGRESSIVE_MAX_STEP_SB          500
#define HOLDEM_ENDTURN_PAUSE_MS                 500u

typedef enum {
    StagePreflop = 0,
    StageFlop,
    StageTurn,
    StageRiver,
    StageShowdown,
} HoldemStage;

typedef enum {
    ActFold = 0,
    ActCheck,
    ActCall,
    ActRaise,
} HoldemAction;

typedef struct {
    uint8_t rank;
    uint8_t suit;
} Card;

typedef struct {
    char name[8];
    int32_t stack;
    Card hole[2];
    bool in_hand;
    bool is_bot;
    bool all_in;
    int32_t street_bet;
    int32_t hand_contrib;
} Player;

typedef struct {
    int idx[HOLDEM_MAX_WINNERS];
    size_t count;
    int32_t payout[HOLDEM_MAX_PLAYERS];
} PayoutResult;

typedef struct {
    Player players[HOLDEM_MAX_PLAYERS];
    size_t player_count;

    Card deck[HOLDEM_DECK_SIZE];
    size_t deck_pos;

    Card board[HOLDEM_BOARD_MAX];
    size_t board_count;

    int32_t pot;
    int32_t small_blind;
    int32_t big_blind;

    int button;
    int sb_idx;
    int bb_idx;
    int hand_no;
    HoldemStage stage;
    bool hand_in_progress;
} HoldemGame;

typedef enum {
    UiModeTable = 0,
    UiModeActionPrompt,
    UiModePause,
    UiModeBigWin,
    UiModeHelp,
    UiModeBlindEdit,
    UiModeBotCountEdit,
    UiModeNewGameConfirm,
    UiModeExitPrompt,
    UiModeStartChoice,
    UiModeStartReady,
    UiModeSplash,
} UiMode;

typedef struct {
    uint32_t magic;
    uint16_t version;
    HoldemGame game;
    uint8_t ai_level_pct;
    bool progressive_blinds_enabled;
    uint8_t progressive_period_hands;
    int32_t progressive_step_sb;
    int32_t progressive_next_raise_hand_no;
    int32_t configured_small_blind;
} HoldemSave;

typedef struct {
    uint32_t magic;
    uint16_t version;
    int32_t stack[HOLDEM_LEGACY_V1_PLAYERS];
    int32_t hand_no;
    int32_t button;
    int32_t sb;
    int32_t bb;
} HoldemSaveV1;

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    bool running;
    HoldemGame game;
    UiMode mode;
    UiMode return_mode;
    UiMode exit_return_mode;
    bool reveal_all;
    int acting_idx;
    int32_t prompt_to_call;
    int32_t prompt_min_raise;
    int32_t prompt_raise_by;
    int32_t prompt_bet_total;
    HoldemAction chosen_action;
    int32_t chosen_raise_by;
    bool action_ready;
    char pause_title[20];
    char pause_body[40];
    char pause_body2[40];
    char pause_body3[40];
    char pause_body4[24];
    char pause_footer[24];
    char pause_cards_label[8];
    Card pause_cards[5];
    size_t pause_card_count;
    char interstitial_text[40];
    bool interstitial_fireworks;
    bool interstitial_show_continue_hint;
    bool exit_requested;
    bool save_on_exit;
    bool back_down;
    bool back_long_handled;
    uint32_t back_press_tick;
    uint8_t ai_level_pct;
    bool bot_delay_enabled;
    uint16_t bot_delay_ms;
    bool bot_turn_active;
    int bot_turn_idx;
    char bot_turn_text[40];
    bool showdown_waiting;
    bool skip_autoplay_requested;
    bool reset_requested;
    uint8_t showdown_winner_mask;
    int32_t blind_edit_sb;
    int32_t blind_edit_initial_sb;
    int32_t configured_small_blind;
    size_t configured_player_count;
    size_t bot_count_edit_value;
    uint8_t configured_ai_level_pct;
    uint8_t bot_difficulty_edit_value;
    bool new_game_confirm_from_bot_edit;
    bool progressive_blinds_enabled;
    uint8_t progressive_period_hands;
    int32_t progressive_step_sb;
    int32_t progressive_next_raise_hand_no;
    int32_t pending_small_blind;
    bool pending_blinds_dirty;
    bool blind_edit_progressive_enabled;
    uint8_t blind_edit_progressive_period_hands;
    int32_t blind_edit_progressive_step_sb;
    uint8_t help_page;
    bool start_requested;
} HoldemApp;

typedef struct {
    int v[6];
} Score;

#endif
