#pragma once

#include <furi.h>
#include "usb_hid_switch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MACRO_MAX_EVENTS   1000
#define MACRO_NAME_MAX_LEN 32
#define MACRO_FILE_MAGIC   0x4D414353 // "MACR" in hex

// Macro event types
typedef enum {
    MacroEventButtonPress,
    MacroEventButtonRelease,
    MacroEventDelay,
    MacroEventDPad,
    MacroEventStick,
} MacroEventType;

// Macro event structure
typedef struct {
    MacroEventType type;
    uint32_t timestamp; // Time since start in ms
    union {
        struct {
            uint16_t button; // Button bit flag
        } button;
        struct {
            uint8_t direction; // HAT direction
        } dpad;
        struct {
            uint16_t lx, ly, rx, ry;
        } stick;
        struct {
            uint32_t duration_ms;
        } delay;
    } data;
} MacroEvent;

// Macro structure
typedef struct {
    char name[MACRO_NAME_MAX_LEN];
    uint32_t event_count;
    uint32_t total_duration_ms;
    MacroEvent events[MACRO_MAX_EVENTS];
} Macro;

// Macro recorder
typedef struct {
    Macro* macro;
    bool recording;
    uint32_t start_time;
    uint32_t last_event_time;
    SwitchControllerState last_state;
} MacroRecorder;

// Macro player
typedef struct {
    Macro* macro;
    bool playing;
    bool loop;
    uint32_t start_time;
    uint32_t current_event_index;
} MacroPlayer;

// Create a new macro
Macro* macro_alloc();

// Free macro
void macro_free(Macro* macro);

// Clear macro (reset all events)
void macro_clear(Macro* macro);

// Create macro recorder
MacroRecorder* macro_recorder_alloc();

// Free macro recorder
void macro_recorder_free(MacroRecorder* recorder);

// Start recording
void macro_recorder_start(MacroRecorder* recorder, Macro* macro, const char* name);

// Stop recording
void macro_recorder_stop(MacroRecorder* recorder);

// Record controller state change
void macro_recorder_update(MacroRecorder* recorder, SwitchControllerState* state);

// Create macro player
MacroPlayer* macro_player_alloc();

// Free macro player
void macro_player_free(MacroPlayer* player);

// Start playing macro
void macro_player_start(MacroPlayer* player, Macro* macro, bool loop);

// Stop playing macro
void macro_player_stop(MacroPlayer* player);

// Update player and get current controller state (returns true if still playing)
bool macro_player_update(MacroPlayer* player, SwitchControllerState* state);

// Save macro to file
bool macro_save(Macro* macro, const char* path);

// Load macro from file
Macro* macro_load(const char* path);

#ifdef __cplusplus
}
#endif
