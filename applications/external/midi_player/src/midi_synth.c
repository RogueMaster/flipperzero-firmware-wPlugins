#include "midi_synth.h"

#include <furi.h>
#include <furi_hal.h>

static float midi_synth_note_frequency(uint8_t note) {
    static const float semitone_ratio[12] = {
        1.000000f,
        1.059463f,
        1.122462f,
        1.189207f,
        1.259921f,
        1.334840f,
        1.414214f,
        1.498307f,
        1.587401f,
        1.681793f,
        1.781797f,
        1.887749f,
    };

    float frequency = 8.175799f * semitone_ratio[note % 12U];
    for(uint8_t octave = 0U; octave < (note / 12U); octave++) {
        frequency *= 2.0f;
    }
    return frequency;
}

static float midi_synth_clamp_frequency(float frequency) {
    if(frequency < 80.0f) {
        return 80.0f;
    }
    if(frequency > 4500.0f) {
        return 4500.0f;
    }
    return frequency;
}

static float midi_synth_clamp_volume(float volume) {
    if(volume < 0.0f) {
        return 0.0f;
    }
    if(volume > 1.0f) {
        return 1.0f;
    }
    return volume;
}

static float midi_synth_style_level(MidiSynthStyle style) {
    switch(style) {
    case MidiSynthStylePiano:
        return 1.00f;
    case MidiSynthStyleOrgan:
        return 0.95f;
    case MidiSynthStyleGuitar:
        return 0.88f;
    case MidiSynthStyleBass:
        return 1.00f;
    case MidiSynthStyleFlute:
        return 0.82f;
    case MidiSynthStyleStrings:
        return 0.88f;
    case MidiSynthStyleSynth:
        return 0.95f;
    case MidiSynthStyleDrums:
        return 0.90f;
    default:
        return 0.90f;
    }
}

static float midi_synth_style_frequency(MidiSynthStyle style, uint8_t note) {
    uint8_t played_note = note;
    if(style == MidiSynthStyleBass) {
        if(played_note > 60U) {
            played_note -= 24U;
        } else if(played_note > 48U) {
            played_note -= 12U;
        }
    }

    return midi_synth_clamp_frequency(midi_synth_note_frequency(played_note));
}

static float midi_synth_output_volume(MidiSynth* synth) {
    const float velocity_scale = (float)synth->velocity / 127.0f;
    return midi_synth_clamp_volume(
        synth->volume * velocity_scale * midi_synth_style_level(synth->style));
}

void midi_synth_init(MidiSynth* synth) {
    if(!synth) {
        return;
    }
    synth->acquired = false;
    synth->active = false;
    synth->volume = 0.85f;
    synth->voice_start_ms = 0U;
    synth->note = 0U;
    synth->velocity = 0U;
    synth->output_volume = 0.0f;
    synth->release_pending = false;
    synth->release_time_ms = 0U;
    synth->style = MidiSynthStylePiano;
}

bool midi_synth_acquire(MidiSynth* synth) {
    if(!synth) {
        return false;
    }
    if(synth->acquired) {
        return true;
    }

    synth->acquired = furi_hal_speaker_acquire(1000U);
    return synth->acquired;
}

void midi_synth_note_on(MidiSynth* synth, uint8_t note, uint8_t velocity, MidiSynthStyle style) {
    if(!synth || !synth->acquired || velocity == 0U) {
        return;
    }

    const bool changed = !synth->active || synth->release_pending || synth->note != note ||
                         synth->velocity != velocity || synth->style != style;

    if(changed) {
        synth->active = true;
        synth->note = note;
        synth->velocity = velocity;
        synth->style = style;
        synth->voice_start_ms = furi_get_tick();
        synth->release_pending = false;
        synth->output_volume = midi_synth_output_volume(synth);
        furi_hal_speaker_start(midi_synth_style_frequency(style, note), synth->output_volume);
    } else {
        synth->release_pending = false;
        synth->output_volume = midi_synth_output_volume(synth);
        furi_hal_speaker_set_volume(synth->output_volume);
    }
}

void midi_synth_render(MidiSynth* synth) {
    if(synth && synth->acquired && synth->active && synth->style == MidiSynthStyleDrums &&
       (furi_get_tick() - synth->voice_start_ms) > 45U) {
        furi_hal_speaker_stop();
        synth->active = false;
        synth->release_pending = false;
    } else if(synth && synth->acquired && synth->active && synth->release_pending) {
        if((int32_t)(furi_get_tick() - synth->release_time_ms) >= 0) {
            furi_hal_speaker_stop();
            synth->active = false;
            synth->release_pending = false;
        }
    }
}

void midi_synth_set_volume(MidiSynth* synth, float volume) {
    if(!synth) {
        return;
    }

    synth->volume = midi_synth_clamp_volume(volume);
    if(synth->acquired && synth->active) {
        synth->output_volume = midi_synth_output_volume(synth);
        furi_hal_speaker_set_volume(synth->output_volume);
    }
}

void midi_synth_release_note(MidiSynth* synth, uint32_t minimum_duration_ms) {
    if(!synth || !synth->acquired || !synth->active) {
        return;
    }

    const uint32_t now = furi_get_tick();
    const uint32_t age_ms = now - synth->voice_start_ms;
    if(age_ms >= minimum_duration_ms) {
        synth->active = false;
        synth->release_pending = false;
        furi_hal_speaker_stop();
    } else {
        synth->release_pending = true;
        synth->release_time_ms = synth->voice_start_ms + minimum_duration_ms;
    }
}

void midi_synth_stop(MidiSynth* synth) {
    if(synth && synth->acquired) {
        synth->active = false;
        synth->release_pending = false;
        furi_hal_speaker_stop();
    }
}

void midi_synth_release(MidiSynth* synth) {
    if(!synth || !synth->acquired) {
        return;
    }

    furi_hal_speaker_stop();
    furi_hal_speaker_release();
    synth->acquired = false;
    synth->active = false;
    synth->release_pending = false;
}

MidiSynthStyle midi_synth_style_from_program(uint8_t program) {
    if(program <= 7U) {
        return MidiSynthStylePiano;
    }
    if(program <= 15U) {
        return MidiSynthStyleGuitar;
    }
    if(program <= 23U) {
        return MidiSynthStyleOrgan;
    }
    if(program <= 31U) {
        return MidiSynthStyleGuitar;
    }
    if(program <= 39U) {
        return MidiSynthStyleBass;
    }
    if(program <= 55U) {
        return MidiSynthStyleStrings;
    }
    if(program <= 63U) {
        return MidiSynthStyleSynth;
    }
    if(program <= 79U) {
        return MidiSynthStyleFlute;
    }
    if(program <= 103U) {
        return MidiSynthStyleSynth;
    }
    return MidiSynthStyleGuitar;
}
