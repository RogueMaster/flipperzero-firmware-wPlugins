#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <furi_hal_subghz.h>
#include <gui/gui.h>
#include <gui/icon.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <nfc/nfc_scanner.h>
#include <storage/storage.h>

#include <answer_book_icons.h>

#include <stdio.h>
#include <string.h>

#define TAG                 "AnswerBook"
#define ORACLE_RECORD_PATH  APP_DATA_PATH("answers.log")
#define ORACLE_SCAN_TICK_MS 120U

typedef enum {
    OracleModeIntro,
    OracleModeScanning,
    OracleModeResult,
} OracleMode;

typedef enum {
    OracleCategoryForward,
    OracleCategoryObserve,
    OracleCategoryWarning,
    OracleCategoryExplore,
    OracleCategoryReflection,
    OracleCategoryCount,
} OracleCategory;

typedef struct {
    uint8_t entropy;
    uint8_t activity;
    uint8_t stability;
    uint8_t novelty;
    uint8_t silence;
} OracleVector;

typedef struct {
    uint8_t ble_activity;
    uint8_t ble_rssi;
    uint8_t subghz_density;
    uint8_t subghz_delta;
    uint8_t nfc_presence;
    uint16_t nfc_protocol_mask;
} OracleSignals;

typedef struct {
    OracleMode mode;
    uint8_t progress;
    OracleVector vector;
    OracleSignals signals;
    OracleCategory category;
    DateTime time;
    uint32_t seed;
    uint32_t uptime_sec;
    uint8_t battery_pct;
    int8_t temperature_c;
    char answer[72];
    char rarity[12];
    char save_status[18];
} OracleModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    View* view;
    Storage* storage;
} OracleApp;

static const char* const category_names[OracleCategoryCount] = {
    "Forward",
    "Observe",
    "Warning",
    "Explore",
    "Reflection",
};

static const char* const forward_answers[] = {
    "Begin now.",
    "Take the next step.",
    "Yes, start small.",
    "Act before certainty.",
};

static const char* const observe_answers[] = {
    "Not yet.",
    "Wait and watch.",
    "Let it become clear.",
    "Give it one more day.",
};

static const char* const warning_answers[] = {
    "Do not force it.",
    "Something is off.",
    "Check the hidden cost.",
    "Pause before saying yes.",
};

static const char* const explore_answers[] = {
    "Try another path.",
    "Look somewhere new.",
    "Change the question.",
    "The side door opens.",
};

static const char* const reflection_answers[] = {
    "Ask what you avoid.",
    "The premise is wrong.",
    "You already know.",
    "Listen to the quiet part.",
};

static const Icon* const book_flip_frames[] = {
    &I_book_flip_0,
    &I_book_flip_1,
    &I_book_flip_2,
    &I_book_flip_3,
    &I_book_flip_4,
    &I_book_flip_5,
    &I_book_flip_6,
};

static uint8_t clamp_percent(int32_t value) {
    if(value < 0) return 0;
    if(value > 100) return 100;
    return value;
}

static uint8_t byte_popcount(uint8_t value) {
    uint8_t count = 0;
    while(value) {
        count += value & 1U;
        value >>= 1U;
    }
    return count;
}

static uint32_t fnv1a_mix(uint32_t hash, uint32_t value) {
    for(size_t i = 0; i < sizeof(value); i++) {
        hash ^= (value >> (i * 8U)) & 0xFFU;
        hash *= 16777619UL;
    }
    return hash;
}

static uint8_t rssi_to_score(float rssi) {
    if(rssi < -120.0f) return 0;
    if(rssi > -20.0f) return 100;
    return (uint8_t)(120.0f + rssi);
}

static uint8_t diff_u8(uint8_t a, uint8_t b) {
    return (a > b) ? (a - b) : (b - a);
}

static void collect_ble_signal(OracleSignals* signals, uint32_t* hash) {
    uint8_t samples[3] = {0};
    uint8_t channels[3] = {37, 38, 39};
    bool sampled = false;
    bool bt_active = furi_hal_bt_is_active();

    *hash = fnv1a_mix(*hash, furi_hal_bt_is_alive());
    *hash = fnv1a_mix(*hash, bt_active);
    *hash = fnv1a_mix(*hash, furi_hal_bt_get_radio_stack());

    if(furi_hal_bt_is_alive() && furi_hal_bt_is_testing_supported() && !bt_active) {
        for(size_t i = 0; i < COUNT_OF(channels); i++) {
            furi_hal_bt_start_rx(channels[i]);
            furi_delay_ms(12);
            samples[i] = rssi_to_score(furi_hal_bt_get_rssi());
            furi_hal_bt_stop_rx();
            *hash = fnv1a_mix(*hash, ((uint32_t)channels[i] << 8U) | samples[i]);
            sampled = true;
        }
    } else if(furi_hal_bt_is_alive()) {
        samples[0] = rssi_to_score(furi_hal_bt_get_rssi());
        sampled = true;
    }

    if(sampled) {
        uint16_t total = 0;
        uint8_t delta = 0;
        for(size_t i = 0; i < COUNT_OF(samples); i++) {
            total += samples[i];
            if(i > 0) delta += diff_u8(samples[i - 1], samples[i]);
        }

        signals->ble_rssi = total / COUNT_OF(samples);
        signals->ble_activity = clamp_percent(((uint16_t)signals->ble_rssi + (delta * 4U)) / 2U);
    } else {
        signals->ble_rssi = 0;
        signals->ble_activity = 0;
    }
}

static void collect_subghz_signal(OracleSignals* signals, uint32_t* hash) {
    static const uint32_t frequencies[] = {
        315000000UL,
        433920000UL,
        868350000UL,
        915000000UL,
    };
    uint8_t scores[COUNT_OF(frequencies)] = {0};
    uint8_t count = 0;
    uint8_t active_count = 0;
    uint16_t total = 0;
    uint8_t delta = 0;

    furi_hal_subghz_reset();
    furi_hal_subghz_flush_rx();

    for(size_t i = 0; i < COUNT_OF(frequencies); i++) {
        if(!furi_hal_subghz_is_frequency_valid(frequencies[i])) continue;

        uint32_t real_frequency = furi_hal_subghz_set_frequency_and_path(frequencies[i]);
        furi_hal_subghz_rx();
        furi_delay_ms(18);

        uint8_t rssi_score = rssi_to_score(furi_hal_subghz_get_rssi());
        uint8_t lqi = furi_hal_subghz_get_lqi();
        bool pipe = furi_hal_subghz_rx_pipe_not_empty();
        scores[count] = clamp_percent((uint16_t)rssi_score + (lqi / 4U) + (pipe ? 10U : 0U));
        if(scores[count] > 35) active_count++;
        if(count > 0) delta += diff_u8(scores[count - 1], scores[count]);
        total += scores[count];

        *hash = fnv1a_mix(*hash, real_frequency);
        *hash = fnv1a_mix(*hash, ((uint32_t)scores[count] << 8U) | lqi | (pipe ? 0x10000UL : 0));

        furi_hal_subghz_idle();
        furi_hal_subghz_flush_rx();
        count++;
    }

    furi_hal_subghz_sleep();

    if(count) {
        signals->subghz_density = clamp_percent((total / count + active_count * 16U) / 2U);
        signals->subghz_delta = delta / count;
    } else {
        signals->subghz_density = 0;
        signals->subghz_delta = 0;
    }
}

typedef struct {
    bool detected;
    uint8_t protocol_num;
    uint16_t protocol_mask;
} NfcSignalContext;

static void nfc_signal_callback(NfcScannerEvent event, void* context) {
    NfcSignalContext* signal = context;
    if(event.type != NfcScannerEventTypeDetected) return;

    signal->detected = true;
    signal->protocol_num = event.data.protocol_num;
    signal->protocol_mask = 0;
    for(size_t i = 0; i < event.data.protocol_num; i++) {
        if(event.data.protocols[i] < 16) {
            signal->protocol_mask |= 1U << event.data.protocols[i];
        }
    }
}

static void collect_nfc_signal(OracleSignals* signals, uint32_t* hash) {
    NfcSignalContext context = {0};
    Nfc* nfc = nfc_alloc();
    NfcScanner* scanner = nfc_scanner_alloc(nfc);

    nfc_scanner_start(scanner, nfc_signal_callback, &context);
    for(size_t i = 0; i < 12; i++) {
        if(context.detected) break;
        furi_delay_ms(20);
    }
    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);
    nfc_free(nfc);

    signals->nfc_presence = context.detected ? clamp_percent(55 + context.protocol_num * 15) : 0;
    signals->nfc_protocol_mask = context.protocol_mask;

    *hash = fnv1a_mix(*hash, signals->nfc_presence);
    *hash = fnv1a_mix(*hash, signals->nfc_protocol_mask);
}

static void collect_wireless_signals(OracleSignals* signals, uint32_t* hash) {
    memset(signals, 0, sizeof(OracleSignals));
    collect_ble_signal(signals, hash);
    collect_subghz_signal(signals, hash);
    collect_nfc_signal(signals, hash);
}

static const char* pick_from(const char* const* answers, size_t count, uint32_t seed) {
    return answers[seed % count];
}

static const char* pick_answer(OracleCategory category, uint32_t seed) {
    switch(category) {
    case OracleCategoryForward:
        return pick_from(forward_answers, COUNT_OF(forward_answers), seed);
    case OracleCategoryObserve:
        return pick_from(observe_answers, COUNT_OF(observe_answers), seed);
    case OracleCategoryWarning:
        return pick_from(warning_answers, COUNT_OF(warning_answers), seed);
    case OracleCategoryExplore:
        return pick_from(explore_answers, COUNT_OF(explore_answers), seed);
    case OracleCategoryReflection:
    default:
        return pick_from(reflection_answers, COUNT_OF(reflection_answers), seed);
    }
}

static OracleCategory choose_category(const OracleVector* vector, uint32_t seed) {
    uint16_t weights[OracleCategoryCount] = {10, 10, 10, 10, 10};

    if(vector->entropy > 80) {
        weights[OracleCategoryWarning] += 24;
        weights[OracleCategoryExplore] += 18;
    }
    if(vector->activity > 70) {
        weights[OracleCategoryForward] += 16;
        weights[OracleCategoryExplore] += 16;
    }
    if(vector->stability > 70) {
        weights[OracleCategoryObserve] += 18;
    } else if(vector->stability < 30) {
        weights[OracleCategoryWarning] += 12;
    }
    if(vector->novelty > 70) {
        weights[OracleCategoryExplore] += 20;
        weights[OracleCategoryReflection] += 12;
    }
    if(vector->silence > 80) {
        weights[OracleCategoryObserve] += 24;
        weights[OracleCategoryReflection] += 24;
    }

    uint16_t total = 0;
    for(size_t i = 0; i < OracleCategoryCount; i++) {
        total += weights[i];
    }

    uint16_t cursor = seed % total;
    for(size_t i = 0; i < OracleCategoryCount; i++) {
        if(cursor < weights[i]) return i;
        cursor -= weights[i];
    }

    return OracleCategoryObserve;
}

static void collect_environment(OracleModel* model) {
    uint32_t samples[12];
    uint32_t hash = 2166136261UL;
    uint16_t bit_activity = 0;
    uint16_t deltas = 0;
    uint16_t high_bytes = 0;
    uint16_t seen_nibbles = 0;

    furi_hal_rtc_get_datetime(&model->time);
    model->uptime_sec = furi_get_tick() / 1000U;
    model->battery_pct = furi_hal_power_get_pct();
    model->temperature_c = (int8_t)furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);

    hash = fnv1a_mix(hash, model->uptime_sec);
    hash = fnv1a_mix(hash, model->battery_pct);
    hash = fnv1a_mix(hash, (uint32_t)(uint8_t)model->temperature_c);
    hash = fnv1a_mix(hash, model->time.hour);
    hash = fnv1a_mix(hash, model->time.minute);
    hash = fnv1a_mix(hash, model->time.day);
    hash = fnv1a_mix(hash, model->time.weekday);

    collect_wireless_signals(&model->signals, &hash);

    for(size_t i = 0; i < COUNT_OF(samples); i++) {
        samples[i] = furi_hal_random_get();
        hash = fnv1a_mix(hash, samples[i]);
        bit_activity += byte_popcount(samples[i] & 0xFFU);
        high_bytes += (samples[i] >> 24U) & 0xFFU;
        seen_nibbles |= 1U << (samples[i] & 0xFU);
        if(i > 0) {
            uint8_t prev = samples[i - 1] & 0xFFU;
            uint8_t next = samples[i] & 0xFFU;
            deltas += (prev > next) ? (prev - next) : (next - prev);
        }
    }

    uint8_t unique_nibbles =
        byte_popcount(seen_nibbles & 0xFFU) + byte_popcount((seen_nibbles >> 8U) & 0xFFU);
    uint8_t temporal_bias =
        (model->time.hour * 3U + model->time.weekday * 7U + model->time.day) % 101U;
    uint8_t random_density = high_bytes / COUNT_OF(samples);
    uint8_t average_delta = deltas / (COUNT_OF(samples) - 1U);
    uint8_t radio_activity = clamp_percent(
        ((uint16_t)model->signals.ble_activity + model->signals.subghz_density +
         model->signals.nfc_presence) /
        3U);
    uint8_t radio_entropy = clamp_percent(
        ((uint16_t)model->signals.subghz_delta + average_delta +
         (model->signals.nfc_protocol_mask ? 35U : 0U)) /
        2U);
    uint8_t radio_novelty = clamp_percent(
        ((uint16_t)model->signals.nfc_presence + model->signals.ble_rssi +
         model->signals.subghz_delta) /
        3U);

    model->seed = hash;
    model->vector.activity = clamp_percent(
        (radio_activity * 2U +
         ((bit_activity * 100U) / (COUNT_OF(samples) * 8U) + random_density + temporal_bias) /
             3U) /
        3U);
    model->vector.entropy =
        clamp_percent((radio_entropy * 2U + random_density + temporal_bias) / 4U);
    model->vector.stability =
        clamp_percent(100 - ((uint16_t)average_delta + model->signals.subghz_delta) / 2U);
    model->vector.novelty =
        clamp_percent((radio_novelty * 2U + ((unique_nibbles * 100U) / 16U)) / 3U);
    model->vector.silence = clamp_percent(
        100 - ((model->signals.ble_activity + model->signals.subghz_density +
                model->signals.nfc_presence + model->vector.entropy) /
               4U));
}

static void generate_oracle(OracleModel* model) {
    collect_environment(model);

    bool legendary = ((model->seed % 200U) == 0U) ||
                     ((model->vector.entropy > 90U) && (model->vector.activity > 85U) &&
                      (model->vector.stability < 15U) && ((model->seed % 8U) == 0U));
    bool rare = legendary || ((model->seed % 20U) == 0U) ||
                ((model->vector.entropy > 80U) && (model->vector.activity > 70U) &&
                 (model->vector.stability < 35U));

    if(legendary) {
        model->category = OracleCategoryExplore;
        strlcpy(model->rarity, "Legendary", sizeof(model->rarity));
        strlcpy(model->answer, "The world answers.", sizeof(model->answer));
    } else {
        model->category = choose_category(&model->vector, model->seed);
        strlcpy(model->rarity, rare ? "Rare" : "Normal", sizeof(model->rarity));
        strlcpy(
            model->answer, pick_answer(model->category, model->seed >> 8U), sizeof(model->answer));
    }
}

static void save_oracle_record(OracleApp* app, OracleModel* model) {
    File* file = storage_file_alloc(app->storage);
    char line[512];
    int written = snprintf(
        line,
        sizeof(line),
        "{\"time\":\"%04u-%02u-%02uT%02u:%02u:%02u\","
        "\"ble_activity\":%u,\"ble_rssi\":%u,"
        "\"subghz_density\":%u,\"subghz_delta\":%u,"
        "\"nfc_presence\":%u,\"nfc_protocol_mask\":%u,"
        "\"entropy\":%u,\"activity\":%u,\"stability\":%u,\"novelty\":%u,\"silence\":%u,"
        "\"answer\":\"%s\",\"category\":\"%s\",\"rarity\":\"%s\",\"seed\":%lu}\n",
        model->time.year,
        model->time.month,
        model->time.day,
        model->time.hour,
        model->time.minute,
        model->time.second,
        model->signals.ble_activity,
        model->signals.ble_rssi,
        model->signals.subghz_density,
        model->signals.subghz_delta,
        model->signals.nfc_presence,
        model->signals.nfc_protocol_mask,
        model->vector.entropy,
        model->vector.activity,
        model->vector.stability,
        model->vector.novelty,
        model->vector.silence,
        model->answer,
        category_names[model->category],
        model->rarity,
        model->seed);

    storage_simply_mkdir(app->storage, APP_DATA_PATH(""));

    if((written > 0) && (written < (int)sizeof(line)) &&
       storage_file_open(file, ORACLE_RECORD_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        size_t bytes = storage_file_write(file, line, written);
        storage_file_close(file);
        strlcpy(
            model->save_status,
            (bytes == (size_t)written) ? "Saved to log" : "Save failed",
            sizeof(model->save_status));
        FURI_LOG_I(TAG, "Record write %u/%d bytes", bytes, written);
    } else {
        storage_file_close(file);
        strlcpy(model->save_status, "Save failed", sizeof(model->save_status));
        FURI_LOG_E(TAG, "Record open failed");
    }

    storage_file_free(file);
}

static void start_scan(OracleApp* app) {
    with_view_model(
        app->view,
        OracleModel * model,
        {
            model->mode = OracleModeScanning;
            model->progress = 0;
            strlcpy(model->answer, "Opening the book...", sizeof(model->answer));
            strlcpy(model->save_status, "", sizeof(model->save_status));
        },
        true);
}

static uint8_t tiny_font_rows(char c, uint8_t row) {
    static const uint8_t digits[10][5] = {
        {0x7, 0x5, 0x5, 0x5, 0x7},
        {0x2, 0x6, 0x2, 0x2, 0x7},
        {0x7, 0x1, 0x7, 0x4, 0x7},
        {0x7, 0x1, 0x7, 0x1, 0x7},
        {0x5, 0x5, 0x7, 0x1, 0x1},
        {0x7, 0x4, 0x7, 0x1, 0x7},
        {0x7, 0x4, 0x7, 0x5, 0x7},
        {0x7, 0x1, 0x2, 0x2, 0x2},
        {0x7, 0x5, 0x7, 0x5, 0x7},
        {0x7, 0x5, 0x7, 0x1, 0x7},
    };

    if(c >= 'a' && c <= 'z') c -= 32;
    if(c >= '0' && c <= '9') return digits[c - '0'][row];

    switch(c) {
    case 'A':
        return (uint8_t[]){0x2, 0x5, 0x7, 0x5, 0x5}[row];
    case 'B':
        return (uint8_t[]){0x6, 0x5, 0x6, 0x5, 0x6}[row];
    case 'C':
        return (uint8_t[]){0x3, 0x4, 0x4, 0x4, 0x3}[row];
    case 'D':
        return (uint8_t[]){0x6, 0x5, 0x5, 0x5, 0x6}[row];
    case 'E':
        return (uint8_t[]){0x7, 0x4, 0x6, 0x4, 0x7}[row];
    case 'F':
        return (uint8_t[]){0x7, 0x4, 0x6, 0x4, 0x4}[row];
    case 'G':
        return (uint8_t[]){0x3, 0x4, 0x5, 0x5, 0x3}[row];
    case 'H':
        return (uint8_t[]){0x5, 0x5, 0x7, 0x5, 0x5}[row];
    case 'I':
        return (uint8_t[]){0x7, 0x2, 0x2, 0x2, 0x7}[row];
    case 'J':
        return (uint8_t[]){0x1, 0x1, 0x1, 0x5, 0x2}[row];
    case 'K':
        return (uint8_t[]){0x5, 0x5, 0x6, 0x5, 0x5}[row];
    case 'L':
        return (uint8_t[]){0x4, 0x4, 0x4, 0x4, 0x7}[row];
    case 'M':
        return (uint8_t[]){0x5, 0x7, 0x7, 0x5, 0x5}[row];
    case 'N':
        return (uint8_t[]){0x5, 0x7, 0x7, 0x7, 0x5}[row];
    case 'O':
        return (uint8_t[]){0x2, 0x5, 0x5, 0x5, 0x2}[row];
    case 'P':
        return (uint8_t[]){0x6, 0x5, 0x6, 0x4, 0x4}[row];
    case 'Q':
        return (uint8_t[]){0x2, 0x5, 0x5, 0x7, 0x3}[row];
    case 'R':
        return (uint8_t[]){0x6, 0x5, 0x6, 0x5, 0x5}[row];
    case 'S':
        return (uint8_t[]){0x3, 0x4, 0x2, 0x1, 0x6}[row];
    case 'T':
        return (uint8_t[]){0x7, 0x2, 0x2, 0x2, 0x2}[row];
    case 'U':
        return (uint8_t[]){0x5, 0x5, 0x5, 0x5, 0x7}[row];
    case 'V':
        return (uint8_t[]){0x5, 0x5, 0x5, 0x5, 0x2}[row];
    case 'W':
        return (uint8_t[]){0x5, 0x5, 0x7, 0x7, 0x5}[row];
    case 'X':
        return (uint8_t[]){0x5, 0x5, 0x2, 0x5, 0x5}[row];
    case 'Y':
        return (uint8_t[]){0x5, 0x5, 0x2, 0x2, 0x2}[row];
    case 'Z':
        return (uint8_t[]){0x7, 0x1, 0x2, 0x4, 0x7}[row];
    default:
        return 0;
    }
}

static uint8_t tiny_text_width(const char* text) {
    uint8_t width = 0;
    while(*text) {
        width += (*text == ' ') ? 3 : 4;
        text++;
    }
    return width ? width - 1 : 0;
}

static void tiny_draw_text(Canvas* canvas, int32_t x, int32_t y, const char* text) {
    while(*text) {
        if(*text == ' ') {
            x += 3;
            text++;
            continue;
        }

        for(uint8_t row = 0; row < 5; row++) {
            uint8_t bits = tiny_font_rows(*text, row);
            for(uint8_t col = 0; col < 3; col++) {
                if(bits & (1U << (2U - col))) {
                    canvas_draw_dot(canvas, x + col, y + row);
                }
            }
        }

        x += 4;
        text++;
    }
}

static void oracle_draw_spark(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_dot(canvas, x, y - 1);
    canvas_draw_dot(canvas, x - 1, y);
    canvas_draw_dot(canvas, x, y);
    canvas_draw_dot(canvas, x + 1, y);
    canvas_draw_dot(canvas, x, y + 1);
}

static void oracle_draw_intro(Canvas* canvas) {
    // Book-cover frame around the whole screen.
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 5);

    // Centered title with a small flourish on each side.
    canvas_set_font(canvas, FontPrimary);
    const char* title = "Answer Book";
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, title);
    uint8_t half = canvas_string_width(canvas, title) / 2U;
    canvas_draw_line(canvas, 64 - half - 10, 8, 64 - half - 5, 8);
    canvas_draw_line(canvas, 64 + half + 5, 8, 64 + half + 10, 8);

    // Closed-book illustration, horizontally centered (icon is 48x30).
    canvas_draw_icon(canvas, 40, 16, &I_book_closed);

    // Filled "Press OK" pill as the call to action.
    canvas_set_font(canvas, FontSecondary);
    const char* cta = "Press OK";
    uint8_t pill_w = canvas_string_width(canvas, cta) + 16U;
    uint8_t pill_x = 64U - pill_w / 2U;
    canvas_draw_rbox(canvas, pill_x, 49, pill_w, 13, 4);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, cta);
    canvas_set_color(canvas, ColorBlack);
}

static void oracle_draw_callback(Canvas* canvas, void* context) {
    OracleModel* model = context;
    char line[64];

    canvas_clear(canvas);

    if(model->mode == OracleModeScanning) {
        uint8_t frame_index = (model->progress * COUNT_OF(book_flip_frames)) / 101U;
        canvas_draw_icon(canvas, 40, 17, book_flip_frames[frame_index]);
        return;
    }

    if(model->mode == OracleModeIntro) {
        oracle_draw_intro(canvas);
        return;
    }

    // Revealed archive page: framed, sparse, and quietly mysterious.
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 5);

    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "No.%03lu", (model->seed % 999UL) + 1UL);
    canvas_draw_str(canvas, 7, 12, line);

    // Rarity sigil, always shown in the top-right corner.
    uint8_t rarity_w = canvas_string_width(canvas, model->rarity);
    canvas_draw_str_aligned(canvas, 120, 12, AlignRight, AlignBottom, model->rarity);
    oracle_draw_spark(canvas, 120 - rarity_w - 5, 9);

    // Delicate dotted rule instead of a hard header line.
    for(uint8_t x = 8; x <= 120; x += 4) {
        canvas_draw_dot(canvas, x, 17);
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, model->answer);

    // Category engraved in small caps, flanked by sparks. No operational hints.
    const char* category = category_names[model->category];
    uint8_t category_w = tiny_text_width(category);
    uint8_t category_x = (128 - category_w) / 2U;
    tiny_draw_text(canvas, category_x, 52, category);
    oracle_draw_spark(canvas, category_x - 8, 54);
    oracle_draw_spark(canvas, category_x + category_w + 7, 54);
}

static bool oracle_input_callback(InputEvent* event, void* context) {
    OracleApp* app = context;
    bool handled = false;

    if((event->key == InputKeyOk) && (event->type == InputTypeShort)) {
        FURI_LOG_I(TAG, "OK short received");
        start_scan(app);
        handled = true;
    }

    return handled;
}

static void oracle_tick_callback(void* context) {
    OracleApp* app = context;
    bool should_generate = false;

    with_view_model(
        app->view,
        OracleModel * model,
        {
            if(model->mode == OracleModeScanning) {
                uint8_t step = 9U + (furi_hal_random_get() % 13U);
                if((uint16_t)model->progress + step >= 100U) {
                    model->progress = 100U;
                    should_generate = true;
                    FURI_LOG_I(TAG, "Page opened");
                } else {
                    model->progress += step;
                }
            }
        },
        true);

    if(should_generate) {
        with_view_model(
            app->view,
            OracleModel * model,
            {
                generate_oracle(model);
                save_oracle_record(app, model);
                model->mode = OracleModeResult;
            },
            true);
    }
}

static bool oracle_navigation_callback(void* context) {
    OracleApp* app = context;
    view_dispatcher_stop(app->dispatcher);
    return true;
}

static OracleApp* oracle_app_alloc(void) {
    OracleApp* app = malloc(sizeof(OracleApp));
    memset(app, 0, sizeof(OracleApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();

    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(OracleModel));
    with_view_model(
        app->view,
        OracleModel * model,
        {
            memset(model, 0, sizeof(OracleModel));
            model->mode = OracleModeIntro;
            strlcpy(model->rarity, "Normal", sizeof(model->rarity));
            strlcpy(model->answer, "Hold a question.", sizeof(model->answer));
        },
        false);

    view_set_draw_callback(app->view, oracle_draw_callback);
    view_set_input_callback(app->view, oracle_input_callback);
    view_set_context(app->view, app);

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, oracle_navigation_callback);
    view_dispatcher_set_tick_event_callback(
        app->dispatcher, oracle_tick_callback, ORACLE_SCAN_TICK_MS);
    view_dispatcher_add_view(app->dispatcher, 0, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->dispatcher, 0);

    return app;
}

static void oracle_app_free(OracleApp* app) {
    view_dispatcher_remove_view(app->dispatcher, 0);
    view_free_model(app->view);
    view_free(app->view);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t answer_book_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Starting Answer Book");
    OracleApp* app = oracle_app_alloc();
    view_dispatcher_run(app->dispatcher);
    oracle_app_free(app);
    FURI_LOG_I(TAG, "Stopped Answer Book");

    return 0;
}
