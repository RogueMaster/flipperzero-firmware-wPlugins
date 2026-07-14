#include "macro.h"
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>

// Create a new macro
Macro* macro_alloc() {
    Macro* macro = malloc(sizeof(Macro));
    macro_clear(macro);
    return macro;
}

// Free macro
void macro_free(Macro* macro) {
    free(macro);
}

// Clear macro
void macro_clear(Macro* macro) {
    memset(macro->name, 0, MACRO_NAME_MAX_LEN);
    macro->event_count = 0;
    macro->total_duration_ms = 0;
}

// Create macro recorder
MacroRecorder* macro_recorder_alloc() {
    MacroRecorder* recorder = malloc(sizeof(MacroRecorder));
    recorder->macro = NULL;
    recorder->recording = false;
    recorder->start_time = 0;
    recorder->last_event_time = 0;
    usb_hid_switch_reset_state(&recorder->last_state);
    return recorder;
}

// Free macro recorder
void macro_recorder_free(MacroRecorder* recorder) {
    free(recorder);
}

// Start recording
void macro_recorder_start(MacroRecorder* recorder, Macro* macro, const char* name) {
    recorder->macro = macro;
    macro_clear(macro);
    snprintf(macro->name, MACRO_NAME_MAX_LEN, "%s", name);
    recorder->recording = true;
    recorder->start_time = furi_get_tick();
    recorder->last_event_time = recorder->start_time;
    usb_hid_switch_reset_state(&recorder->last_state);
}

// Stop recording
void macro_recorder_stop(MacroRecorder* recorder) {
    recorder->recording = false;
    if(recorder->macro) {
        recorder->macro->total_duration_ms = furi_get_tick() - recorder->start_time;
    }
}

// Helper to add event to macro
static void macro_add_event(Macro* macro, MacroEvent* event) {
    if(macro->event_count < MACRO_MAX_EVENTS) {
        macro->events[macro->event_count++] = *event;
    }
}

// Record controller state change
void macro_recorder_update(MacroRecorder* recorder, SwitchControllerState* state) {
    if(!recorder->recording || !recorder->macro) return;

    uint32_t current_time = furi_get_tick();
    uint32_t timestamp = current_time - recorder->start_time;

    // Check for button changes
    uint16_t button_changes = state->buttons ^ recorder->last_state.buttons;
    if(button_changes) {
        for(int i = 0; i < 14; i++) {
            uint16_t button_bit = (1 << i);
            if(button_changes & button_bit) {
                MacroEvent event = {0};
                event.timestamp = timestamp;
                event.data.button.button = button_bit;

                if(state->buttons & button_bit) {
                    event.type = MacroEventButtonPress;
                } else {
                    event.type = MacroEventButtonRelease;
                }

                macro_add_event(recorder->macro, &event);
            }
        }
    }

    // Check for D-Pad changes
    if(state->hat != recorder->last_state.hat) {
        MacroEvent event = {0};
        event.type = MacroEventDPad;
        event.timestamp = timestamp;
        event.data.dpad.direction = state->hat;
        macro_add_event(recorder->macro, &event);
    }

    // Check for analog stick changes (only record if significant change)
    if(abs((int)state->lx - (int)recorder->last_state.lx) > 1000 ||
       abs((int)state->ly - (int)recorder->last_state.ly) > 1000 ||
       abs((int)state->rx - (int)recorder->last_state.rx) > 1000 ||
       abs((int)state->ry - (int)recorder->last_state.ry) > 1000) {
        MacroEvent event = {0};
        event.type = MacroEventStick;
        event.timestamp = timestamp;
        event.data.stick.lx = state->lx;
        event.data.stick.ly = state->ly;
        event.data.stick.rx = state->rx;
        event.data.stick.ry = state->ry;
        macro_add_event(recorder->macro, &event);
    }

    recorder->last_state = *state;
    recorder->last_event_time = current_time;
}

// Create macro player
MacroPlayer* macro_player_alloc() {
    MacroPlayer* player = malloc(sizeof(MacroPlayer));
    player->macro = NULL;
    player->playing = false;
    player->loop = false;
    player->start_time = 0;
    player->current_event_index = 0;
    return player;
}

// Free macro player
void macro_player_free(MacroPlayer* player) {
    free(player);
}

// Start playing macro
void macro_player_start(MacroPlayer* player, Macro* macro, bool loop) {
    player->macro = macro;
    player->playing = true;
    player->loop = loop;
    player->start_time = furi_get_tick();
    player->current_event_index = 0;
}

// Stop playing macro
void macro_player_stop(MacroPlayer* player) {
    player->playing = false;
}

// Update player and get current controller state
bool macro_player_update(MacroPlayer* player, SwitchControllerState* state) {
    if(!player->playing || !player->macro) return false;

    uint32_t current_time = furi_get_tick();
    uint32_t elapsed_time = current_time - player->start_time;

    // Apply all events up to current time
    while(player->current_event_index < player->macro->event_count) {
        MacroEvent* event = &player->macro->events[player->current_event_index];

        if(event->timestamp > elapsed_time) {
            // Haven't reached this event yet
            break;
        }

        // Apply event
        switch(event->type) {
        case MacroEventButtonPress:
            state->buttons |= event->data.button.button;
            break;
        case MacroEventButtonRelease:
            state->buttons &= ~event->data.button.button;
            break;
        case MacroEventDPad:
            state->hat = event->data.dpad.direction;
            break;
        case MacroEventStick:
            state->lx = event->data.stick.lx;
            state->ly = event->data.stick.ly;
            state->rx = event->data.stick.rx;
            state->ry = event->data.stick.ry;
            break;
        case MacroEventDelay:
            // Delays are handled by timestamp
            break;
        }

        player->current_event_index++;
    }

    // Check if we've finished playing
    if(player->current_event_index >= player->macro->event_count) {
        if(player->loop) {
            // Restart from beginning
            player->current_event_index = 0;
            player->start_time = current_time;
        } else {
            // Stop playing
            player->playing = false;
            return false;
        }
    }

    return true;
}

// Save macro to file
bool macro_save(Macro* macro, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool success = false;

    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) break;

        // Write magic number
        uint32_t magic = MACRO_FILE_MAGIC;
        if(storage_file_write(file, &magic, sizeof(magic)) != sizeof(magic)) break;

        // Write name
        if(storage_file_write(file, macro->name, MACRO_NAME_MAX_LEN) != MACRO_NAME_MAX_LEN) break;

        // Write event count
        if(storage_file_write(file, &macro->event_count, sizeof(macro->event_count)) !=
           sizeof(macro->event_count))
            break;

        // Write total duration
        if(storage_file_write(file, &macro->total_duration_ms, sizeof(macro->total_duration_ms)) !=
           sizeof(macro->total_duration_ms))
            break;

        // Write events
        size_t events_size = macro->event_count * sizeof(MacroEvent);
        if(storage_file_write(file, macro->events, events_size) != events_size) break;

        success = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

// Load macro from file
Macro* macro_load(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    Macro* macro = NULL;

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        // Read and verify magic number
        uint32_t magic;
        if(storage_file_read(file, &magic, sizeof(magic)) != sizeof(magic)) break;
        if(magic != MACRO_FILE_MAGIC) break;

        // Allocate macro
        macro = macro_alloc();

        // Read name
        if(storage_file_read(file, macro->name, MACRO_NAME_MAX_LEN) != MACRO_NAME_MAX_LEN) {
            macro_free(macro);
            macro = NULL;
            break;
        }

        // Read event count
        if(storage_file_read(file, &macro->event_count, sizeof(macro->event_count)) !=
           sizeof(macro->event_count)) {
            macro_free(macro);
            macro = NULL;
            break;
        }

        // Validate event count
        if(macro->event_count > MACRO_MAX_EVENTS) {
            macro_free(macro);
            macro = NULL;
            break;
        }

        // Read total duration
        if(storage_file_read(file, &macro->total_duration_ms, sizeof(macro->total_duration_ms)) !=
           sizeof(macro->total_duration_ms)) {
            macro_free(macro);
            macro = NULL;
            break;
        }

        // Read events
        size_t events_size = macro->event_count * sizeof(MacroEvent);
        if(storage_file_read(file, macro->events, events_size) != events_size) {
            macro_free(macro);
            macro = NULL;
            break;
        }

    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return macro;
}
