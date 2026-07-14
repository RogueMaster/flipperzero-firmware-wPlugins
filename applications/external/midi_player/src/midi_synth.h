#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MidiSynthStylePiano = 0,
    MidiSynthStyleOrgan,
    MidiSynthStyleGuitar,
    MidiSynthStyleBass,
    MidiSynthStyleFlute,
    MidiSynthStyleStrings,
    MidiSynthStyleSynth,
    MidiSynthStyleDrums,
} MidiSynthStyle;

typedef struct {
    bool acquired;
    bool active;
    float volume;
    uint32_t voice_start_ms;
    uint8_t note;
    uint8_t velocity;
    float output_volume;
    bool release_pending;
    uint32_t release_time_ms;
    MidiSynthStyle style;
} MidiSynth;

void midi_synth_init(MidiSynth* synth);
bool midi_synth_acquire(MidiSynth* synth);
void midi_synth_note_on(MidiSynth* synth, uint8_t note, uint8_t velocity, MidiSynthStyle style);
void midi_synth_render(MidiSynth* synth);
void midi_synth_set_volume(MidiSynth* synth, float volume);
void midi_synth_release_note(MidiSynth* synth, uint32_t minimum_duration_ms);
void midi_synth_stop(MidiSynth* synth);
void midi_synth_release(MidiSynth* synth);
MidiSynthStyle midi_synth_style_from_program(uint8_t program);

#ifdef __cplusplus
}
#endif
