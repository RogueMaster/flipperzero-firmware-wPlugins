#include <input/input.h>
#include <gui/icon.h>
#include <gui/canvas.h>

// I'm using the code for sound from Doom by MMX @ https://github.com/xMasterX/all-the-plugins/tree/dev/base_pack/doom
// Thanks for the permission, MMX!
#include "../helpers/sound.h"

#pragma once
// Likelyhood for special rounds to force a stratagem to match the theme, out of 10.
#define SPECIAL_ROUND_SELECTION_CHANCE 75
#define MENU_OPTION_COUNT              3
#define SOUND
typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    int32_t score;
    int32_t round;

    // bool states[2];
} Record;

typedef enum {
    SpecialRoundNone,
    SpecialRoundOrbital,
    SpecialRoundEagle,
    SpecialRoundBackpack,
    SpecialRoundWeapon,
} SpecialRound;

typedef struct {
    bool unfaithful; // For adherence to original Stratagem Hero. If true, removes warbond stratagems.
    bool enable_sound; // Whether or not to play jingles at the ends of games/rounds
    bool center_queue; // If true, draw the current stratagem in the middle. If false, draw it offset to the left.
} Config;

typedef struct {
    FuriMutex* mutex;

    // ARROW HANDLING
    char arrowDirections[8]; // Store directions of up to 8 arrows
    bool arrowFilled[8]; // Track whether each arrow is filled
    int numArrows; // Number of arrows currently active
    int nextArrowToFill; // Index of the next arrow to fill
    char seq_buf[9]; // Buffer for arrow sequences

    // QUEUE
    int queue_spot; // Current place in the stratgaem queue
    int stratagem_queue
        [20]; // 20 long even though there will only be a max of 16 stratagems. I honstly don't know why i did this lol
    SpecialRound special_round;

    char name_buf[32]; // Buffer for stratagem name

    // GAME STATS
    int score;
    int round;
    int timer;
    Record* record; // High score and high round

    // MENU HANDLING
    int menu_spot;
    Config* config;

    // MARQUEE
    bool init_marquee;
    bool do_marquee;
    int marquee_data[3];
    // 0: Time at stratagem start
    // 1: Current offset
    // 2: Marquee spacing

    // END OF ROUND BONUSES
    bool perfect;
    int bonuses[3]; // round bonus at 0, time bonus at 1, perfect bonus at 2.

    // GAME STATE
    bool allowMenuInput;
    bool allowOkInput;
    bool allowPauseInput;
    bool isGamePaused;
    bool isGameStarted;
    bool isScoreScreen;
    bool isPreRound;
    bool isRoundOver;
    bool isGameOver;

    //Music stuff
#ifdef SOUND
    MusicPlayer* music_instance;
    bool playing_sound;
    bool gameOverSoundPlayed;
    int mus_timer;
#endif
} PluginState;

typedef enum {
    ArrowDirectionUp,
    ArrowDirectionRight,
    ArrowDirectionLeft,
    ArrowDirectionDown
} ArrowDirection;
