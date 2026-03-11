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

#define HOLDEM_MAX_PLAYERS 4
#define HOLDEM_MIN_PLAYERS 2
#define HOLDEM_DEFAULT_PLAYERS HOLDEM_MAX_PLAYERS
#define HOLDEM_LEGACY_V1_PLAYERS 3
#define HOLDEM_BOARD_MAX 5
#define HOLDEM_DECK_SIZE 52
#define HOLDEM_MAX_WINNERS HOLDEM_MAX_PLAYERS

#define HOLDEM_SAVE_PATH "/ext/apps_data/holdem/save.bin"
#define HOLDEM_SAVE_DIR "/ext/apps_data/holdem"
#define HOLDEM_SAVE_MAGIC 0x48444D31u
#define HOLDEM_BACK_HOLD_MS 1500u
#define HOLDEM_AI_DEFAULT_LEVEL 50u
#define HOLDEM_BOT_DELAY_MS 1600u
#define HOLDEM_BLIND_STEP_SB 5
#define HOLDEM_ENDTURN_PAUSE_MS 500u

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
    UiModeRestartConfirm,
    UiModeExitPrompt,
    UiModeStartChoice,
    UiModeStartReady,
    UiModeSplash,
} UiMode;

typedef struct {
    uint32_t magic;
    uint16_t version;
    HoldemGame game;
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
    char pause_footer[24];
    char pause_cards_label[8];
    Card pause_cards[5];
    size_t pause_card_count;
    char interstitial_text[20];
    bool interstitial_fireworks;
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
    bool reset_requested;
    uint8_t showdown_winner_mask;
    int32_t blind_edit_sb;
    size_t configured_player_count;
    size_t bot_count_edit_value;
    int32_t pending_small_blind;
    bool pending_blinds_dirty;
    uint8_t help_page;
} HoldemApp;

typedef struct {
    int v[6];
} Score;

#endif

