#include "midi_player.h"

#include "midi_synth.h"

#include <furi.h>
#include <storage/storage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIDI_PLAYER_COMMAND_QUEUE_DEPTH          (8U)
#define MIDI_PLAYER_THREAD_STACK_SIZE            (12288U)
#define MIDI_PLAYER_MAX_ACTIVE_NOTES             (128U)
#define MIDI_PLAYER_DEFAULT_TEMPO_US_PER_QUARTER (500000UL)
#define MIDI_PLAYER_DRUM_CHANNEL                 (9U)
#define MIDI_PLAYER_VOLUME_STEP                  (0.10f)
#define MIDI_PLAYER_DEFAULT_VOLUME               (0.85f)
#define MIDI_PLAYER_MIN_AUDIBLE_NOTE_MS          (38U)

typedef struct {
    File* file;
} MidiStorageReader;

typedef struct {
    bool active;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint32_t start_tick;
    MidiSynthStyle style;
} MidiActiveNote;

typedef struct {
    uint32_t notes;
    uint32_t total_note;
    uint32_t total_velocity;
    uint8_t min_note;
    uint8_t max_note;
} MidiChannelStats;

struct MidiPlayer {
    Storage* storage;
    FuriThread* thread;
    FuriMessageQueue* commands;
    FuriMutex* state_mutex;
    MidiPlayerState state;
    char path[128];
    float volume;
    bool running;
};

static float midi_player_clamp_volume(float volume) {
    if(volume < 0.0f) {
        return 0.0f;
    }
    if(volume > 1.0f) {
        return 1.0f;
    }
    return volume;
}

static uint8_t midi_player_volume_percent(float volume) {
    volume = midi_player_clamp_volume(volume);
    return (uint8_t)((volume * 100.0f) + 0.5f);
}

static size_t midi_storage_reader_read(void* context, uint8_t* data, size_t size) {
    MidiStorageReader* reader = context;
    return storage_file_read(reader->file, data, size);
}

static bool midi_storage_reader_seek(void* context, uint32_t position) {
    MidiStorageReader* reader = context;
    return storage_file_seek(reader->file, position, true);
}

static uint32_t midi_storage_reader_tell(void* context) {
    MidiStorageReader* reader = context;
    return (uint32_t)storage_file_tell(reader->file);
}

static bool midi_storage_reader_eof(void* context) {
    MidiStorageReader* reader = context;
    return storage_file_eof(reader->file);
}

static void midi_player_state_set(
    MidiPlayer* player,
    MidiPlayerStatus status,
    MidiParserStatus parser_status,
    const char* message) {
    if(furi_mutex_acquire(player->state_mutex, FuriWaitForever) == FuriStatusOk) {
        player->state.status = status;
        player->state.parser_status = parser_status;
        if(message) {
            snprintf(player->state.message, sizeof(player->state.message), "%s", message);
        }
        furi_mutex_release(player->state_mutex);
    }
}

static void
    midi_player_state_progress(MidiPlayer* player, uint32_t tick, bool has_note, uint8_t note) {
    if(furi_mutex_acquire(player->state_mutex, FuriWaitForever) == FuriStatusOk) {
        player->state.tick = tick;
        player->state.has_current_note = has_note;
        player->state.current_note = note;
        furi_mutex_release(player->state_mutex);
    }
}

static void midi_player_state_volume(MidiPlayer* player, float volume, bool show_message) {
    if(furi_mutex_acquire(player->state_mutex, FuriWaitForever) == FuriStatusOk) {
        player->state.volume_percent = midi_player_volume_percent(volume);
        if(show_message) {
            snprintf(
                player->state.message,
                sizeof(player->state.message),
                "Vol %u%%",
                player->state.volume_percent);
        }
        furi_mutex_release(player->state_mutex);
    }
}

static void midi_active_notes_clear(MidiActiveNote* notes) {
    memset(notes, 0, sizeof(MidiActiveNote) * MIDI_PLAYER_MAX_ACTIVE_NOTES);
}

static void midi_active_notes_note_on(
    MidiActiveNote* notes,
    uint8_t channel,
    uint8_t note,
    uint8_t velocity,
    uint32_t start_tick,
    MidiSynthStyle style) {
    for(uint8_t i = 0U; i < MIDI_PLAYER_MAX_ACTIVE_NOTES; i++) {
        if(notes[i].active && notes[i].channel == channel && notes[i].note == note) {
            notes[i].velocity = velocity;
            notes[i].start_tick = start_tick;
            notes[i].style = style;
            return;
        }
    }

    for(uint8_t i = 0U; i < MIDI_PLAYER_MAX_ACTIVE_NOTES; i++) {
        if(!notes[i].active) {
            notes[i].active = true;
            notes[i].channel = channel;
            notes[i].note = note;
            notes[i].velocity = velocity;
            notes[i].start_tick = start_tick;
            notes[i].style = style;
            return;
        }
    }

    uint8_t replace_index = 0U;
    for(uint8_t i = 1U; i < MIDI_PLAYER_MAX_ACTIVE_NOTES; i++) {
        if(notes[i].start_tick < notes[replace_index].start_tick) {
            replace_index = i;
        }
    }

    notes[replace_index].channel = channel;
    notes[replace_index].note = note;
    notes[replace_index].velocity = velocity;
    notes[replace_index].start_tick = start_tick;
    notes[replace_index].style = style;
}

static void midi_active_notes_note_off(MidiActiveNote* notes, uint8_t channel, uint8_t note) {
    for(uint8_t i = 0U; i < MIDI_PLAYER_MAX_ACTIVE_NOTES; i++) {
        if(notes[i].active && notes[i].channel == channel && notes[i].note == note) {
            notes[i].active = false;
        }
    }
}

static bool midi_active_notes_select_best(
    MidiActiveNote* notes,
    bool use_channel,
    uint8_t channel,
    MidiActiveNote* selected) {
    bool found = false;
    MidiActiveNote best = {0};
    for(uint8_t i = 0U; i < MIDI_PLAYER_MAX_ACTIVE_NOTES; i++) {
        if(!notes[i].active || (use_channel && notes[i].channel != channel)) {
            continue;
        }

        if(!found || notes[i].start_tick > best.start_tick ||
           (notes[i].start_tick == best.start_tick && notes[i].note > best.note)) {
            found = true;
            best = notes[i];
        }
    }

    if(found) {
        *selected = best;
    }
    return found;
}

static void midi_player_apply_active_note(
    MidiPlayer* player,
    MidiSynth* synth,
    MidiActiveNote* notes,
    uint32_t tick,
    bool has_lead_channel,
    uint8_t lead_channel) {
    MidiActiveNote selected;
    bool found = false;
    if(has_lead_channel) {
        found = midi_active_notes_select_best(notes, true, lead_channel, &selected);
    }
    if(!found) {
        found = midi_active_notes_select_best(notes, false, 0U, &selected);
    }

    if(found) {
        midi_synth_note_on(synth, selected.note, selected.velocity, selected.style);
        midi_player_state_progress(player, tick, true, selected.note);
    } else {
        midi_synth_release_note(synth, MIDI_PLAYER_MIN_AUDIBLE_NOTE_MS);
        midi_player_state_progress(player, tick, false, 0U);
    }
}

static bool midi_player_handle_command(
    MidiPlayer* player,
    MidiSynth* synth,
    const MidiPlayerCommand* command,
    bool* paused) {
    switch(command->type) {
    case MidiPlayerCommandStop:
        midi_synth_stop(synth);
        midi_player_state_set(player, MidiPlayerStatusStopped, MidiParserStatusOk, "Stopped");
        return false;
    case MidiPlayerCommandPause:
        *paused = true;
        midi_synth_stop(synth);
        midi_player_state_set(player, MidiPlayerStatusPaused, MidiParserStatusOk, "Paused");
        return true;
    case MidiPlayerCommandResume:
        *paused = false;
        midi_player_state_set(player, MidiPlayerStatusPlaying, MidiParserStatusOk, "Playing");
        return true;
    case MidiPlayerCommandSetVolume:
        player->volume = midi_player_clamp_volume(command->volume);
        midi_synth_set_volume(synth, player->volume);
        midi_player_state_volume(player, player->volume, true);
        return true;
    default:
        return true;
    }
}

static bool midi_player_wait_while_paused(
    MidiPlayer* player,
    MidiSynth* synth,
    MidiActiveNote* notes,
    uint32_t tick,
    bool has_lead_channel,
    uint8_t lead_channel,
    bool* paused) {
    while(*paused) {
        MidiPlayerCommand command;
        if(furi_message_queue_get(player->commands, &command, FuriWaitForever) == FuriStatusOk) {
            if(!midi_player_handle_command(player, synth, &command, paused)) {
                return false;
            }
        }
    }

    midi_player_apply_active_note(player, synth, notes, tick, has_lead_channel, lead_channel);
    return true;
}

static bool midi_player_delay_us(
    MidiPlayer* player,
    MidiSynth* synth,
    MidiActiveNote* notes,
    uint32_t tick,
    uint64_t delay_us,
    bool has_lead_channel,
    uint8_t lead_channel,
    bool* paused) {
    while(delay_us > 0U) {
        uint32_t slice_ms = (delay_us + 999U) / 1000U;
        if(slice_ms == 0U) {
            slice_ms = 1U;
        } else if(slice_ms > 25U) {
            slice_ms = 25U;
        }

        MidiPlayerCommand command;
        if(furi_message_queue_get(player->commands, &command, slice_ms) == FuriStatusOk) {
            if(!midi_player_handle_command(player, synth, &command, paused)) {
                return false;
            }
            if(*paused &&
               !midi_player_wait_while_paused(
                   player, synth, notes, tick, has_lead_channel, lead_channel, paused)) {
                return false;
            }
        }

        const uint64_t consumed_us = (uint64_t)slice_ms * 1000ULL;
        delay_us = delay_us > consumed_us ? delay_us - consumed_us : 0U;
        midi_synth_render(synth);
    }

    return true;
}

static uint64_t midi_player_calculate_delay_us(
    const MidiHeader* header,
    uint32_t delta_ticks,
    uint32_t tempo_us_per_quarter) {
    if(delta_ticks == 0U || !header) {
        return 0U;
    }

    if(header->timing_mode == MidiTimingModeSmpte) {
        return header->ticks_per_second == 0U ?
                   0U :
                   ((uint64_t)delta_ticks * 1000000ULL) / (uint64_t)header->ticks_per_second;
    }

    return header->ticks_per_quarter == 0U ?
               0U :
               ((uint64_t)delta_ticks * (uint64_t)tempo_us_per_quarter) /
                   (uint64_t)header->ticks_per_quarter;
}

static void midi_channel_stats_note_on(
    MidiChannelStats* stats,
    uint8_t channel,
    uint8_t note,
    uint8_t velocity) {
    MidiChannelStats* channel_stats = &stats[channel];
    if(channel_stats->notes == 0U) {
        channel_stats->min_note = note;
        channel_stats->max_note = note;
    } else {
        if(note < channel_stats->min_note) {
            channel_stats->min_note = note;
        }
        if(note > channel_stats->max_note) {
            channel_stats->max_note = note;
        }
    }

    channel_stats->notes++;
    channel_stats->total_note += note;
    channel_stats->total_velocity += velocity;
}

static bool midi_player_choose_lead_channel_from_stats(
    const MidiChannelStats* stats,
    uint8_t* lead_channel) {
    bool found = false;
    uint8_t best_channel = 0U;
    int32_t best_score = INT32_MIN;

    for(uint8_t channel = 0U; channel < 16U; channel++) {
        const MidiChannelStats* channel_stats = &stats[channel];
        if(channel_stats->notes == 0U || channel == MIDI_PLAYER_DRUM_CHANNEL) {
            continue;
        }

        const uint32_t avg_note = channel_stats->total_note / channel_stats->notes;
        const uint32_t avg_velocity = channel_stats->total_velocity / channel_stats->notes;
        const uint32_t capped_notes = channel_stats->notes > 1200U ? 1200U : channel_stats->notes;

        int32_t score = (int32_t)(avg_note * 6U) + (int32_t)(channel_stats->max_note * 3U) +
                        (int32_t)(avg_velocity * 2U) + (int32_t)(capped_notes / 3U);

        if(avg_note < 50U) {
            score -= 180;
        }
        if(channel_stats->notes < 12U) {
            score -= 120;
        }
        if(channel_stats->notes > 5000U) {
            score -= (int32_t)((channel_stats->notes - 5000U) / 20U);
        }

        if(!found || score > best_score) {
            found = true;
            best_score = score;
            best_channel = channel;
        }
    }

    if(found) {
        *lead_channel = best_channel;
    }
    return found;
}

static int32_t midi_player_thread(void* context) {
    MidiPlayer* player = context;
    File* file = storage_file_alloc(player->storage);
    MidiSynth synth;
    MidiParser parser;
    MidiStorageReader storage_reader = {.file = file};
    MidiActiveNote active_notes[MIDI_PLAYER_MAX_ACTIVE_NOTES];
    MidiSynthStyle channel_styles[16];
    MidiChannelStats channel_stats[16];
    bool has_lead_channel = false;
    uint8_t lead_channel = 0U;
    bool paused = false;
    uint32_t tempo_us_per_quarter = MIDI_PLAYER_DEFAULT_TEMPO_US_PER_QUARTER;
    uint32_t previous_tick = 0U;
    uint16_t events_since_command_poll = 0U;

    midi_synth_init(&synth);
    midi_parser_init(&parser);
    midi_active_notes_clear(active_notes);
    memset(channel_stats, 0, sizeof(channel_stats));
    for(uint8_t i = 0U; i < 16U; i++) {
        channel_styles[i] = MidiSynthStylePiano;
    }

    if(!storage_file_open(file, player->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        midi_player_state_set(
            player, MidiPlayerStatusError, MidiParserStatusIoError, "Open failed");
        goto cleanup;
    }

    MidiByteReader reader = {
        .read = midi_storage_reader_read,
        .seek = midi_storage_reader_seek,
        .tell = midi_storage_reader_tell,
        .eof = midi_storage_reader_eof,
        .context = &storage_reader,
    };

    MidiParserStatus status = midi_parser_open(&parser, &reader);
    if(status == MidiParserStatusOk) {
        status = midi_parser_read_header(&parser);
    }
    if(status != MidiParserStatusOk) {
        midi_player_state_set(
            player, MidiPlayerStatusError, status, midi_parser_status_string(status));
        goto cleanup;
    }

    if(!midi_synth_acquire(&synth)) {
        midi_player_state_set(
            player, MidiPlayerStatusError, MidiParserStatusIoError, "Speaker busy");
        goto cleanup;
    }
    midi_synth_set_volume(&synth, player->volume);
    midi_player_state_volume(player, player->volume, false);
    midi_player_state_set(player, MidiPlayerStatusPlaying, MidiParserStatusOk, "Playing");

    while(true) {
        events_since_command_poll++;
        if(events_since_command_poll >= 32U) {
            events_since_command_poll = 0U;
            MidiPlayerCommand command;
            if(furi_message_queue_get(player->commands, &command, 0U) == FuriStatusOk) {
                if(!midi_player_handle_command(player, &synth, &command, &paused)) {
                    break;
                }
                if(paused && !midi_player_wait_while_paused(
                                 player,
                                 &synth,
                                 active_notes,
                                 previous_tick,
                                 has_lead_channel,
                                 lead_channel,
                                 &paused)) {
                    break;
                }
            }
        }

        MidiEvent event;
        status = midi_parser_next_event(&parser, &event);
        if(status == MidiParserStatusEof) {
            midi_player_state_set(player, MidiPlayerStatusDone, MidiParserStatusOk, "Done");
            break;
        }
        if(status != MidiParserStatusOk) {
            midi_player_state_set(
                player, MidiPlayerStatusError, status, midi_parser_status_string(status));
            break;
        }

        const uint32_t delta_ticks = event.absolute_tick - previous_tick;
        previous_tick = event.absolute_tick;
        const uint64_t delay_us = midi_player_calculate_delay_us(
            midi_parser_get_header(&parser), delta_ticks, tempo_us_per_quarter);

        if(!midi_player_delay_us(
               player,
               &synth,
               active_notes,
               event.absolute_tick,
               delay_us,
               has_lead_channel,
               lead_channel,
               &paused)) {
            break;
        }

        switch(event.type) {
        case MidiEventTypeTempo:
            if(event.tempo_us_per_quarter > 0U) {
                tempo_us_per_quarter = event.tempo_us_per_quarter;
            }
            break;
        case MidiEventTypeNoteOn:
            if(event.channel != MIDI_PLAYER_DRUM_CHANNEL) {
                midi_channel_stats_note_on(
                    channel_stats, event.channel, event.note, event.velocity);
                if(!has_lead_channel || channel_stats[event.channel].notes <= 64U ||
                   (channel_stats[event.channel].notes % 32U) == 0U) {
                    const bool had_lead_channel = has_lead_channel;
                    const uint8_t old_lead_channel = lead_channel;
                    has_lead_channel =
                        midi_player_choose_lead_channel_from_stats(channel_stats, &lead_channel);
                    if(has_lead_channel &&
                       (!had_lead_channel || old_lead_channel != lead_channel)) {
                        char message[32];
                        snprintf(
                            message, sizeof(message), "Lead Ch %u", (uint8_t)(lead_channel + 1U));
                        midi_player_state_set(
                            player, MidiPlayerStatusPlaying, MidiParserStatusOk, message);
                    }
                }
                midi_active_notes_note_on(
                    active_notes,
                    event.channel,
                    event.note,
                    event.velocity,
                    event.absolute_tick,
                    channel_styles[event.channel]);
                midi_player_apply_active_note(
                    player,
                    &synth,
                    active_notes,
                    event.absolute_tick,
                    has_lead_channel,
                    lead_channel);
            }
            break;
        case MidiEventTypeNoteOff:
            if(event.channel != MIDI_PLAYER_DRUM_CHANNEL) {
                midi_active_notes_note_off(active_notes, event.channel, event.note);
                midi_player_apply_active_note(
                    player,
                    &synth,
                    active_notes,
                    event.absolute_tick,
                    has_lead_channel,
                    lead_channel);
            }
            break;
        case MidiEventTypeProgramChange:
            channel_styles[event.channel] = midi_synth_style_from_program(event.program);
            break;
        case MidiEventTypeEndOfTrack:
            midi_player_state_set(player, MidiPlayerStatusDone, MidiParserStatusOk, "Done");
            goto cleanup;
        default:
            break;
        }
    }

cleanup:
    midi_synth_release(&synth);
    midi_parser_close(&parser);
    if(storage_file_is_open(file)) {
        storage_file_close(file);
    }
    storage_file_free(file);

    if(furi_mutex_acquire(player->state_mutex, FuriWaitForever) == FuriStatusOk) {
        player->running = false;
        player->state.has_current_note = false;
        furi_mutex_release(player->state_mutex);
    }

    return 0;
}

MidiPlayer* midi_player_alloc(Storage* storage) {
    if(!storage) {
        return NULL;
    }

    MidiPlayer* player = malloc(sizeof(MidiPlayer));
    if(!player) {
        return NULL;
    }

    memset(player, 0, sizeof(*player));
    player->storage = storage;
    player->commands =
        furi_message_queue_alloc(MIDI_PLAYER_COMMAND_QUEUE_DEPTH, sizeof(MidiPlayerCommand));
    player->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    player->volume = MIDI_PLAYER_DEFAULT_VOLUME;
    player->state.status = MidiPlayerStatusIdle;
    player->state.volume_percent = midi_player_volume_percent(player->volume);
    snprintf(player->state.message, sizeof(player->state.message), "%s", "Idle");

    if(!player->commands || !player->state_mutex) {
        midi_player_free(player);
        return NULL;
    }

    return player;
}

void midi_player_free(MidiPlayer* player) {
    if(!player) {
        return;
    }

    if(player->thread) {
        MidiPlayerCommand stop_command = {.type = MidiPlayerCommandStop, .volume = 0.0f};
        midi_player_send_command(player, &stop_command);
        furi_thread_join(player->thread);
        furi_thread_free(player->thread);
        player->thread = NULL;
    }

    if(player->commands) {
        furi_message_queue_free(player->commands);
    }
    if(player->state_mutex) {
        furi_mutex_free(player->state_mutex);
    }
    free(player);
}

bool midi_player_start_path(MidiPlayer* player, const char* path) {
    if(!player || !path || player->thread) {
        return false;
    }

    snprintf(player->path, sizeof(player->path), "%s", path);
    player->thread = furi_thread_alloc_ex(
        "MIDI Player", MIDI_PLAYER_THREAD_STACK_SIZE, midi_player_thread, player);
    if(!player->thread) {
        return false;
    }

    if(furi_mutex_acquire(player->state_mutex, FuriWaitForever) == FuriStatusOk) {
        player->running = true;
        player->state.status = MidiPlayerStatusIdle;
        player->state.tick = 0U;
        player->state.has_current_note = false;
        player->state.volume_percent = midi_player_volume_percent(player->volume);
        snprintf(player->state.message, sizeof(player->state.message), "%s", "Loading");
        furi_mutex_release(player->state_mutex);
    }

    furi_thread_start(player->thread);
    return true;
}

bool midi_player_send_command(MidiPlayer* player, const MidiPlayerCommand* command) {
    if(!player || !command || !player->commands) {
        return false;
    }
    return furi_message_queue_put(player->commands, command, 0U) == FuriStatusOk;
}

MidiPlayerState midi_player_get_state(MidiPlayer* player) {
    MidiPlayerState state = {
        .status = MidiPlayerStatusError,
        .parser_status = MidiParserStatusInvalidArgument,
        .tick = 0U,
        .current_note = 0U,
        .volume_percent = 0U,
        .has_current_note = false,
        .message = "No player",
    };

    if(!player) {
        return state;
    }

    if(furi_mutex_acquire(player->state_mutex, FuriWaitForever) == FuriStatusOk) {
        state = player->state;
        furi_mutex_release(player->state_mutex);
    }

    return state;
}

const char* midi_player_status_string(MidiPlayerStatus status) {
    switch(status) {
    case MidiPlayerStatusIdle:
        return "Idle";
    case MidiPlayerStatusPlaying:
        return "Playing";
    case MidiPlayerStatusPaused:
        return "Paused";
    case MidiPlayerStatusStopped:
        return "Stopped";
    case MidiPlayerStatusDone:
        return "Done";
    case MidiPlayerStatusError:
        return "Error";
    default:
        return "Unknown";
    }
}
