/*
 * Purpose: Run the Morse keyer state machine for paddles and straight input.
 * Owns: keyer timing, queueing, relay pulses, and event emission.
 * Depends on: keyer.h and host-safe string routines.
 * Tests: tests/test_keyer.c and tests/test_vail_modes.c.
 */

#include "keyer.h"

#include <string.h>

static uint16_t morse_keyer_sane_dit_ms(uint16_t dit_duration_ms) {
    return dit_duration_ms == 0U ? 1U : dit_duration_ms;
}

static void morse_keyer_clear_runtime(MorseKeyer* keyer) {
    memset(keyer->paddle_down, 0, sizeof(keyer->paddle_down));
    memset(keyer->relay_closed, 0, sizeof(keyer->relay_closed));
    keyer->pulse_active = false;
    keyer->next_pulse_at = 0U;
    keyer->next_repeat_paddle = -1;
    keyer->current_pulse_paddle = -1;
    keyer->current_output_paddle = -1;
    keyer->set_queue_len = 0U;
    keyer->fifo_queue_len = 0U;
}

static void morse_keyer_push_event(MorseKeyer* keyer, uint8_t type, uint8_t paddle) {
    if(keyer->event_count >= MORSE_KEYER_EVENT_CAPACITY) {
        return;
    }

    uint8_t tail =
        (uint8_t)((keyer->event_head + keyer->event_count) % MORSE_KEYER_EVENT_CAPACITY);
    keyer->events[tail].type = type;
    keyer->events[tail].paddle = paddle;
    keyer->event_count++;
}

static void morse_keyer_set_output(MorseKeyer* keyer, uint8_t paddle, bool active) {
    if(keyer->output_active[paddle] == active) {
        return;
    }

    keyer->output_active[paddle] = active;
    morse_keyer_push_event(keyer, active ? MorseKeyerEventPress : MorseKeyerEventRelease, paddle);
}

static void morse_keyer_release_outputs(MorseKeyer* keyer) {
    for(size_t paddle = 0; paddle < MorseKeyerPaddleCount; paddle++) {
        morse_keyer_set_output(keyer, (uint8_t)paddle, false);
    }
}

static bool morse_keyer_any_relay_closed(const MorseKeyer* keyer) {
    return keyer->relay_closed[MorseKeyerPaddleDit] || keyer->relay_closed[MorseKeyerPaddleDah];
}

static void morse_keyer_set_relay(MorseKeyer* keyer, uint8_t paddle, bool closed) {
    bool was_closed = morse_keyer_any_relay_closed(keyer);
    keyer->relay_closed[paddle] = closed;
    bool now_closed = morse_keyer_any_relay_closed(keyer);

    if(was_closed == now_closed) {
        return;
    }

    if(now_closed) {
        keyer->current_output_paddle = (int8_t)paddle;
        morse_keyer_set_output(keyer, paddle, true);
    } else {
        if(keyer->current_output_paddle >= 0) {
            morse_keyer_set_output(keyer, (uint8_t)keyer->current_output_paddle, false);
        }
        keyer->current_output_paddle = -1;
    }
}

static void morse_keyer_begin_pulse(MorseKeyer* keyer, uint32_t now_ms) {
    if(!keyer->pulse_active) {
        keyer->pulse_active = true;
        keyer->next_pulse_at = now_ms;
    }
}

static void morse_keyer_stop_pulse(MorseKeyer* keyer) {
    keyer->pulse_active = false;
    keyer->next_pulse_at = 0U;
}

static void morse_keyer_set_queue_add(MorseKeyer* keyer, uint8_t paddle) {
    if(keyer->set_queue_len >= MORSE_KEYER_QUEUE_CAPACITY) {
        return;
    }

    for(uint8_t i = 0; i < keyer->set_queue_len; i++) {
        if(keyer->set_queue[i] == paddle) {
            return;
        }
    }

    keyer->set_queue[keyer->set_queue_len++] = paddle;
}

static int8_t morse_keyer_set_queue_shift(MorseKeyer* keyer) {
    if(keyer->set_queue_len == 0U) {
        return -1;
    }

    uint8_t paddle = keyer->set_queue[0];
    keyer->set_queue_len--;

    for(uint8_t i = 0; i < keyer->set_queue_len; i++) {
        keyer->set_queue[i] = keyer->set_queue[i + 1U];
    }

    return (int8_t)paddle;
}

static void morse_keyer_fifo_push(MorseKeyer* keyer, uint8_t paddle) {
    if(keyer->fifo_queue_len >= MORSE_KEYER_QUEUE_CAPACITY) {
        return;
    }

    keyer->fifo_queue[keyer->fifo_queue_len++] = paddle;
}

static int8_t morse_keyer_fifo_shift(MorseKeyer* keyer) {
    if(keyer->fifo_queue_len == 0U) {
        return -1;
    }

    uint8_t paddle = keyer->fifo_queue[0];
    keyer->fifo_queue_len--;

    for(uint8_t i = 0; i < keyer->fifo_queue_len; i++) {
        keyer->fifo_queue[i] = keyer->fifo_queue[i + 1U];
    }

    return (int8_t)paddle;
}

static int8_t morse_keyer_first_down_paddle(const MorseKeyer* keyer) {
    for(uint8_t paddle = 0; paddle < MorseKeyerPaddleCount; paddle++) {
        if(keyer->paddle_down[paddle]) {
            return (int8_t)paddle;
        }
    }

    return -1;
}

static int8_t morse_keyer_opposite_paddle(int8_t paddle) {
    if(paddle == MorseKeyerPaddleDit) {
        return MorseKeyerPaddleDah;
    }

    if(paddle == MorseKeyerPaddleDah) {
        return MorseKeyerPaddleDit;
    }

    return -1;
}

static uint32_t morse_keyer_element_duration(const MorseKeyer* keyer, uint8_t paddle) {
    if(paddle == MorseKeyerPaddleDah) {
        return (uint32_t)keyer->dit_duration_ms * 3U;
    }

    return keyer->dit_duration_ms;
}

static int8_t morse_keyer_next_elbug(const MorseKeyer* keyer) {
    if(morse_keyer_first_down_paddle(keyer) < 0) {
        return -1;
    }

    return keyer->next_repeat_paddle;
}

static int8_t morse_keyer_next_single_dot(MorseKeyer* keyer) {
    int8_t queued = morse_keyer_set_queue_shift(keyer);
    if(queued >= 0) {
        return queued;
    }

    if(keyer->paddle_down[MorseKeyerPaddleDah]) {
        return MorseKeyerPaddleDah;
    }

    if(keyer->paddle_down[MorseKeyerPaddleDit]) {
        return MorseKeyerPaddleDit;
    }

    return -1;
}

static int8_t morse_keyer_next_ultimatic(MorseKeyer* keyer) {
    int8_t queued = morse_keyer_set_queue_shift(keyer);
    if(queued >= 0) {
        return queued;
    }

    return morse_keyer_next_elbug(keyer);
}

static int8_t morse_keyer_next_plain_iambic(MorseKeyer* keyer) {
    int8_t next = morse_keyer_next_elbug(keyer);

    if(next < 0) {
        next = morse_keyer_first_down_paddle(keyer);
    }

    if(next >= 0 && keyer->paddle_down[MorseKeyerPaddleDit] &&
       keyer->paddle_down[MorseKeyerPaddleDah]) {
        keyer->next_repeat_paddle = morse_keyer_opposite_paddle(next);
    }

    return next;
}

static int8_t morse_keyer_next_iambic_a(MorseKeyer* keyer) {
    int8_t next = morse_keyer_next_plain_iambic(keyer);
    int8_t queued = morse_keyer_set_queue_shift(keyer);

    if(queued >= 0) {
        return queued;
    }

    return next;
}

static int8_t morse_keyer_next_iambic_b(MorseKeyer* keyer) {
    for(uint8_t paddle = 0; paddle < MorseKeyerPaddleCount; paddle++) {
        if(keyer->paddle_down[paddle]) {
            morse_keyer_set_queue_add(keyer, paddle);
        }
    }

    return morse_keyer_set_queue_shift(keyer);
}

static int8_t morse_keyer_next_keyahead(MorseKeyer* keyer) {
    int8_t queued = morse_keyer_fifo_shift(keyer);
    if(queued >= 0) {
        return queued;
    }

    return morse_keyer_next_elbug(keyer);
}

static int8_t morse_keyer_next_pulse_for_mode(MorseKeyer* keyer) {
    switch(keyer->mode) {
    case MorseKeyerModeElBug:
        return morse_keyer_next_elbug(keyer);
    case MorseKeyerModeSingleDot:
        return morse_keyer_next_single_dot(keyer);
    case MorseKeyerModeUltimatic:
        return morse_keyer_next_ultimatic(keyer);
    case MorseKeyerModePlainIambic:
        return morse_keyer_next_plain_iambic(keyer);
    case MorseKeyerModeIambicA:
        return morse_keyer_next_iambic_a(keyer);
    case MorseKeyerModeIambicB:
        return morse_keyer_next_iambic_b(keyer);
    case MorseKeyerModeKeyahead:
        return morse_keyer_next_keyahead(keyer);
    default:
        return -1;
    }
}

static void morse_keyer_pulse_bug(MorseKeyer* keyer, uint32_t now_ms) {
    if(keyer->relay_closed[MorseKeyerPaddleDit]) {
        morse_keyer_set_relay(keyer, MorseKeyerPaddleDit, false);
        keyer->next_pulse_at = now_ms + keyer->dit_duration_ms;
    } else if(keyer->paddle_down[MorseKeyerPaddleDit]) {
        morse_keyer_set_relay(keyer, MorseKeyerPaddleDit, true);
        keyer->next_pulse_at = now_ms + keyer->dit_duration_ms;
    } else {
        morse_keyer_stop_pulse(keyer);
    }
}

static void morse_keyer_pulse_family(MorseKeyer* keyer, uint32_t now_ms) {
    if(keyer->current_pulse_paddle >= 0) {
        morse_keyer_set_relay(keyer, (uint8_t)keyer->current_pulse_paddle, false);
        keyer->current_pulse_paddle = -1;
        keyer->next_pulse_at = now_ms + keyer->dit_duration_ms;
        return;
    }

    int8_t next = morse_keyer_next_pulse_for_mode(keyer);
    if(next < 0 || next >= MorseKeyerPaddleCount) {
        keyer->next_repeat_paddle = morse_keyer_first_down_paddle(keyer);
        morse_keyer_stop_pulse(keyer);
        return;
    }

    keyer->current_pulse_paddle = next;
    morse_keyer_set_relay(keyer, (uint8_t)next, true);
    keyer->next_pulse_at = now_ms + morse_keyer_element_duration(keyer, (uint8_t)next);
}

void morse_keyer_init(MorseKeyer* keyer, uint8_t mode, uint16_t dit_duration_ms) {
    if(keyer == NULL) {
        return;
    }

    memset(keyer, 0, sizeof(*keyer));
    keyer->current_pulse_paddle = -1;
    keyer->current_output_paddle = -1;
    keyer->next_repeat_paddle = -1;
    keyer->mode = morse_keyer_mode_valid(mode) ? mode : MorseKeyerModePassthrough;
    keyer->dit_duration_ms = morse_keyer_sane_dit_ms(dit_duration_ms);
}

void morse_keyer_reset(MorseKeyer* keyer) {
    if(keyer == NULL) {
        return;
    }

    morse_keyer_release_outputs(keyer);
    morse_keyer_clear_runtime(keyer);
}

bool morse_keyer_mode_valid(uint8_t mode) {
    return mode <= MorseKeyerModeKeyahead;
}

void morse_keyer_set_mode(MorseKeyer* keyer, uint8_t mode) {
    if(keyer == NULL) {
        return;
    }

    if(!morse_keyer_mode_valid(mode)) {
        mode = MorseKeyerModePassthrough;
    }

    if(keyer->mode == mode) {
        return;
    }

    morse_keyer_release_outputs(keyer);
    morse_keyer_clear_runtime(keyer);
    keyer->mode = mode;
}

uint8_t morse_keyer_get_mode(const MorseKeyer* keyer) {
    if(keyer == NULL) {
        return MorseKeyerModePassthrough;
    }

    return keyer->mode;
}

void morse_keyer_set_dit_duration(MorseKeyer* keyer, uint16_t dit_duration_ms) {
    if(keyer == NULL) {
        return;
    }

    keyer->dit_duration_ms = morse_keyer_sane_dit_ms(dit_duration_ms);
}

uint16_t morse_keyer_get_dit_duration(const MorseKeyer* keyer) {
    if(keyer == NULL) {
        return 1U;
    }

    return keyer->dit_duration_ms;
}

void morse_keyer_paddle_event(MorseKeyer* keyer, uint8_t paddle, bool pressed, uint32_t now_ms) {
    if(keyer == NULL || paddle >= MorseKeyerPaddleCount) {
        return;
    }

    keyer->paddle_down[paddle] = pressed;

    switch(keyer->mode) {
    case MorseKeyerModePassthrough:
        morse_keyer_set_output(keyer, paddle, pressed);
        break;
    case MorseKeyerModeStraight:
        morse_keyer_set_relay(keyer, paddle, pressed);
        break;
    case MorseKeyerModeBug:
        if(paddle == MorseKeyerPaddleDit) {
            morse_keyer_begin_pulse(keyer, now_ms);
        } else {
            morse_keyer_set_relay(keyer, paddle, pressed);
        }
        break;
    case MorseKeyerModeElBug:
    case MorseKeyerModePlainIambic:
        if(pressed) {
            keyer->next_repeat_paddle = (int8_t)paddle;
            morse_keyer_begin_pulse(keyer, now_ms);
        } else {
            keyer->next_repeat_paddle = morse_keyer_first_down_paddle(keyer);
        }
        break;
    case MorseKeyerModeSingleDot:
        if(pressed && paddle == MorseKeyerPaddleDit) {
            morse_keyer_set_queue_add(keyer, paddle);
        }
        if(pressed) {
            keyer->next_repeat_paddle = (int8_t)paddle;
            morse_keyer_begin_pulse(keyer, now_ms);
        } else {
            keyer->next_repeat_paddle = morse_keyer_first_down_paddle(keyer);
        }
        break;
    case MorseKeyerModeUltimatic:
        if(pressed) {
            morse_keyer_set_queue_add(keyer, paddle);
            keyer->next_repeat_paddle = (int8_t)paddle;
            morse_keyer_begin_pulse(keyer, now_ms);
        } else {
            keyer->next_repeat_paddle = morse_keyer_first_down_paddle(keyer);
        }
        break;
    case MorseKeyerModeIambicA:
        if(pressed && paddle == MorseKeyerPaddleDit) {
            morse_keyer_set_queue_add(keyer, paddle);
        }
        if(pressed) {
            keyer->next_repeat_paddle = (int8_t)paddle;
            morse_keyer_begin_pulse(keyer, now_ms);
        } else {
            keyer->next_repeat_paddle = morse_keyer_first_down_paddle(keyer);
        }
        break;
    case MorseKeyerModeIambicB:
        if(pressed) {
            morse_keyer_set_queue_add(keyer, paddle);
            keyer->next_repeat_paddle = (int8_t)paddle;
            morse_keyer_begin_pulse(keyer, now_ms);
        } else {
            keyer->next_repeat_paddle = morse_keyer_first_down_paddle(keyer);
        }
        break;
    case MorseKeyerModeKeyahead:
        if(pressed) {
            morse_keyer_fifo_push(keyer, paddle);
            keyer->next_repeat_paddle = (int8_t)paddle;
            morse_keyer_begin_pulse(keyer, now_ms);
        } else {
            keyer->next_repeat_paddle = morse_keyer_first_down_paddle(keyer);
        }
        break;
    default:
        break;
    }
}

void morse_keyer_tick(MorseKeyer* keyer, uint32_t now_ms) {
    if(keyer == NULL || !keyer->pulse_active || now_ms < keyer->next_pulse_at) {
        return;
    }

    switch(keyer->mode) {
    case MorseKeyerModeBug:
        morse_keyer_pulse_bug(keyer, now_ms);
        break;
    case MorseKeyerModeElBug:
    case MorseKeyerModeSingleDot:
    case MorseKeyerModeUltimatic:
    case MorseKeyerModePlainIambic:
    case MorseKeyerModeIambicA:
    case MorseKeyerModeIambicB:
    case MorseKeyerModeKeyahead:
        morse_keyer_pulse_family(keyer, now_ms);
        break;
    default:
        morse_keyer_stop_pulse(keyer);
        break;
    }
}

bool morse_keyer_pop_event(MorseKeyer* keyer, MorseKeyerEvent* event) {
    if(keyer == NULL || event == NULL || keyer->event_count == 0U) {
        return false;
    }

    *event = keyer->events[keyer->event_head];
    keyer->event_head = (uint8_t)((keyer->event_head + 1U) % MORSE_KEYER_EVENT_CAPACITY);
    keyer->event_count--;
    return true;
}

bool morse_keyer_output_active(const MorseKeyer* keyer, uint8_t paddle) {
    if(keyer == NULL || paddle >= MorseKeyerPaddleCount) {
        return false;
    }

    return keyer->output_active[paddle];
}

const char* morse_keyer_mode_name(uint8_t mode) {
    switch(mode) {
    case MorseKeyerModePassthrough:
        return "passthru";
    case MorseKeyerModeStraight:
        return "straight";
    case MorseKeyerModeBug:
        return "bug";
    case MorseKeyerModeElBug:
        return "elbug";
    case MorseKeyerModeSingleDot:
        return "single dot";
    case MorseKeyerModeUltimatic:
        return "ultimatic";
    case MorseKeyerModePlainIambic:
        return "plain iambic";
    case MorseKeyerModeIambicA:
        return "iambic a";
    case MorseKeyerModeIambicB:
        return "iambic b";
    case MorseKeyerModeKeyahead:
        return "keyahead";
    default:
        return "straight";
    }
}

uint8_t morse_keyer_next_ui_mode(uint8_t mode) {
    switch(mode) {
    case MorseKeyerModeStraight:
        return MorseKeyerModeBug;
    case MorseKeyerModeBug:
        return MorseKeyerModePlainIambic;
    case MorseKeyerModePlainIambic:
        return MorseKeyerModeIambicA;
    case MorseKeyerModeIambicA:
        return MorseKeyerModeIambicB;
    case MorseKeyerModeIambicB:
        return MorseKeyerModeUltimatic;
    case MorseKeyerModeUltimatic:
        return MorseKeyerModeKeyahead;
    default:
        return MorseKeyerModeStraight;
    }
}
