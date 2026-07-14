#include "subghz_settings.h"

#include <stdio.h>

static const uint32_t dcf77_subghz_note_freqs_millihz[] = {
    110000, /* A2 */
    116541, /* Bb2 */
    123471, /* B2 */
    130813, /* C3 */
    138591, /* Db3 */
    146832, /* D3 */
    155563, /* Eb3 */
    164814, /* E3 */
    174614, /* F3 */
    184997, /* Gb3 */
    195998, /* G3 */
    207652, /* Ab3 */
    220000, /* A3 */
    233082, /* Bb3 */
    246942, /* B3 */
    261626, /* C4 */
    277183, /* Db4 */
    293665, /* D4 */
    311127, /* Eb4 */
    329628, /* E4 */
    349228, /* F4 */
    369994, /* Gb4 */
    391995, /* G4 */
    415305, /* Ab4 */
    440000, /* A4 */
    466164, /* Bb4 */
    493883, /* B4 */
    523251, /* C5 */
    554365, /* Db5 */
    587330, /* D5 */
    622254, /* Eb5 */
    659255, /* E5 */
    698456, /* F5 */
};

static const uint16_t dcf77_subghz_note_half_periods_us[] = {
    4545U, 4290U, 4050U, 3822U, 3608U, 3405U, 3214U, 3034U, 2863U, 2703U, 2551U,
    2408U, 2273U, 2145U, 2025U, 1911U, 1804U, 1703U, 1607U, 1517U, 1432U, 1351U,
    1276U, 1204U, 1136U, 1073U, 1012U, 956U,  902U,  851U,  804U,  758U,  716U,
};

static const char* const dcf77_subghz_tx_timeout_text[] = {
    "1s",
    "5s",
    "10s",
    "15s",
    "30s",
    "60s",
    "90s",
    "120s",
    "5min",
    "10min",
    "30min",
};

static const uint32_t dcf77_subghz_tx_timeout_seconds_values[] = {
    1U,
    5U,
    10U,
    15U,
    30U,
    60U,
    90U,
    120U,
    300U,
    600U,
    1800U,
};

size_t dcf77_subghz_note_count(void) {
    return sizeof(dcf77_subghz_note_freqs_millihz) / sizeof(dcf77_subghz_note_freqs_millihz[0]);
}

uint32_t dcf77_subghz_note_freq_millihz(size_t index) {
    if(index >= dcf77_subghz_note_count()) {
        index = 0;
    }

    return dcf77_subghz_note_freqs_millihz[index];
}

float dcf77_subghz_note_freq_hz(size_t index) {
    return (float)dcf77_subghz_note_freq_millihz(index) / 1000.0f;
}

uint32_t dcf77_subghz_note_half_period_us(size_t index) {
    if(index >= dcf77_subghz_note_count()) {
        index = 0;
    }

    return dcf77_subghz_note_half_periods_us[index];
}

size_t dcf77_subghz_tx_timeout_count(void) {
    return sizeof(dcf77_subghz_tx_timeout_seconds_values) /
           sizeof(dcf77_subghz_tx_timeout_seconds_values[0]);
}

uint32_t dcf77_subghz_tx_timeout_seconds(size_t index) {
    if(index >= dcf77_subghz_tx_timeout_count()) {
        index = dcf77_subghz_tx_timeout_default_index();
    }

    return dcf77_subghz_tx_timeout_seconds_values[index];
}

uint8_t dcf77_subghz_tx_timeout_index_for_seconds(uint32_t seconds) {
    for(size_t i = 0; i < dcf77_subghz_tx_timeout_count(); i++) {
        if(dcf77_subghz_tx_timeout_seconds_values[i] == seconds) {
            return (uint8_t)i;
        }
    }

    return dcf77_subghz_tx_timeout_default_index();
}

uint8_t dcf77_subghz_tx_timeout_default_index(void) {
    return 8U;
}

const char* dcf77_subghz_tx_timeout_label(size_t index) {
    if(index >= dcf77_subghz_tx_timeout_count()) {
        index = dcf77_subghz_tx_timeout_default_index();
    }

    return dcf77_subghz_tx_timeout_text[index];
}

void dcf77_subghz_format_note_text(char* buffer, size_t buffer_size, size_t index) {
    snprintf(
        buffer, buffer_size, "%lu", (unsigned long)(dcf77_subghz_note_freq_millihz(index) / 1000U));
}

void dcf77_subghz_format_band_text(char* buffer, size_t buffer_size, uint32_t band_start_hz) {
    snprintf(buffer, buffer_size, "%lu MHz", (unsigned long)(band_start_hz / 1000000U));
}

void dcf77_subghz_format_frequency_text(char* buffer, size_t buffer_size, uint32_t freq_hz) {
    const uint32_t mhz = freq_hz / 1000000U;
    const uint32_t khz = (freq_hz % 1000000U) / 1000U;
    snprintf(buffer, buffer_size, "%lu.%03lu", (unsigned long)mhz, (unsigned long)khz);
}

void dcf77_subghz_format_frequency_khz_text(char* buffer, size_t buffer_size, uint32_t freq_hz) {
    snprintf(buffer, buffer_size, "%lu", (unsigned long)(freq_hz / 1000U));
}

uint32_t dcf77_subghz_clamp_frequency(uint32_t freq_hz, const Dcf77SubGhzBand* band) {
    if(freq_hz < band->start_hz) {
        return band->start_hz;
    }

    if(freq_hz > band->end_hz) {
        return band->end_hz;
    }

    return freq_hz;
}

uint32_t
    dcf77_subghz_step_frequency(uint32_t freq_hz, const Dcf77SubGhzBand* band, int8_t direction) {
    uint32_t next;

    freq_hz = dcf77_subghz_clamp_frequency(freq_hz, band);

    if(direction > 0) {
        if(freq_hz >= band->end_hz) {
            return band->end_hz;
        }

        next =
            ((freq_hz + DCF77_SUBGHZ_STEP_HZ - 1U) / DCF77_SUBGHZ_STEP_HZ) * DCF77_SUBGHZ_STEP_HZ;
        if(next <= freq_hz) {
            next += DCF77_SUBGHZ_STEP_HZ;
        }
        if(next > band->end_hz) {
            next = band->end_hz;
        }
        return next;
    }

    if(direction < 0) {
        if(freq_hz <= band->start_hz) {
            return band->start_hz;
        }

        next = (freq_hz / DCF77_SUBGHZ_STEP_HZ) * DCF77_SUBGHZ_STEP_HZ;
        if(next >= freq_hz && next >= DCF77_SUBGHZ_STEP_HZ) {
            next -= DCF77_SUBGHZ_STEP_HZ;
        }
        if(next < band->start_hz) {
            next = band->start_hz;
        }
        return next;
    }

    return freq_hz;
}
