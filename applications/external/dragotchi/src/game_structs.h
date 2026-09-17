#ifndef __game_structs_h__
#define __game_structs_h__

#include <stdint.h>
#include <stdbool.h>

/* Messages from the GUI/main thread to the logic thread */
enum ThreadsMessageType {
    SAVE_AND_EXIT,
    RESET_STATE,
    PROCESS_FEED,
    PROCESS_PLAY,
    PROCESS_CLEAN,
    PROCESS_MEDICINE,
    PROCESS_SCOLD,
    TOGGLE_LIGHTS,
    PROCESS_FORAGE,
    PROCESS_HATCH_HEIR,
    PROCESS_EXPEDITION
};

struct ThreadsMessage {
    enum ThreadsMessageType type;
    uint32_t arg; // optional payload (e.g. expedition minutes)
};

enum LifeStage {
    EGG,
    HATCHLING,
    WYRMLING,
    DRAKE,
    ADULT,
    DEAD,
    LIFE_STAGES_NUM
};

/* Adult character, chosen by care quality at the Drake->Adult branch */
enum DragonAlignment {
    ALIGN_NONE, // not yet adult
    ALIGN_WHITE, // raised very well
    ALIGN_GREY,  // middling
    ALIGN_BLACK, // neglected
    DRAGON_ALIGNMENTS_NUM
};

/* Transient animation/display hint (not persisted) */
enum DisplayState {
    DISP_IDLE,
    DISP_EATING,
    DISP_PLAYING,
    DISP_SICK,
    DISP_SLEEPING,
    DISP_EVOLVING,
    DISP_DEAD
};

enum CatchCategory { CATCH_PREY, CATCH_TREASURE, CATCH_EGG };
enum CatchTier { TIER_SMALL, TIER_MED, TIER_LARGE };
struct Catch {
    uint8_t category; // enum CatchCategory
    uint8_t tier;     // enum CatchTier (prey/treasure) or rarity (egg: 0 common,1 rare)
    uint16_t value;   // food or hoard points
    uint8_t band;     // 0..HUNT_BANDS-1 (flavour)
};

/* Event flags returned by advance/action functions so the device layer can
 * decide sound/vibration and build the "while you were away" summary. */
typedef uint32_t GameEventFlags;
#define EVT_NONE 0u
#define EVT_EVOLVED (1u << 0)
#define EVT_STARVING (1u << 1) // hunger reached 0
#define EVT_SICK (1u << 2) // became sick
#define EVT_POOPED (1u << 3) // new poop appeared
#define EVT_DIED (1u << 4) // died (any cause)
#define EVT_CALL (1u << 5) // discipline/attention call raised
#define EVT_HEALED (1u << 6) // recovered from sickness (via care)
#define EVT_FED (1u << 7)
#define EVT_PLAYED (1u << 8)
#define EVT_CLEANED (1u << 9)
#define EVT_CAUGHT (1u << 10)
#define EVT_EXPED_RETURN (1u << 11)

/* Persisted game state (saved to storage) */
struct PersistentGameState {
    uint8_t stage; // enum LifeStage
    uint8_t alignment; // enum DragonAlignment
    uint32_t birth_timestamp; // for age
    uint32_t stage_entered_timestamp; // age within current stage

    // Needs
    uint32_t hunger;
    uint32_t last_hunger_update;
    uint32_t happiness;
    uint32_t last_happiness_update;
    uint32_t health;
    uint32_t last_health_update;

    // States
    uint8_t poop; // 0..MAX_POOP
    uint32_t last_poop_update;
    uint32_t poop_since; // when current (uncleaned) poop first appeared; 0 = clean
    uint8_t poop_penalized; // care already docked for this poop episode
    uint8_t sick; // 0/1
    uint32_t last_sick_update;
    uint32_t sick_since; // when became sick; 0 = healthy
    uint8_t sick_penalized; // care already docked for this illness episode
    uint8_t lights_off; // 1 = user allowed sleep (lights out)
    uint32_t last_sleep_update; // cursor for awake-at-night checks

    // Hidden drivers
    int32_t care_score; // 0..100
    uint32_t discipline; // 0..100
    uint8_t attention_call; // 1 = calling with no real need
    uint32_t last_attention_update; // cursor for attention-call checks
    uint32_t last_oldage_update; // cursor for old-age death checks (adult)
    // --- v0.2 Hunt ---
    uint32_t hoard;             // treasure points (score)
    uint16_t eggs_common;       // hatchery
    uint16_t eggs_rare;
    uint32_t last_forage_time;  // forage cooldown cursor
    // Inventory breakdown (lifetime / current)
    uint16_t treasure_small;
    uint16_t treasure_med;
    uint16_t treasure_large;
    uint16_t prey_caught;   // lifetime prey
    uint16_t eggs_caught;   // lifetime eggs (eggs_common/rare are current, decremented on hatch)
    // Expedition
    uint8_t on_expedition;
    uint32_t expedition_start;
    uint16_t expedition_minutes;
    // --- v0.4 Signal storm (WiFi devboard) ---
    uint16_t eggs_storm;   // board-exclusive "storm eggs" (lifetime collectible)
};

struct PersistentSettings {
    uint8_t vibration;
    uint8_t sound;
};

struct GameState {
    struct PersistentGameState persistent;
    struct PersistentSettings settings;
    // Transient
    uint32_t next_animation_index;
    uint8_t display_state; // enum DisplayState
    struct Catch last_catch;   // for the catch reveal (transient)
    uint8_t reveal_ticks;      // >0 = show reveal banner (transient)
    char reveal_text[24];      // banner text (transient)
    uint8_t journey_ready;     // transient: expedition returned, show log
    char journey_log[96];      // transient: journey log text
    uint8_t board_present;     // transient: WiFi devboard replied on last forage
    uint8_t last_wifi;         // transient: nearby AP count from last board probe
    int8_t last_rssi;          // transient: strongest AP RSSI from last board probe
    uint8_t storm_ready;       // transient: show the Signal Storm result screen
};

#endif
