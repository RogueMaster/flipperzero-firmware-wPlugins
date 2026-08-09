#include "dcf77_gpio.h"

#include <string.h>

Dcf77PulseKind dcf77_classify_pulse_ms(uint32_t pulse_ms) {
    /* Windows with margin around 100ms / 200ms DCF77 marks. */
    if(pulse_ms >= 60 && pulse_ms <= 140) {
        return Dcf77PulseZero;
    }
    if(pulse_ms >= 160 && pulse_ms <= 260) {
        return Dcf77PulseOne;
    }
    return Dcf77PulseNoise;
}

Dcf77PulseKind dcf77_classify_gap_ms(uint32_t gap_ms) {
    if(gap_ms >= 1500) {
        return Dcf77PulseMinuteMark;
    }
    return Dcf77PulseNoise;
}

void dcf77_bit_buffer_reset(Dcf77BitBuffer* buf) {
    if(!buf) {
        return;
    }
    memset(buf, 0, sizeof(*buf));
}

bool dcf77_bit_buffer_feed(Dcf77BitBuffer* buf, Dcf77PulseKind kind) {
    if(!buf) {
        return false;
    }
    buf->complete = false;

    if(kind == Dcf77PulseNoise) {
        return false;
    }

    if(kind == Dcf77PulseMinuteMark) {
        if(buf->count == DCF77_FRAME_BITS) {
            buf->complete = true;
            return true;
        }
        dcf77_bit_buffer_reset(buf);
        return false;
    }

    if(buf->count >= DCF77_FRAME_BITS) {
        dcf77_bit_buffer_reset(buf);
    }
    buf->bits[buf->count] = (kind == Dcf77PulseOne) ? 1 : 0;
    buf->count++;
    return false;
}
