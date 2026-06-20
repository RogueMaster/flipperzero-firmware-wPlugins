#pragma once

#include "midi_player.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FuriMessageQueue FuriMessageQueue;
typedef struct Gui Gui;
typedef struct MidiPlayerUi MidiPlayerUi;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MidiPlayerUiEventBack = 0,
    MidiPlayerUiEventPlayPause,
    MidiPlayerUiEventVolumeUp,
    MidiPlayerUiEventVolumeDown,
} MidiPlayerUiEventType;

typedef struct {
    MidiPlayerUiEventType type;
} MidiPlayerUiEvent;

typedef struct {
    MidiPlayerStatus status;
    uint32_t tick;
    uint8_t current_note;
    uint8_t volume_percent;
    bool has_current_note;
    char message[64];
    char filename[40];
} MidiPlayerUiState;

MidiPlayerUi* midi_ui_alloc(FuriMessageQueue* events);
void midi_ui_attach(MidiPlayerUi* ui, Gui* gui);
void midi_ui_update(MidiPlayerUi* ui, const MidiPlayerUiState* state);
void midi_ui_free(MidiPlayerUi* ui, Gui* gui);

#ifdef __cplusplus
}
#endif
