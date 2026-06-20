#pragma once

#include "midi_parser.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Storage Storage;
typedef struct MidiPlayer MidiPlayer;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MidiPlayerStatusIdle = 0,
    MidiPlayerStatusPlaying,
    MidiPlayerStatusPaused,
    MidiPlayerStatusStopped,
    MidiPlayerStatusDone,
    MidiPlayerStatusError,
} MidiPlayerStatus;

typedef enum {
    MidiPlayerCommandStop = 0,
    MidiPlayerCommandPause,
    MidiPlayerCommandResume,
    MidiPlayerCommandSetVolume,
} MidiPlayerCommandType;

typedef struct {
    MidiPlayerCommandType type;
    float volume;
} MidiPlayerCommand;

typedef struct {
    MidiPlayerStatus status;
    MidiParserStatus parser_status;
    uint32_t tick;
    uint8_t current_note;
    uint8_t volume_percent;
    bool has_current_note;
    char message[64];
} MidiPlayerState;

MidiPlayer* midi_player_alloc(Storage* storage);
void midi_player_free(MidiPlayer* player);
bool midi_player_start_path(MidiPlayer* player, const char* path);
bool midi_player_send_command(MidiPlayer* player, const MidiPlayerCommand* command);
MidiPlayerState midi_player_get_state(MidiPlayer* player);
const char* midi_player_status_string(MidiPlayerStatus status);

#ifdef __cplusplus
}
#endif
