#include "radio_clock_protocol.h"

#include <string.h>

#include "bsf_util.h"
#include "bpc_util.h"
#include "dcf77_util.h"
#include "hbg_util.h"
#include "jjy_util.h"
#include "msf_util.h"
#include "wwvb_util.h"

#define RADIO_CLOCK_LPTIM_HZ               32768U
#define RADIO_CLOCK_FULL_SECOND_TICKS      RADIO_CLOCK_LPTIM_HZ
#define RADIO_CLOCK_DCF77_ZERO_HIGH_TICKS  ((RADIO_CLOCK_LPTIM_HZ * 9U) / 10U)
#define RADIO_CLOCK_DCF77_ONE_HIGH_TICKS   ((RADIO_CLOCK_LPTIM_HZ * 8U) / 10U)
#define RADIO_CLOCK_WWVB_ZERO_HIGH_TICKS   ((RADIO_CLOCK_LPTIM_HZ * 8U) / 10U)
#define RADIO_CLOCK_WWVB_ONE_HIGH_TICKS    ((RADIO_CLOCK_LPTIM_HZ * 5U) / 10U)
#define RADIO_CLOCK_WWVB_MARKER_HIGH_TICKS ((RADIO_CLOCK_LPTIM_HZ * 2U) / 10U)
#define RADIO_CLOCK_JJY_ZERO_HIGH_TICKS    ((RADIO_CLOCK_LPTIM_HZ * 8U) / 10U)
#define RADIO_CLOCK_JJY_ONE_HIGH_TICKS     ((RADIO_CLOCK_LPTIM_HZ * 5U) / 10U)
#define RADIO_CLOCK_JJY_MARKER_HIGH_TICKS  ((RADIO_CLOCK_LPTIM_HZ * 2U) / 10U)
#define RADIO_CLOCK_DCF77_ZERO_LOW_TICKS \
    (RADIO_CLOCK_FULL_SECOND_TICKS - RADIO_CLOCK_DCF77_ZERO_HIGH_TICKS)
#define RADIO_CLOCK_DCF77_ONE_LOW_TICKS \
    (RADIO_CLOCK_FULL_SECOND_TICKS - RADIO_CLOCK_DCF77_ONE_HIGH_TICKS)
#define RADIO_CLOCK_MSF_MARKER_LOW_TICKS ((RADIO_CLOCK_LPTIM_HZ * 5U) / 10U)
#define RADIO_CLOCK_MSF_SLOT_TICKS       ((RADIO_CLOCK_LPTIM_HZ + 5U) / 10U)
#define RADIO_CLOCK_BPC_00_LOW_TICKS     ((RADIO_CLOCK_LPTIM_HZ + 5U) / 10U)
#define RADIO_CLOCK_BPC_01_LOW_TICKS     (((RADIO_CLOCK_LPTIM_HZ * 2U) + 5U) / 10U)
#define RADIO_CLOCK_BPC_10_LOW_TICKS     (((RADIO_CLOCK_LPTIM_HZ * 3U) + 5U) / 10U)
#define RADIO_CLOCK_BPC_11_LOW_TICKS     (((RADIO_CLOCK_LPTIM_HZ * 4U) + 5U) / 10U)
#define RADIO_CLOCK_BSF_00_LOW_TICKS     (((RADIO_CLOCK_LPTIM_HZ * 2U) + 5U) / 10U)
#define RADIO_CLOCK_BSF_01_LOW_TICKS     (((RADIO_CLOCK_LPTIM_HZ * 4U) + 5U) / 10U)
#define RADIO_CLOCK_BSF_11_LOW_TICKS     (((RADIO_CLOCK_LPTIM_HZ * 6U) + 5U) / 10U)
#define RADIO_CLOCK_BSF_10_LOW_TICKS     (((RADIO_CLOCK_LPTIM_HZ * 8U) + 5U) / 10U)
#define RADIO_CLOCK_WWVB_ZERO_LOW_TICKS \
    (RADIO_CLOCK_FULL_SECOND_TICKS - RADIO_CLOCK_WWVB_ZERO_HIGH_TICKS)
#define RADIO_CLOCK_WWVB_ONE_LOW_TICKS \
    (RADIO_CLOCK_FULL_SECOND_TICKS - RADIO_CLOCK_WWVB_ONE_HIGH_TICKS)
#define RADIO_CLOCK_WWVB_MARKER_LOW_TICKS \
    (RADIO_CLOCK_FULL_SECOND_TICKS - RADIO_CLOCK_WWVB_MARKER_HIGH_TICKS)

static void radio_clock_waveform_clear(RadioClockMinuteFrame* frame, uint8_t second) {
    frame->waveforms[second].segment_count = 0U;
}

static void radio_clock_waveform_add_segment(
    RadioClockMinuteFrame* frame,
    uint8_t second,
    bool level,
    uint16_t ticks) {
    const uint8_t index = frame->waveforms[second].segment_count;

    if(index >= RADIO_CLOCK_MAX_SECOND_SEGMENTS || ticks == 0U) {
        return;
    }

    if(index > 0U && frame->waveforms[second].segments[index - 1U].level == level) {
        frame->waveforms[second].segments[index - 1U].ticks =
            (uint16_t)(frame->waveforms[second].segments[index - 1U].ticks + ticks);
        return;
    }

    frame->waveforms[second].segments[index].level = level;
    frame->waveforms[second].segments[index].ticks = ticks;
    frame->waveforms[second].segment_count++;
}

static void radio_clock_waveform_set_ook_symbol(
    RadioClockMinuteFrame* frame,
    uint8_t second,
    RadioClockPulse pulse,
    uint16_t low_ticks) {
    radio_clock_waveform_clear(frame, second);
    frame->pulses[second] = pulse;

    if(low_ticks == 0U) {
        radio_clock_waveform_add_segment(frame, second, true, RADIO_CLOCK_FULL_SECOND_TICKS);
        return;
    }

    if(low_ticks >= RADIO_CLOCK_FULL_SECOND_TICKS) {
        radio_clock_waveform_add_segment(frame, second, false, RADIO_CLOCK_FULL_SECOND_TICKS);
        return;
    }

    radio_clock_waveform_add_segment(frame, second, false, low_ticks);
    radio_clock_waveform_add_segment(
        frame, second, true, (uint16_t)(RADIO_CLOCK_FULL_SECOND_TICKS - low_ticks));
}

static void radio_clock_waveform_set_positive_symbol(
    RadioClockMinuteFrame* frame,
    uint8_t second,
    RadioClockPulse pulse,
    uint16_t high_ticks) {
    radio_clock_waveform_clear(frame, second);
    frame->pulses[second] = pulse;

    if(high_ticks == 0U) {
        radio_clock_waveform_add_segment(frame, second, false, RADIO_CLOCK_FULL_SECOND_TICKS);
        return;
    }

    if(high_ticks >= RADIO_CLOCK_FULL_SECOND_TICKS) {
        radio_clock_waveform_add_segment(frame, second, true, RADIO_CLOCK_FULL_SECOND_TICKS);
        return;
    }

    radio_clock_waveform_add_segment(frame, second, true, high_ticks);
    radio_clock_waveform_add_segment(
        frame, second, false, (uint16_t)(RADIO_CLOCK_FULL_SECOND_TICKS - high_ticks));
}

static uint16_t radio_clock_waveform_pair_low_ticks(
    RadioClockPulse pulse,
    uint16_t pair00_ticks,
    uint16_t pair01_ticks,
    uint16_t pair10_ticks,
    uint16_t pair11_ticks) {
    switch(pulse) {
    case RadioClockPulsePair01:
        return pair01_ticks;
    case RadioClockPulsePair10:
        return pair10_ticks;
    case RadioClockPulsePair11:
        return pair11_ticks;
    case RadioClockPulsePair00:
    default:
        return pair00_ticks;
    }
}

static void radio_clock_waveform_set_pair_symbol(
    RadioClockMinuteFrame* frame,
    uint8_t second,
    RadioClockPulse pulse,
    uint16_t pair00_ticks,
    uint16_t pair01_ticks,
    uint16_t pair10_ticks,
    uint16_t pair11_ticks) {
    if(pulse == RadioClockPulseMarker) {
        radio_clock_waveform_set_ook_symbol(frame, second, RadioClockPulseMarker, 0U);
        return;
    }

    radio_clock_waveform_set_ook_symbol(
        frame,
        second,
        pulse,
        radio_clock_waveform_pair_low_ticks(
            pulse, pair00_ticks, pair01_ticks, pair10_ticks, pair11_ticks));
}

static RadioClockPulse radio_clock_msf_symbol(bool bit_a, bool bit_b) {
    if(bit_a) {
        return bit_b ? RadioClockPulseMsfA1B1 : RadioClockPulseMsfA1B0;
    }

    return bit_b ? RadioClockPulseMsfA0B1 : RadioClockPulseMsfA0B0;
}

static void radio_clock_waveform_set_msf_symbol(
    RadioClockMinuteFrame* frame,
    uint8_t second,
    bool bit_a,
    bool bit_b) {
    const uint16_t trailing_ticks =
        (uint16_t)(RADIO_CLOCK_FULL_SECOND_TICKS - (RADIO_CLOCK_MSF_SLOT_TICKS * 3U));

    radio_clock_waveform_clear(frame, second);
    frame->pulses[second] = radio_clock_msf_symbol(bit_a, bit_b);
    radio_clock_waveform_add_segment(frame, second, false, RADIO_CLOCK_MSF_SLOT_TICKS);
    radio_clock_waveform_add_segment(frame, second, !bit_a, RADIO_CLOCK_MSF_SLOT_TICKS);
    radio_clock_waveform_add_segment(frame, second, !bit_b, RADIO_CLOCK_MSF_SLOT_TICKS);
    radio_clock_waveform_add_segment(frame, second, true, trailing_ticks);
}

static uint8_t radio_clock_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t result = days[month - 1U];

    if(month == 2U && wwvb_is_leap_year(year)) {
        result = 29U;
    }

    return result;
}

static uint8_t radio_clock_last_sunday(uint16_t year, uint8_t month) {
    uint8_t day = radio_clock_days_in_month(year, month);
    static const uint8_t month_offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t adjusted_year = year;

    if(month < 3U) {
        adjusted_year--;
    }

    const uint8_t weekday =
        (uint8_t)((adjusted_year + (adjusted_year / 4U) - (adjusted_year / 100U) +
                   (adjusted_year / 400U) + month_offsets[month - 1U] + day) %
                  7U);
    return (uint8_t)(day - weekday);
}

static bool dcf77_is_dst(const RadioClockProtocolTime* time) {
    if(time->month < 3U || time->month > 10U) {
        return false;
    }

    if(time->month > 3U && time->month < 10U) {
        return true;
    }

    const uint8_t switch_day = radio_clock_last_sunday(time->year, time->month);

    if(time->month == 3U) {
        if(time->day > switch_day) {
            return true;
        }
        if(time->day < switch_day) {
            return false;
        }
        return time->hour >= 3U;
    }

    if(time->day < switch_day) {
        return true;
    }
    if(time->day > switch_day) {
        return false;
    }
    return time->hour < 4U;
}

static void dcf77_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    memset(frame, 0, sizeof(*frame));

    set_dcf_message(
        frame->encoded,
        time->minute,
        time->hour,
        time->day,
        time->month,
        (uint8_t)(time->year % 100U),
        time->weekday,
        dcf77_is_dst(time),
        false,
        false,
        false,
        0x0000);

    for(uint8_t second = 0; second < RADIO_CLOCK_FRAME_SECONDS; second++) {
        if(second == 59U) {
            radio_clock_waveform_set_ook_symbol(frame, second, RadioClockPulseMarker, 0U);
        } else {
            radio_clock_waveform_set_ook_symbol(
                frame,
                second,
                dcf77_message_get_bit(frame->encoded, second) ? RadioClockPulseOne :
                                                                RadioClockPulseZero,
                dcf77_message_get_bit(frame->encoded, second) ? RADIO_CLOCK_DCF77_ONE_LOW_TICKS :
                                                                RADIO_CLOCK_DCF77_ZERO_LOW_TICKS);
        }
    }
}

static void hbg_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    dcf77_prepare_frame(frame, time);
    hbg_build_start_waveform(
        &frame->waveforms[0], hbg_get_start_pattern(time->hour, time->minute));
}

static void wwvb_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    memset(frame, 0, sizeof(*frame));

    set_wwvb_am_symbols(
        frame->pulses,
        time->minute,
        time->hour,
        time->day_of_year,
        (uint8_t)(time->year % 100U),
        false,
        false,
        wwvb_is_leap_year(time->year),
        false,
        0);

    for(uint8_t second = 0; second < RADIO_CLOCK_FRAME_SECONDS; second++) {
        switch(frame->pulses[second]) {
        case RadioClockPulseOne:
            radio_clock_waveform_set_ook_symbol(
                frame, second, RadioClockPulseOne, RADIO_CLOCK_WWVB_ONE_LOW_TICKS);
            break;
        case RadioClockPulseMarker:
            radio_clock_waveform_set_ook_symbol(
                frame, second, RadioClockPulseMarker, RADIO_CLOCK_WWVB_MARKER_LOW_TICKS);
            break;
        case RadioClockPulseZero:
        default:
            radio_clock_waveform_set_ook_symbol(
                frame, second, RadioClockPulseZero, RADIO_CLOCK_WWVB_ZERO_LOW_TICKS);
            break;
        }
    }
}

static void msf_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    bool bits_a[MSF_FRAME_SECONDS];
    bool bits_b[MSF_FRAME_SECONDS];

    memset(frame, 0, sizeof(*frame));
    set_msf_timecode(
        bits_a,
        bits_b,
        time->minute,
        time->hour,
        time->day,
        time->month,
        (uint8_t)(time->year % 100U),
        (uint8_t)(time->weekday % 7U),
        false,
        false,
        0);

    radio_clock_waveform_set_ook_symbol(
        frame, 0U, RadioClockPulseMarker, RADIO_CLOCK_MSF_MARKER_LOW_TICKS);

    for(uint8_t second = 1U; second < RADIO_CLOCK_FRAME_SECONDS; second++) {
        radio_clock_waveform_set_msf_symbol(frame, second, bits_a[second], bits_b[second]);
    }
}

static void jjy_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    memset(frame, 0, sizeof(*frame));

    set_jjy_timecode(
        frame->pulses,
        time->minute,
        time->hour,
        time->day_of_year,
        (uint8_t)(time->year % 100U),
        (uint8_t)(time->weekday % 7U),
        false,
        false,
        false,
        false);

    for(uint8_t second = 0; second < RADIO_CLOCK_FRAME_SECONDS; second++) {
        switch(frame->pulses[second]) {
        case RadioClockPulseOne:
            radio_clock_waveform_set_positive_symbol(
                frame, second, RadioClockPulseOne, RADIO_CLOCK_JJY_ONE_HIGH_TICKS);
            break;
        case RadioClockPulseMarker:
            radio_clock_waveform_set_positive_symbol(
                frame, second, RadioClockPulseMarker, RADIO_CLOCK_JJY_MARKER_HIGH_TICKS);
            break;
        case RadioClockPulseZero:
        default:
            radio_clock_waveform_set_positive_symbol(
                frame, second, RadioClockPulseZero, RADIO_CLOCK_JJY_ZERO_HIGH_TICKS);
            break;
        }
    }
}

static void bpc_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    memset(frame, 0, sizeof(*frame));

    set_bpc_timecode(
        frame->pulses,
        time->minute,
        time->hour,
        time->day,
        time->month,
        (uint8_t)(time->year % 100U),
        time->weekday);

    for(uint8_t second = 0; second < RADIO_CLOCK_FRAME_SECONDS; second++) {
        radio_clock_waveform_set_pair_symbol(
            frame,
            second,
            frame->pulses[second],
            RADIO_CLOCK_BPC_00_LOW_TICKS,
            RADIO_CLOCK_BPC_01_LOW_TICKS,
            RADIO_CLOCK_BPC_10_LOW_TICKS,
            RADIO_CLOCK_BPC_11_LOW_TICKS);
    }
}

static void bsf_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    memset(frame, 0, sizeof(*frame));

    set_bsf_timecode(
        frame->pulses,
        time->minute,
        time->hour,
        time->day,
        time->month,
        (uint8_t)(time->year % 100U),
        (uint8_t)(time->weekday % 7U),
        false,
        false);

    for(uint8_t second = 0; second < RADIO_CLOCK_FRAME_SECONDS; second++) {
        radio_clock_waveform_set_pair_symbol(
            frame,
            second,
            frame->pulses[second],
            RADIO_CLOCK_BSF_00_LOW_TICKS,
            RADIO_CLOCK_BSF_01_LOW_TICKS,
            RADIO_CLOCK_BSF_10_LOW_TICKS,
            RADIO_CLOCK_BSF_11_LOW_TICKS);
    }
}

static const RadioClockProtocolOps radio_clock_protocol_dcf77 = {
    .prepare_frame = dcf77_prepare_frame,
};

static const RadioClockProtocolOps radio_clock_protocol_wwvb = {
    .prepare_frame = wwvb_prepare_frame,
};

static const RadioClockProtocolOps radio_clock_protocol_msf = {
    .prepare_frame = msf_prepare_frame,
};

static const RadioClockProtocolOps radio_clock_protocol_bpc = {
    .prepare_frame = bpc_prepare_frame,
};

static const RadioClockProtocolOps radio_clock_protocol_bsf = {
    .prepare_frame = bsf_prepare_frame,
};

static const RadioClockProtocolOps radio_clock_protocol_jjy = {
    .prepare_frame = jjy_prepare_frame,
};

static const RadioClockProtocolOps radio_clock_protocol_hbg = {
    .prepare_frame = hbg_prepare_frame,
};

const RadioClockProtocolOps* radio_clock_protocol_get(RadioClockSignal signal) {
    switch(signal) {
    case RadioClockSignalHbg:
        return &radio_clock_protocol_hbg;
    case RadioClockSignalBsf:
        return &radio_clock_protocol_bsf;
    case RadioClockSignalBpc:
        return &radio_clock_protocol_bpc;
    case RadioClockSignalJjy:
        return &radio_clock_protocol_jjy;
    case RadioClockSignalMsf:
        return &radio_clock_protocol_msf;
    case RadioClockSignalWwvb:
        return &radio_clock_protocol_wwvb;
    case RadioClockSignalDcf77:
    default:
        return &radio_clock_protocol_dcf77;
    }
}

bool radio_clock_protocol_uses_following_minute(RadioClockSignal signal) {
    switch(signal) {
    case RadioClockSignalDcf77:
    case RadioClockSignalMsf:
    case RadioClockSignalHbg:
        return true;
    case RadioClockSignalWwvb:
    case RadioClockSignalJjy:
    case RadioClockSignalBpc:
    case RadioClockSignalBsf:
    default:
        return false;
    }
}

const char* radio_clock_protocol_pulse_label(RadioClockPulse pulse) {
    switch(pulse) {
    case RadioClockPulseMsfA0B0:
        return "A0B0";
    case RadioClockPulseMsfA0B1:
        return "A0B1";
    case RadioClockPulseMsfA1B0:
        return "A1B0";
    case RadioClockPulseMsfA1B1:
        return "A1B1";
    case RadioClockPulsePair00:
        return "00";
    case RadioClockPulsePair01:
        return "01";
    case RadioClockPulsePair10:
        return "10";
    case RadioClockPulsePair11:
        return "11";
    case RadioClockPulseOne:
        return "1";
    case RadioClockPulseMarker:
        return "M";
    case RadioClockPulseZero:
    default:
        return "0";
    }
}

uint16_t radio_clock_protocol_day_of_year(uint16_t year, uint8_t month, uint8_t day) {
    return wwvb_day_of_year(year, month, day);
}

void radio_clock_protocol_prepare_frame(
    RadioClockSignal signal,
    RadioClockMinuteFrame* frame,
    const RadioClockProtocolTime* time) {
    radio_clock_protocol_get(signal)->prepare_frame(frame, time);
}
