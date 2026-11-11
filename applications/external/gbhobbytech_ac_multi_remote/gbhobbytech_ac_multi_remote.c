#include <furi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <infrared/worker/infrared_worker.h>
#include <lib/infrared/signal/infrared_signal.h>
#include "ac_mitsubishi.h"
#include "ac_mitsubishi_legacy.h"
#include "ac_mhi.h"
#include "ac_gree.h"
#include "ac_fujitsu.h"
#include "ac_protocols.h"
#include "ac_lg.h"

#define EVENT_QUEUE_SIZE                        12U
#define MAX_PROFILES                            8U
#define OK_DOUBLE_CLICK_MS                      300U
#define PROFILE_DIRECTORY                       EXT_PATH("apps_data/gbhobbytech_ac_multi_remote")
#define PROFILE_PATH_FORMAT                     PROFILE_DIRECTORY "/AC_Unit_%02u.ir"
#define PROFILE_NAME_PATH_FORMAT                PROFILE_DIRECTORY "/AC_Unit_%02u.name"
#define PROFILE_NAME_LENGTH                     16U
#define RENAME_TEXT_VIEW_ID                     1U
#define MITSUBISHI_TEMP_MIN                     16U
#define MITSUBISHI_TEMP_MAX                     31U
#define MENU_VISIBLE_ROWS                       3U
#define AC_ROOM_PROBE_MAX_VARIANTS              12U
#define AC_CAPTURE_QUIET_TIMEOUT_MS             300U
#define AC_CAPTURE_INTER_SIGNAL_GAP_FALLBACK_US 40000U
#define AC_CAPTURE_MAX_TIMINGS                  2048U
static const uint8_t mitsubishi_fan_codes[] = {0U, 1U, 2U, 3U, 4U};
static const uint8_t mhi_88_fan_codes[] = {0U, 2U, 3U, 4U, 6U};
static const uint8_t mhi_152_fan_codes[] = {0U, 1U, 2U, 3U, 4U};
#define MHI_TEMP_MIN 17U
#define MHI_TEMP_MAX 31U

typedef enum {
    KeypadModeControl,
    KeypadModeSelectUnit,
} KeypadMode;

typedef enum {
    PageControl,
    PageSelectUnit,
    PageEditUnit,
    PageDeleteConfirm,
    PageEditFailed,
    PageWizardMethod,
    PageWizardSelectBrand,
    PageNoBrandProtocols,
    PageRoomProbe,
    PageRoomProbeQuestion,
    PageRoomProbeSaved,
    PageRoomProbeFailed,
    PageLearnNormal,
    PageListenNormal,
    PageLearnOff,
    PageListenOff,
    PageLearnDone,
    PageLearnFailed,
} AppPage;

typedef enum {
    AppEventTypeInput,
    AppEventTypeSignalCaptured,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    FuriMessageQueue* event_queue;
    FuriMutex* capture_mutex;
    ViewPort* view_port;
    Gui* gui;
    ViewDispatcher* rename_dispatcher;
    TextInput* rename_text_input;
    char rename_buffer[PROFILE_NAME_LENGTH + 1U];
    bool rename_accepted;
    InfraredWorker* infrared_worker;
    InfraredSignal* capture_signal;
    InfraredSignal* normal_signal;
    uint32_t* capture_timings;
    size_t capture_timings_count;
    uint32_t last_raw_signal_tick;
    uint32_t capture_last_signal_tick;
    bool capture_is_raw;
    bool capture_overflow;
    KeypadMode mode;
    AppPage page;
    uint8_t profile_ids[MAX_PROFILES];
    char profile_names[MAX_PROFILES][PROFILE_NAME_LENGTH + 1U];
    size_t profile_count;
    size_t selected_unit_index;
    size_t edit_menu_index;
    size_t wizard_method_index;
    size_t selected_manufacturer_index;
    bool has_expected_variant;
    size_t expected_variant;
    size_t room_probe_variants[AC_ROOM_PROBE_MAX_VARIANTS];
    size_t room_probe_count;
    size_t room_probe_position;
    AcIdentifyResult learned_result;
    size_t learned_matches[AC_IDENTIFY_MAX_MATCHES];
    size_t learned_match_count;
    uint8_t active_profile_id;
    uint8_t pending_profile_id;
    uint8_t temperature_c;
    uint8_t fan_speed;
    uint8_t mitsubishi_frame[AC_MITSUBISHI_FRAME_BYTES];
    uint8_t mitsubishi_legacy_frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES];
    AcMitsubishiLegacyVariant mitsubishi_legacy_variant;
    uint32_t mitsubishi_prefix[AC_MITSUBISHI_PREFIX_MAX];
    size_t mitsubishi_prefix_count;
    uint8_t mhi_frame[AC_MHI_MAX_STATE_BYTES];
    uint8_t gree_frame[AC_GREE_STATE_BYTES];
    AcGreeModel gree_model;
    AcMhiVariant mhi_variant;
    AcFujitsuVariant fujitsu_variant;
    uint8_t fujitsu_frame[AC_FUJITSU_MAX_STATE_BYTES];
    uint8_t mhi_mode;
    bool mhi_supported;
    bool gree_supported;
    bool fujitsu_supported;
    bool lg_supported;
    AcLgVariant lg_variant;
    uint8_t lg_mode;
    bool state_control_supported;
    bool power_on;
    bool preview_power_on;
    bool capturing;
    bool capture_received;
    bool running;
} AcMultiRemoteState;

static const char* const fan_speed_names[] = {"AUTO", "LOW", "MED", "HIGH", "MAX"};
static const char* const wizard_method_names[] = {
    "IDENTIFY UNIT",
    "LEARN REMOTE",
};
#define WIZARD_METHOD_COUNT    (sizeof(wizard_method_names) / sizeof(wizard_method_names[0]))
#define WIZARD_METHOD_IDENTIFY 0U
#define FAN_SPEED_COUNT        (sizeof(fan_speed_names) / sizeof(fan_speed_names[0]))

static uint8_t active_fan_count(const AcMultiRemoteState* state) {
    if(state->mhi_supported) return (uint8_t)FAN_SPEED_COUNT;
    if(state->gree_supported) return 4U;
    if(state->fujitsu_supported) return 5U;
    if(state->lg_supported) return 4U;
    if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone) {
        return ac_mitsubishi_legacy_fan_count(state->mitsubishi_legacy_variant);
    }
    return (uint8_t)FAN_SPEED_COUNT;
}

static const char* active_fan_name(const AcMultiRemoteState* state) {
    static const char* const legacy_fan_names[] = {"QUIET", "LOW", "MED", "MAX"};
    static const char* const gree_fan_names[] = {"AUTO", "LOW", "MED", "MAX"};
    static const char* const fujitsu_fan_names[] = {"AUTO", "LOW", "MED", "HIGH", "QUIET"};
    if(state->fujitsu_supported && state->fan_speed < 5U) {
        return fujitsu_fan_names[state->fan_speed];
    }
    if(state->gree_supported && state->fan_speed < 4U) {
        return gree_fan_names[state->fan_speed];
    }
    if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone &&
       state->fan_speed < (uint8_t)(sizeof(legacy_fan_names) / sizeof(legacy_fan_names[0]))) {
        return legacy_fan_names[state->fan_speed];
    }
    return fan_speed_names
        [state->fan_speed < (uint8_t)FAN_SPEED_COUNT ? state->fan_speed :
                                                       (uint8_t)(FAN_SPEED_COUNT - 1U)];
}

static const char* active_profile_name(const AcMultiRemoteState* state) {
    for(size_t i = 0U; i < state->profile_count; i++) {
        if(state->profile_ids[i] == state->active_profile_id) {
            return state->profile_names[i];
        }
    }
    return NULL;
}

static void app_request_redraw(AcMultiRemoteState* state) {
    view_port_update(state->view_port);
}

static void input_callback(InputEvent* input, void* context) {
    AcMultiRemoteState* state = context;

    const bool long_action =
        input->type == InputTypeLong &&
        (input->key == InputKeyOk || input->key == InputKeyLeft || input->key == InputKeyRight);

    if(input->type == InputTypeShort || long_action) {
        AppEvent event = {
            .type = AppEventTypeInput,
            .input = *input,
        };
        /* GUI input callback must never block if the event queue is full. */
        furi_message_queue_put(state->event_queue, &event, 0U);
    }
}

/* Preserve the mark/space parity when appending successive IR bursts.
 * Cap the buffer before copying so malformed/long captures cannot overrun it. */
static bool append_capture_timings(
    AcMultiRemoteState* state,
    const uint32_t* timings,
    size_t timings_count,
    uint32_t now_tick) {
    if(!state->capture_timings || !timings || timings_count == 0U || state->capture_overflow) {
        return false;
    }

    uint64_t signal_duration_us = 0U;
    for(size_t i = 0U; i < timings_count; i++) {
        signal_duration_us += timings[i];
    }

    size_t gap_slots = 0U;
    uint32_t gap_us = AC_CAPTURE_INTER_SIGNAL_GAP_FALLBACK_US;
    if(state->capture_timings_count > 0U) {
        /* The worker calls back after each raw packet. Remove this packet's
         * duration from the callback interval to estimate the intervening space. */
        gap_slots = (state->capture_timings_count & 1U) ? 1U : 0U;
        const uint64_t elapsed_us = (uint64_t)(now_tick - state->last_raw_signal_tick) * 1000U;
        if(elapsed_us > signal_duration_us) {
            const uint64_t estimated_gap_us = elapsed_us - signal_duration_us;
            if(estimated_gap_us >= 5000U && estimated_gap_us <= 250000U) {
                gap_us = (uint32_t)estimated_gap_us;
            }
        }
    }

    if(timings_count > AC_CAPTURE_MAX_TIMINGS - state->capture_timings_count - gap_slots) {
        state->capture_overflow = true;
        return false;
    }

    if(state->capture_timings_count > 0U) {
        if(gap_slots > 0U) {
            state->capture_timings[state->capture_timings_count++] = gap_us;
        } else {
            state->capture_timings[state->capture_timings_count - 1U] += gap_us;
        }
    }

    memcpy(
        &state->capture_timings[state->capture_timings_count],
        timings,
        timings_count * sizeof(uint32_t));
    state->capture_timings_count += timings_count;
    state->last_raw_signal_tick = now_tick;
    return true;
}

static void received_signal_callback(void* context, InfraredWorkerSignal* received_signal) {
    AcMultiRemoteState* state = context;
    if(furi_mutex_acquire(state->capture_mutex, FuriWaitForever) != FuriStatusOk) return;

    bool post_event = false;
    const uint32_t now_tick = furi_get_tick();
    if(state->capturing) {
        if(infrared_worker_signal_is_decoded(received_signal)) {
            if(!state->capture_received) {
                infrared_signal_set_message(
                    state->capture_signal, infrared_worker_get_decoded_signal(received_signal));
                state->capture_is_raw = false;
                state->capture_received = true;
                state->capture_last_signal_tick = now_tick;
                post_event = true;
            }
        } else if(!state->capture_received || state->capture_is_raw) {
            const uint32_t* timings = NULL;
            size_t timings_count = 0U;
            infrared_worker_get_raw_signal(received_signal, &timings, &timings_count);
            if(append_capture_timings(state, timings, timings_count, now_tick)) {
                state->capture_is_raw = true;
            } else {
                state->capture_overflow = true;
            }
            state->capture_received = true;
            state->capture_last_signal_tick = now_tick;
            post_event = true;
        }
    }

    furi_mutex_release(state->capture_mutex);

    if(post_event) {
        AppEvent event = {.type = AppEventTypeSignalCaptured};
        /* Capture is also polled via capture_activity_snapshot(), so a
         * full queue must not block the infrared worker callback. */
        furi_message_queue_put(state->event_queue, &event, 0U);
    }
}

static void draw_header(Canvas* canvas, const char* heading) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "A/C MASTER");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 20, heading);
    canvas_draw_line(canvas, 4, 23, 123, 23);
}

static void draw_str_bold(Canvas* canvas, int32_t x, int32_t y, const char* text) {
    canvas_draw_str(canvas, x, y, text);
    canvas_draw_str(canvas, x + 1, y, text);
}

/* Direction arrows share the line style used for the main-screen controls. */
static void draw_left_arrow(Canvas* canvas, int32_t x, int32_t cy) {
    canvas_draw_line(canvas, x + 4, cy, x, cy);
    canvas_draw_line(canvas, x, cy, x + 2, cy - 2);
    canvas_draw_line(canvas, x, cy, x + 2, cy + 2);
}

static void draw_right_arrow(Canvas* canvas, int32_t x, int32_t cy) {
    canvas_draw_line(canvas, x, cy, x + 4, cy);
    canvas_draw_line(canvas, x + 4, cy, x + 2, cy - 2);
    canvas_draw_line(canvas, x + 4, cy, x + 2, cy + 2);
}

static void draw_up_arrow(Canvas* canvas, int32_t cx, int32_t cy) {
    canvas_draw_line(canvas, cx, cy + 2, cx, cy - 2);
    canvas_draw_line(canvas, cx, cy - 2, cx - 2, cy);
    canvas_draw_line(canvas, cx, cy - 2, cx + 2, cy);
}

static void draw_down_arrow(Canvas* canvas, int32_t cx, int32_t cy) {
    canvas_draw_line(canvas, cx, cy - 2, cx, cy + 2);
    canvas_draw_line(canvas, cx, cy + 2, cx - 2, cy);
    canvas_draw_line(canvas, cx, cy + 2, cx + 2, cy);
}

static void draw_fan_speed_arrows(Canvas* canvas, int32_t x, int32_t text_baseline_y) {
    const int32_t cy = text_baseline_y - 4;
    draw_left_arrow(canvas, x + 2, cy);
    draw_right_arrow(canvas, x + 7, cy);
}

/* Temperature up/down arrows below the temperature. */
static void draw_temp_arrows(Canvas* canvas, int32_t x, int32_t cy) {
    draw_up_arrow(canvas, x, cy);
    draw_down_arrow(canvas, x + 7, cy);
}

/* Short OK press: power. */
static void draw_power_short_icon(Canvas* canvas, int32_t cx, int32_t cy) {
    canvas_draw_disc(canvas, cx, cy, 2);
}

/* Long OK press: mode. */
static void draw_mode_hold_icon(Canvas* canvas, int32_t cx, int32_t cy) {
    canvas_draw_circle(canvas, cx, cy, 3);
    canvas_draw_disc(canvas, cx, cy, 1);
}

/* Double-click OK: open the unit list. */
static void draw_double_ok_icon(Canvas* canvas, int32_t cx, int32_t cy) {
    canvas_draw_disc(canvas, cx, cy, 2);
    canvas_draw_disc(canvas, cx + 6, cy, 2);
}

static void draw_short_ok_hint(Canvas* canvas, int32_t x, int32_t baseline, const char* label) {
    draw_power_short_icon(canvas, x + 2, baseline - 4);
    canvas_draw_str(canvas, x + 8, baseline, label);
}

static void draw_hold_ok_hint(Canvas* canvas, int32_t x, int32_t baseline, const char* label) {
    draw_mode_hold_icon(canvas, x + 3, baseline - 4);
    canvas_draw_str(canvas, x + 10, baseline, label);
}

static void draw_control_key_hints(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 5, 63, "HOLD");
    draw_fan_speed_arrows(canvas, 30, 63);
    canvas_draw_str(canvas, 45, 63, "UNIT");
    draw_double_ok_icon(canvas, 92, 59);
    canvas_draw_str(canvas, 104, 63, "LIST");
}

static void draw_control_screen(Canvas* canvas, const AcMultiRemoteState* state) {
    char temperature_text[16];
    char power_text[12];
    char fan_text[12];
    char profile_text[24];
    char mode_text[12];

    if(state->active_profile_id && state->state_control_supported) {
        const uint8_t mode_code =
            state->lg_supported ?
                state->lg_mode :
            state->fujitsu_supported ?
                (state->fujitsu_frame[9] & 0x07U) :
            state->mhi_supported ?
                state->mhi_mode :
                (state->gree_supported ?
                     (state->gree_frame[0] & 0x07U) :
                     (state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone ?
                          (state->mitsubishi_legacy_frame[6] & 0x07U) :
                          (uint8_t)((state->mitsubishi_frame[6] >> 3) & 0x07U)));
        const char* mode_name = "UNKNOWN";
        if(state->lg_supported) {
            mode_name = mode_code == AC_LG_MODE_HEAT ? "HEAT" : "COOL";
        } else if(state->fujitsu_supported) {
            if(mode_code == 0U)
                mode_name = "AUTO";
            else if(mode_code == 1U)
                mode_name = "COOL";
            else if(mode_code == 2U)
                mode_name = "DRY";
            else if(mode_code == 3U)
                mode_name = "FAN";
            else if(mode_code == 4U)
                mode_name = "HEAT";
        } else if(state->mhi_supported) {
            if(mode_code == (uint8_t)AcMhiModeAuto)
                mode_name = "AUTO";
            else if(mode_code == (uint8_t)AcMhiModeCool)
                mode_name = "COOL";
            else if(mode_code == (uint8_t)AcMhiModeDry)
                mode_name = "DRY";
            else if(mode_code == (uint8_t)AcMhiModeFan)
                mode_name = "FAN";
            else if(mode_code == (uint8_t)AcMhiModeHeat)
                mode_name = "HEAT";
        } else if(state->gree_supported) {
            if(mode_code == 0U)
                mode_name = "AUTO";
            else if(mode_code == 1U)
                mode_name = "COOL";
            else if(mode_code == 2U)
                mode_name = "DRY";
            else if(mode_code == 3U)
                mode_name = "FAN";
            else if(mode_code == 4U)
                mode_name = "HEAT";
            else if(mode_code == 5U)
                mode_name = "ECO";
        } else if(state->mitsubishi_legacy_variant == AcMitsubishiLegacy112) {
            if(mode_code == 1U)
                mode_name = "HEAT";
            else if(mode_code == 2U)
                mode_name = "DRY";
            else if(mode_code == 3U)
                mode_name = "COOL";
            else if(mode_code == 7U)
                mode_name = "AUTO";
        } else if(state->mitsubishi_legacy_variant == AcMitsubishiLegacy136) {
            if(mode_code == 0U)
                mode_name = "FAN";
            else if(mode_code == 1U)
                mode_name = "COOL";
            else if(mode_code == 2U)
                mode_name = "HEAT";
            else if(mode_code == 3U)
                mode_name = "AUTO";
            else if(mode_code == 5U)
                mode_name = "DRY";
        } else {
            if(mode_code == 1U)
                mode_name = "HEAT";
            else if(mode_code == 2U)
                mode_name = "DRY";
            else if(mode_code == 3U)
                mode_name = "COOL";
            else if(mode_code == 4U)
                mode_name = "AUTO";
            else if(mode_code == 7U)
                mode_name = "FAN";
        }

        const char* const active_name = active_profile_name(state);
        snprintf(profile_text, sizeof(profile_text), "%s", active_name ? active_name : "UNIT");
        snprintf(temperature_text, sizeof(temperature_text), "%u", (unsigned)state->temperature_c);
        snprintf(power_text, sizeof(power_text), "%s", state->power_on ? "ON" : "OFF");
        snprintf(fan_text, sizeof(fan_text), "%s", active_fan_name(state));
        snprintf(mode_text, sizeof(mode_text), "%s", mode_name);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 9, "A/C MASTER");
        const int32_t name_x = 124 - (int32_t)canvas_string_width(canvas, profile_text);
        canvas_draw_str(canvas, name_x, 9, profile_text);
        canvas_draw_line(canvas, 4, 13, 123, 13);

        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 4, 48, temperature_text);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 30, 34, "\u00b0C");
        canvas_draw_str(canvas, 42, 32, "MODE");
        canvas_draw_str(canvas, 42, 41, "POWER");
        canvas_draw_str(canvas, 42, 50, "FAN");

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 78, 32, mode_text);
        draw_mode_hold_icon(canvas, 122, 28);

        canvas_draw_str(canvas, 78, 41, power_text);
        draw_power_short_icon(canvas, 122, 37);

        canvas_draw_str(canvas, 78, 50, fan_text);
        draw_fan_speed_arrows(canvas, 115, 50);

        canvas_draw_line(canvas, 4, 54, 123, 54);
        draw_temp_arrows(canvas, 11, 25);
        draw_control_key_hints(canvas);
    } else {
        draw_header(canvas, "CONTROL / 2xOK: UNITS");

        if(state->active_profile_id) {
            const char* const active_name = active_profile_name(state);
            snprintf(
                profile_text,
                sizeof(profile_text),
                "%s",
                active_name ? active_name : "LEARNED UNIT");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 33, profile_text);

            snprintf(
                power_text, sizeof(power_text), "POWER  %s", state->power_on ? "ON*" : "OFF*");
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 4, 46, power_text);

            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 56, "TEMP/FAN NOT LEARNED");
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 33, "NO PROFILE - PREVIEW");

            snprintf(
                temperature_text,
                sizeof(temperature_text),
                "TEMP  %u C",
                (unsigned)state->temperature_c);
            snprintf(
                power_text,
                sizeof(power_text),
                "POWER  %s",
                state->preview_power_on ? "ON" : "OFF");
            snprintf(fan_text, sizeof(fan_text), "FAN  %s", fan_speed_names[state->fan_speed]);

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 4, 46, temperature_text);

            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 55, power_text);
            canvas_draw_str(canvas, 70, 55, fan_text);
        }
    }
}

static void draw_unit_screen(Canvas* canvas, const AcMultiRemoteState* state) {
    draw_header(canvas, "SELECT UNIT");
    canvas_set_font(canvas, FontSecondary);

    if(state->profile_count == 0U) {
        canvas_draw_str(canvas, 4, 37, "NO SAVED UNITS");
        draw_down_arrow(canvas, 8, 48);
        canvas_draw_str(canvas, 16, 52, "ADD UNIT");
        draw_double_ok_icon(canvas, 7, 59);
        canvas_draw_str(canvas, 22, 63, "CONTROL");
    } else if(state->selected_unit_index < state->profile_count) {
        canvas_draw_str(canvas, 4, 37, state->profile_names[state->selected_unit_index]);

        draw_fan_speed_arrows(canvas, 4, 52);
        canvas_draw_str(canvas, 18, 52, "CHANGE");
        draw_short_ok_hint(canvas, 55, 52, "USE");
        draw_hold_ok_hint(canvas, 82, 52, "EDIT");

        draw_down_arrow(canvas, 8, 59);
        canvas_draw_str(canvas, 16, 63, "ADD");
        draw_double_ok_icon(canvas, 61, 59);
        canvas_draw_str(canvas, 76, 63, "CONTROL");
    }
}

/* ---- Wizard list menus ---- */

/* Number of entries in the list shown on the current wizard page. Each list
 * ends with a "not listed" entry that leads to capture identification. */
static size_t wizard_menu_count(const AcMultiRemoteState* state) {
    switch(state->page) {
    case PageWizardMethod:
        return WIZARD_METHOD_COUNT;
    case PageWizardSelectBrand:
        return ac_manufacturer_count() + 1U;
    default:
        return 0U;
    }
}

static size_t* wizard_menu_selection(AcMultiRemoteState* state) {
    switch(state->page) {
    case PageWizardMethod:
        return &state->wizard_method_index;
    case PageWizardSelectBrand:
        return &state->selected_manufacturer_index;
    default:
        return NULL;
    }
}

static size_t wizard_menu_selected(const AcMultiRemoteState* state) {
    return wizard_menu_selection((AcMultiRemoteState*)state) ?
               *wizard_menu_selection((AcMultiRemoteState*)state) :
               0U;
}

static void wizard_menu_label(
    const AcMultiRemoteState* state,
    size_t index,
    char* label,
    size_t label_size) {
    if(state->page == PageWizardMethod) {
        snprintf(label, label_size, "%s", wizard_method_names[index]);
    } else if(index < ac_manufacturer_count()) {
        const AcManufacturer* manufacturer = ac_manufacturer_get(index);
        snprintf(label, label_size, "%s", manufacturer ? manufacturer->name : "?");
    } else {
        snprintf(label, label_size, "NOT SURE: TRY ALL");
    }
}

/* Scrollable list: three rows, inverted highlight on the selection, and a
 * scroll bar on the right when the list is longer than the screen. */
static void draw_wizard_menu(Canvas* canvas, const AcMultiRemoteState* state) {
    const size_t count = wizard_menu_count(state);
    const size_t selected = wizard_menu_selected(state);
    const size_t top = selected >= MENU_VISIBLE_ROWS ? selected - MENU_VISIBLE_ROWS + 1U : 0U;
    char label[32];

    canvas_set_font(canvas, FontSecondary);
    for(size_t row = 0; row < MENU_VISIBLE_ROWS && top + row < count; row++) {
        const size_t index = top + row;
        const int32_t baseline = 33 + (int32_t)row * 9;
        wizard_menu_label(state, index, label, sizeof(label));

        if(index == selected) {
            canvas_draw_rbox(canvas, 2, baseline - 7, 120, 9, 1);
            canvas_set_color(canvas, ColorWhite);
            draw_str_bold(canvas, 5, baseline, label);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 5, baseline, label);
        }
    }

    if(count > MENU_VISIBLE_ROWS) {
        const int32_t track_top = 25;
        const int32_t track_height = 30;
        int32_t thumb_height = track_height * (int32_t)MENU_VISIBLE_ROWS / (int32_t)count;
        if(thumb_height < 4) thumb_height = 4;
        const int32_t thumb_top =
            track_top + (track_height - thumb_height) * (int32_t)selected / (int32_t)(count - 1U);
        /* Dotted track with a solid 3 px thumb, so the thumb stands out. */
        for(int32_t y = track_top; y < track_top + track_height; y += 2) {
            canvas_draw_dot(canvas, 126, y);
        }
        canvas_draw_box(canvas, 125, thumb_top, 3, thumb_height);
    }

    draw_temp_arrows(canvas, 7, 59);
    canvas_draw_str(canvas, 18, 63, "MOVE");
    draw_short_ok_hint(canvas, 55, 63, "SELECT");
    canvas_draw_str(canvas, 102, 63, "BACK");
}

static void draw_variant_name(Canvas* canvas, int32_t x, int32_t y, size_t variant_index) {
    const AcProtocolFamily* family = ac_variant_family(variant_index);
    const AcProtocolVariant* variant = ac_variant_get(variant_index);
    char text[40];
    snprintf(
        text, sizeof(text), "%s %s", family ? family->name : "?", variant ? variant->name : "?");
    draw_str_bold(canvas, x, y, text);
}

static void draw_learning_screen(Canvas* canvas, const AcMultiRemoteState* state) {
    canvas_set_font(canvas, FontSecondary);

    switch(state->page) {
    case PageWizardMethod:
        draw_header(canvas, "ADD UNIT: IDENTIFY BY");
        draw_wizard_menu(canvas, state);
        break;
    case PageWizardSelectBrand:
        draw_header(canvas, "IDENTIFY UNIT BRAND");
        draw_wizard_menu(canvas, state);
        break;
    case PageEditUnit: {
        draw_header(canvas, "EDIT UNIT");
        if(state->selected_unit_index < state->profile_count) {
            canvas_draw_str(canvas, 4, 34, state->profile_names[state->selected_unit_index]);
        }
        const char* const actions[] = {"RENAME", "DELETE"};
        for(size_t i = 0U; i < 2U; i++) {
            const int32_t baseline = 45 + (int32_t)i * 10;
            if(i == state->edit_menu_index) {
                canvas_draw_rbox(canvas, 2, baseline - 8, 120, 10, 1);
                canvas_set_color(canvas, ColorWhite);
                draw_str_bold(canvas, 5, baseline, actions[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 5, baseline, actions[i]);
            }
        }
        draw_temp_arrows(canvas, 7, 59);
        canvas_draw_str(canvas, 18, 63, "MOVE");
        draw_short_ok_hint(canvas, 55, 63, "SELECT");
        canvas_draw_str(canvas, 102, 63, "BACK");
        break;
    }
    case PageDeleteConfirm:
        draw_header(canvas, "DELETE UNIT?");
        if(state->selected_unit_index < state->profile_count) {
            canvas_draw_str(canvas, 4, 36, state->profile_names[state->selected_unit_index]);
        }
        canvas_draw_str(canvas, 4, 47, "Remove this saved unit?");
        draw_left_arrow(canvas, 7, 57);
        canvas_draw_str(canvas, 16, 61, "KEEP");
        draw_right_arrow(canvas, 65, 57);
        canvas_draw_str(canvas, 74, 61, "DELETE");
        break;
    case PageEditFailed:
        draw_header(canvas, "EDIT FAILED");
        canvas_draw_str(canvas, 4, 36, "Could not save changes.");
        canvas_draw_str(canvas, 4, 48, "BACK: EDIT UNIT");
        break;
    case PageNoBrandProtocols:
        draw_header(canvas, "NO TESTS FOR BRAND YET");
        canvas_draw_str(canvas, 4, 36, "Learn from its remote");
        canvas_draw_str(canvas, 4, 46, "to use this unit.");
        draw_short_ok_hint(canvas, 4, 63, "LEARN REMOTE");
        canvas_draw_str(canvas, 97, 63, "BACK");
        break;
    case PageRoomProbe: {
        const size_t variant_index = state->room_probe_variants[state->room_probe_position];
        draw_header(canvas, "TRY PROTOCOL IN ROOM");
        draw_variant_name(canvas, 4, 34, variant_index);
        canvas_draw_str(canvas, 4, 43, "Sends ON: COOL 24C AUTO");
        canvas_draw_str(canvas, 4, 52, "Aim at unit.");
        draw_short_ok_hint(canvas, 4, 63, "SEND");
        canvas_draw_str(canvas, 82, 63, "BACK");
        break;
    }
    case PageRoomProbeQuestion:
        draw_header(canvas, "DID THE UNIT RESPOND?");
        canvas_draw_str(canvas, 4, 34, "Did it beep or start?");
        draw_left_arrow(canvas, 6, 46);
        canvas_draw_str(canvas, 14, 50, "NO");
        draw_right_arrow(canvas, 46, 46);
        canvas_draw_str(canvas, 54, 50, "YES");
        canvas_draw_str(canvas, 4, 63, "BACK: BRANDS");
        break;
    case PageRoomProbeSaved:
        draw_header(canvas, "PROTOCOL CONFIRMED");
        draw_variant_name(canvas, 4, 34, state->expected_variant);
        const AcProtocolVariant* variant = ac_variant_get(state->expected_variant);
        canvas_draw_str(
            canvas,
            4,
            46,
            (variant && variant->supports_state_control) ? "DIRECT CONTROL READY" :
                                                           "POWER REPLAY READY");
        draw_short_ok_hint(canvas, 4, 63, "CONTROL");
        break;
    case PageRoomProbeFailed:
        draw_header(canvas, "NO TEST RESPONDED");
        canvas_draw_str(canvas, 4, 34, "No protocol matched.");
        canvas_draw_str(canvas, 4, 46, "Learn from the original remote.");
        draw_short_ok_hint(canvas, 4, 63, "LEARN REMOTE");
        canvas_draw_str(canvas, 91, 63, "BACK");
        break;
    case PageLearnNormal:
        draw_header(canvas, "LEARN NORMAL STATE");
        canvas_draw_str(canvas, 4, 34, "Set the AC to usual settings.");
        canvas_draw_str(canvas, 4, 43, "Point remote at Flipper.");
        canvas_draw_str(canvas, 4, 52, "Choose any setting button.");
        draw_short_ok_hint(canvas, 4, 63, "LISTEN");
        canvas_draw_str(canvas, 101, 63, "BACK");
        break;
    case PageListenNormal:
        draw_header(canvas, "LISTENING FOR NORMAL");
        canvas_draw_str(canvas, 4, 34, "Point remote at Flipper.");
        canvas_draw_str(canvas, 4, 44, "Press a setting button once.");
        canvas_draw_str(canvas, 4, 54, "Wait for the full burst.");
        canvas_draw_str(canvas, 4, 63, "BACK: CANCEL");
        break;
    case PageLearnOff:
        draw_header(canvas, "LEARN OFF STATE");
        canvas_draw_str(canvas, 4, 34, "Unit should be on.");
        canvas_draw_str(canvas, 4, 43, "Point remote at Flipper.");
        canvas_draw_str(canvas, 4, 52, "Press POWER to switch it off.");
        draw_short_ok_hint(canvas, 4, 63, "LISTEN");
        canvas_draw_str(canvas, 101, 63, "BACK");
        break;
    case PageListenOff:
        draw_header(canvas, "LISTENING FOR OFF");
        canvas_draw_str(canvas, 4, 34, "Point remote at Flipper.");
        canvas_draw_str(canvas, 4, 44, "Press POWER once to turn off.");
        canvas_draw_str(canvas, 4, 54, "Wait for the full burst.");
        canvas_draw_str(canvas, 4, 63, "BACK: CANCEL");
        break;
    case PageLearnDone: {
        draw_header(canvas, "PROFILE SAVED");
        canvas_draw_str(canvas, 4, 33, "Normal and Off learned.");
        bool expected_seen = false;
        size_t learned_variant = SIZE_MAX;
        if(state->has_expected_variant) {
            for(size_t i = 0; i < state->learned_match_count; i++) {
                if(state->learned_matches[i] == state->expected_variant) {
                    expected_seen = true;
                    learned_variant = state->expected_variant;
                }
            }
        }
        if(learned_variant == SIZE_MAX && state->learned_result == AcIdentifyMatch) {
            learned_variant = state->learned_matches[0];
        }
        const AcProtocolVariant* variant =
            (learned_variant != SIZE_MAX) ? ac_variant_get(learned_variant) : NULL;
        if(variant) {
            draw_variant_name(canvas, 4, 41, learned_variant);
        } else {
            canvas_draw_str(canvas, 4, 41, "Replay only: on / off.");
        }
        if(state->has_expected_variant && !expected_seen) {
            canvas_draw_str(canvas, 4, 49, "Chosen protocol not seen.");
        } else if(variant && !variant->supports_state_control) {
            canvas_draw_str(canvas, 4, 49, "POWER REPLAY ONLY");
        }
        draw_short_ok_hint(canvas, 4, 63, "RETURN TO CONTROLS");
        break;
    }
    case PageLearnFailed:
        draw_header(canvas, "LEARNING FAILED");
        canvas_draw_str(canvas, 4, 34, "Could not save this profile.");
        canvas_draw_str(canvas, 4, 46, "BACK: RETURN TO UNITS");
        break;
    default:
        break;
    }
}

static void draw_callback(Canvas* canvas, void* context) {
    const AcMultiRemoteState* state = context;

    canvas_clear(canvas);
    if(state->page == PageControl) {
        draw_control_screen(canvas, state);
    } else if(state->page == PageSelectUnit) {
        draw_unit_screen(canvas, state);
    } else {
        draw_learning_screen(canvas, state);
    }
}

static void build_profile_path(uint8_t profile_id, char* path, size_t path_size) {
    snprintf(path, path_size, PROFILE_PATH_FORMAT, (unsigned)profile_id);
}

static void build_profile_name_path(uint8_t profile_id, char* path, size_t path_size) {
    snprintf(path, path_size, PROFILE_NAME_PATH_FORMAT, (unsigned)profile_id);
}

static void load_profile_name(Storage* storage, uint8_t profile_id, char* name, size_t name_size) {
    char path[96];
    build_profile_name_path(profile_id, path, sizeof(path));
    File* file = storage_file_alloc(storage);
    const bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    size_t bytes_read = 0U;
    if(opened && name_size > 1U) {
        bytes_read = storage_file_read(file, name, name_size - 1U);
    }
    storage_file_close(file);
    storage_file_free(file);

    if(bytes_read == 0U) {
        snprintf(name, name_size, "UNIT %02u", (unsigned)profile_id);
    } else {
        name[bytes_read] = '\0';
    }
}

static bool save_profile_name(uint8_t profile_id, const char* name) {
    char path[96];
    build_profile_name_path(profile_id, path, sizeof(path));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROFILE_DIRECTORY);
    File* file = storage_file_alloc(storage);
    const bool opened = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    const size_t name_length = strlen(name);
    const bool success = opened && name_length > 0U &&
                         storage_file_write(file, name, name_length) == name_length &&
                         storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

static void rename_text_input_done(void* context) {
    AcMultiRemoteState* state = context;
    state->rename_accepted = true;
    view_dispatcher_stop(state->rename_dispatcher);
}

static bool rename_text_input_back(void* context) {
    AcMultiRemoteState* state = context;
    state->rename_accepted = false;
    view_dispatcher_stop(state->rename_dispatcher);
    return true;
}

static bool prompt_profile_name(AcMultiRemoteState* state) {
    if(state->selected_unit_index >= state->profile_count) return false;
    const uint8_t profile_id = state->profile_ids[state->selected_unit_index];
    snprintf(
        state->rename_buffer,
        sizeof(state->rename_buffer),
        "%s",
        state->profile_names[state->selected_unit_index]);
    state->rename_accepted = false;

    state->rename_text_input = text_input_alloc();
    state->rename_dispatcher = view_dispatcher_alloc();
    text_input_set_header_text(state->rename_text_input, "Rename AC unit");
    text_input_set_minimum_length(state->rename_text_input, 1U);
    text_input_set_result_callback(
        state->rename_text_input,
        rename_text_input_done,
        state,
        state->rename_buffer,
        sizeof(state->rename_buffer),
        false);
    view_dispatcher_set_event_callback_context(state->rename_dispatcher, state);
    view_dispatcher_set_navigation_event_callback(
        state->rename_dispatcher, rename_text_input_back);
    view_dispatcher_add_view(
        state->rename_dispatcher,
        RENAME_TEXT_VIEW_ID,
        text_input_get_view(state->rename_text_input));

    gui_remove_view_port(state->gui, state->view_port);
    view_dispatcher_attach_to_gui(
        state->rename_dispatcher, state->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(state->rename_dispatcher, RENAME_TEXT_VIEW_ID);
    view_dispatcher_run(state->rename_dispatcher);

    view_dispatcher_remove_view(state->rename_dispatcher, RENAME_TEXT_VIEW_ID);
    view_dispatcher_free(state->rename_dispatcher);
    text_input_free(state->rename_text_input);
    state->rename_dispatcher = NULL;
    state->rename_text_input = NULL;
    gui_add_view_port(state->gui, state->view_port, GuiLayerFullscreen);
    view_port_update(state->view_port);

    if(!state->rename_accepted) return false;
    if(!save_profile_name(profile_id, state->rename_buffer)) {
        state->page = PageEditFailed;
        return false;
    }
    snprintf(
        state->profile_names[state->selected_unit_index],
        sizeof(state->profile_names[state->selected_unit_index]),
        "%s",
        state->rename_buffer);
    state->page = PageSelectUnit;
    return true;
}

static bool profile_signal_exists(Storage* storage, const char* path, const char* signal_name) {
    bool found = false;
    FlipperFormat* format = flipper_format_file_alloc(storage);
    InfraredSignal* signal = infrared_signal_alloc();

    if(flipper_format_file_open_existing(format, path)) {
        found = infrared_signal_search_by_name_and_read(signal, format, signal_name) ==
                InfraredErrorCodeNone;
        flipper_format_file_close(format);
    }

    infrared_signal_free(signal);
    flipper_format_free(format);
    return found;
}

static size_t load_profiles(AcMultiRemoteState* state) {
    size_t count = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROFILE_DIRECTORY);

    for(uint8_t id = 1; id <= (uint8_t)MAX_PROFILES; id++) {
        char path[96];
        build_profile_path(id, path, sizeof(path));
        if(storage_common_stat(storage, path, NULL) == FSE_OK &&
           profile_signal_exists(storage, path, "Normal") &&
           profile_signal_exists(storage, path, "Off")) {
            state->profile_ids[count] = id;
            load_profile_name(
                storage, id, state->profile_names[count], sizeof(state->profile_names[count]));
            count++;
        }
    }

    furi_record_close(RECORD_STORAGE);
    return count;
}

static uint8_t find_free_profile_id(void) {
    uint8_t free_id = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROFILE_DIRECTORY);

    for(uint8_t id = 1; id <= (uint8_t)MAX_PROFILES; id++) {
        char path[96];
        build_profile_path(id, path, sizeof(path));
        if(storage_common_stat(storage, path, NULL) == FSE_NOT_EXIST) {
            free_id = id;
            break;
        }
    }

    furi_record_close(RECORD_STORAGE);
    return free_id;
}

static bool save_signal_to_profile(
    Storage* storage,
    const char* path,
    const char* name,
    const InfraredSignal* signal,
    bool create_file) {
    bool success = false;
    FlipperFormat* format = flipper_format_file_alloc(storage);
    const bool opened = create_file ? flipper_format_file_open_new(format, path) :
                                      flipper_format_file_open_append(format, path);

    if(opened) {
        success = true;
        if(create_file) {
            success = flipper_format_write_header_cstr(format, "IR signals file", 1);
        }
        if(success) {
            success = infrared_signal_save(signal, format, name) == InfraredErrorCodeNone;
        }
        flipper_format_file_close(format);
    }

    flipper_format_free(format);
    return success;
}

/* Store Normal and Off together; remove a half-written profile on failure. */
static bool save_new_profile(AcMultiRemoteState* state) {
    bool success = false;
    char path[96];
    build_profile_path(state->pending_profile_id, path, sizeof(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROFILE_DIRECTORY);

    const bool normal_saved =
        save_signal_to_profile(storage, path, "Normal", state->normal_signal, true);
    if(normal_saved) {
        success = save_signal_to_profile(storage, path, "Off", state->capture_signal, false);
    }

    if(!success && normal_saved) {
        storage_common_remove(storage, path);
    }

    furi_record_close(RECORD_STORAGE);
    return success;
}

static bool transmit_profile_signal(uint8_t profile_id, const char* signal_name) {
    bool success = false;
    char path[96];
    build_profile_path(profile_id, path, sizeof(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* format = flipper_format_file_alloc(storage);
    InfraredSignal* signal = infrared_signal_alloc();

    if(flipper_format_file_open_existing(format, path)) {
        if(infrared_signal_search_by_name_and_read(signal, format, signal_name) ==
           InfraredErrorCodeNone) {
            infrared_signal_transmit(signal);
            success = true;
        }
        flipper_format_file_close(format);
    }

    infrared_signal_free(signal);
    flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    return success;
}

static uint8_t mhi_fan_speed_index(AcMhiVariant variant, uint8_t fan_code) {
    const uint8_t* codes = (variant == AcMhiVariant88) ? mhi_88_fan_codes : mhi_152_fan_codes;
    for(uint8_t i = 0U; i < (uint8_t)FAN_SPEED_COUNT; i++) {
        if(codes[i] == fan_code) return i;
    }
    return (uint8_t)(FAN_SPEED_COUNT - 1U);
}

/* Decode the saved Normal signal to restore editable settings. Unrecognised
 * profiles remain usable as basic Normal/Off replay without state controls. */
static bool load_profile_control_state(AcMultiRemoteState* state, uint8_t profile_id) {
    state->state_control_supported = false;
    state->mhi_supported = false;
    state->gree_supported = false;
    state->fujitsu_supported = false;
    state->lg_supported = false;
    state->lg_variant = AcLgVariantLg;
    state->lg_mode = AC_LG_MODE_COOL;
    state->fujitsu_variant = AcFujitsuVariantUnknown;
    state->mitsubishi_prefix_count = 0U;

    char path[96];
    build_profile_path(profile_id, path, sizeof(path));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* format = flipper_format_file_alloc(storage);
    InfraredSignal* signal = infrared_signal_alloc();
    bool mitsubishi_decoded = false;
    bool mitsubishi_legacy_decoded = false;
    bool mhi_decoded = false;
    bool gree_decoded = false;
    bool fujitsu_decoded = false;
    bool lg_decoded = false;
    uint32_t lg_frame = 0U;
    state->mitsubishi_legacy_variant = AcMitsubishiLegacyNone;

    if(flipper_format_file_open_existing(format, path)) {
        if(infrared_signal_search_by_name_and_read(signal, format, "Normal") ==
           InfraredErrorCodeNone) {
            mitsubishi_decoded = ac_mitsubishi_decode_profile(
                signal,
                state->mitsubishi_frame,
                state->mitsubishi_prefix,
                &state->mitsubishi_prefix_count);
            if(!mitsubishi_decoded) {
                mitsubishi_legacy_decoded = ac_mitsubishi_legacy_decode(
                    signal, &state->mitsubishi_legacy_variant, state->mitsubishi_legacy_frame);
            }
            if(!mitsubishi_decoded && !mitsubishi_legacy_decoded) {
                mhi_decoded = ac_mhi_decode_signal(signal, &state->mhi_variant, state->mhi_frame);
            }
            if(!mitsubishi_decoded && !mitsubishi_legacy_decoded && !mhi_decoded) {
                gree_decoded =
                    ac_gree_decode_signal(signal, &state->gree_model, state->gree_frame);
            }
            if(!mitsubishi_decoded && !mitsubishi_legacy_decoded && !mhi_decoded &&
               !gree_decoded) {
                fujitsu_decoded = ac_fujitsu_decode_signal(
                    signal, &state->fujitsu_variant, state->fujitsu_frame);
            }
            if(!mitsubishi_decoded && !mitsubishi_legacy_decoded && !mhi_decoded &&
               !gree_decoded && !fujitsu_decoded) {
                lg_decoded = ac_lg_decode_signal(signal, &state->lg_variant, &lg_frame);
            }
        }
        flipper_format_file_close(format);
    }

    infrared_signal_free(signal);
    flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);

    if(lg_decoded) {
        bool power_on = false;
        uint8_t mode = 0U;
        if(ac_lg_read_controls(
               lg_frame, &power_on, &state->temperature_c, &mode, &state->fan_speed)) {
            state->lg_supported = true;
            state->state_control_supported = true;
            state->power_on = power_on;
            state->lg_mode = mode;
            return true;
        }
    }

    if(mitsubishi_decoded) {
        state->state_control_supported = true;
        state->power_on = (state->mitsubishi_frame[5] & 0x20U) != 0U;
        state->temperature_c =
            (uint8_t)(MITSUBISHI_TEMP_MIN + (state->mitsubishi_frame[7] & 0x0FU));
        const uint8_t fan_code = state->mitsubishi_frame[9] & 0x07U;
        if((state->mitsubishi_frame[9] & 0x80U) || fan_code == 0U) {
            state->fan_speed = 0U;
        } else {
            state->fan_speed = (fan_code < (uint8_t)(sizeof(mitsubishi_fan_codes) /
                                                     sizeof(mitsubishi_fan_codes[0]))) ?
                                   fan_code :
                                   (uint8_t)(FAN_SPEED_COUNT - 1U);
        }
        return true;
    }

    if(mitsubishi_legacy_decoded) {
        state->state_control_supported = true;
        state->mhi_supported = false;
        uint8_t mode_code = 0U;
        return ac_mitsubishi_legacy_get_controls(
            state->mitsubishi_legacy_variant,
            state->mitsubishi_legacy_frame,
            &state->power_on,
            &state->temperature_c,
            &mode_code,
            &state->fan_speed);
    }

    if(gree_decoded) {
        state->state_control_supported = true;
        state->gree_supported = true;
        uint8_t mode_code = 0U;
        return ac_gree_get_controls(
            &state->gree_model,
            state->gree_frame,
            &state->power_on,
            &state->temperature_c,
            &mode_code,
            &state->fan_speed);
    }

    if(fujitsu_decoded) {
        /* Keep swing state out of every direct-control transmission. */
        state->fujitsu_frame[10] &= (uint8_t)~0x30U;
        ac_fujitsu_update_checksum(state->fujitsu_variant, state->fujitsu_frame);
        state->state_control_supported = true;
        state->fujitsu_supported = true;
        const uint8_t raw_temp = (state->fujitsu_frame[8] >> 2) & 0x3FU;
        state->temperature_c = state->fujitsu_variant == AcFujitsuVariantLong31 ?
                                   (uint8_t)(8U + raw_temp / 2U) :
                                   (uint8_t)(16U + raw_temp / 4U);
        state->power_on = (state->fujitsu_frame[8] & 0x01U) != 0U;
        const uint8_t fan_code = state->fujitsu_frame[10] & 0x07U;
        switch(fan_code) {
        case 0U:
            state->fan_speed = 0U;
            break;
        case 3U:
            state->fan_speed = 1U;
            break;
        case 2U:
            state->fan_speed = 2U;
            break;
        case 1U:
            state->fan_speed = 3U;
            break;
        case 4U:
            state->fan_speed = 4U;
            break;
        default:
            state->fan_speed = 0U;
            break;
        }
        if(state->temperature_c < ac_fujitsu_min_temperature()) {
            state->temperature_c = ac_fujitsu_min_temperature();
        }
        if(state->temperature_c > ac_fujitsu_max_temperature()) {
            state->temperature_c = ac_fujitsu_max_temperature();
        }
        return true;
    }

    if(mhi_decoded) {
        state->state_control_supported = true;
        state->mhi_supported = true;
        if(state->mhi_variant == AcMhiVariant88) {
            const uint8_t settings = state->mhi_frame[9];
            state->power_on = (settings & 0x08U) != 0U;
            state->mhi_mode = settings & 0x07U;
            state->temperature_c = (uint8_t)(MHI_TEMP_MIN + ((settings >> 4) & 0x0FU));
            state->fan_speed =
                mhi_fan_speed_index(state->mhi_variant, (state->mhi_frame[7] >> 5) & 0x07U);
        } else {
            state->power_on = (state->mhi_frame[5] & 0x08U) != 0U;
            state->mhi_mode = state->mhi_frame[5] & 0x07U;
            state->temperature_c = (uint8_t)(MHI_TEMP_MIN + (state->mhi_frame[7] & 0x0FU));
            state->fan_speed =
                mhi_fan_speed_index(state->mhi_variant, state->mhi_frame[9] & 0x0FU);
        }
        if(state->mhi_mode > (uint8_t)AcMhiModeHeat) {
            state->mhi_mode = (uint8_t)AcMhiModeAuto;
        }
        if(state->temperature_c > MHI_TEMP_MAX) {
            state->temperature_c = (uint8_t)MHI_TEMP_MAX;
        }
        return true;
    }

    return false;
}

static bool delete_selected_profile(AcMultiRemoteState* state) {
    if(state->selected_unit_index >= state->profile_count) return false;

    const uint8_t profile_id = state->profile_ids[state->selected_unit_index];
    const bool deleting_active = state->active_profile_id == profile_id;
    char profile_path[96];
    char name_path[96];
    build_profile_path(profile_id, profile_path, sizeof(profile_path));
    build_profile_name_path(profile_id, name_path, sizeof(name_path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error name_status = storage_common_stat(storage, name_path, NULL);
    bool success = true;
    if(name_status == FSE_OK) {
        success = storage_common_remove(storage, name_path) == FSE_OK;
    } else if(name_status != FSE_NOT_EXIST) {
        success = false;
    }
    if(success) {
        success = storage_common_remove(storage, profile_path) == FSE_OK;
    }
    furi_record_close(RECORD_STORAGE);
    if(!success) return false;

    state->profile_count = load_profiles(state);
    if(state->selected_unit_index >= state->profile_count && state->profile_count > 0U) {
        state->selected_unit_index = state->profile_count - 1U;
    }
    if(state->profile_count == 0U) state->selected_unit_index = 0U;

    if(deleting_active) {
        if(state->profile_count > 0U) {
            state->active_profile_id = state->profile_ids[0];
            if(!load_profile_control_state(state, state->active_profile_id)) {
                state->power_on = false;
            }
        } else {
            state->active_profile_id = 0U;
            state->state_control_supported = false;
            state->mhi_supported = false;
            state->power_on = false;
        }
    }

    return true;
}

static void cycle_active_unit(AcMultiRemoteState* state, bool next) {
    if(state->profile_count < 2U) return;

    size_t current_index = state->profile_count;
    for(size_t i = 0; i < state->profile_count; i++) {
        if(state->profile_ids[i] == state->active_profile_id) {
            current_index = i;
            break;
        }
    }

    if(current_index == state->profile_count) current_index = 0;

    const size_t next_index =
        next ? (current_index + 1U) % state->profile_count :
               (current_index == 0U ? state->profile_count - 1U : current_index - 1U);

    state->selected_unit_index = next_index;
    state->active_profile_id = state->profile_ids[next_index];

    if(!load_profile_control_state(state, state->active_profile_id)) {
        state->power_on = false;
    }
}

static bool transmit_mitsubishi_state(AcMultiRemoteState* state) {
    uint32_t timings[AC_MITSUBISHI_RAW_MAX];
    size_t timings_count = 0U;
    InfraredSignal* signal = infrared_signal_alloc();

    const bool built = ac_mitsubishi_build_signal(
        state->mitsubishi_frame,
        state->mitsubishi_prefix,
        state->mitsubishi_prefix_count,
        timings,
        AC_MITSUBISHI_RAW_MAX,
        &timings_count);
    if(built) {
        infrared_signal_set_raw_signal(
            signal, timings, timings_count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
        infrared_signal_transmit(signal);
    }

    infrared_signal_free(signal);
    return built;
}

static bool transmit_mhi_state(AcMultiRemoteState* state) {
    uint8_t frame[AC_MHI_MAX_STATE_BYTES];
    uint32_t timings[AC_MHI_MAX_RAW_TIMINGS];
    size_t timings_count = 0U;
    const uint8_t* fan_codes = (state->mhi_variant == AcMhiVariant88) ? mhi_88_fan_codes :
                                                                        mhi_152_fan_codes;

    if(!ac_mhi_build_state(
           state->mhi_variant,
           state->power_on,
           state->temperature_c,
           (AcMhiMode)state->mhi_mode,
           fan_codes[state->fan_speed],
           frame) ||
       !ac_mhi_build_raw_timings(
           state->mhi_variant, frame, timings, AC_MHI_MAX_RAW_TIMINGS, &timings_count)) {
        return false;
    }

    InfraredSignal* signal = infrared_signal_alloc();
    infrared_signal_set_raw_signal(
        signal, timings, timings_count, 38000U, INFRARED_COMMON_DUTY_CYCLE);
    infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    return true;
}

static bool transmit_active_state(AcMultiRemoteState* state) {
    if(state->lg_supported) {
        return ac_lg_transmit(
            state->lg_variant,
            state->power_on,
            state->temperature_c,
            state->lg_mode,
            state->fan_speed);
    }
    if(state->fujitsu_supported)
        return ac_fujitsu_transmit(state->fujitsu_variant, state->fujitsu_frame);
    if(state->mhi_supported) return transmit_mhi_state(state);
    if(state->gree_supported) return ac_gree_transmit(state->gree_model, state->gree_frame);
    if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone) {
        return ac_mitsubishi_legacy_transmit(
            state->mitsubishi_legacy_variant, state->mitsubishi_legacy_frame);
    }
    return transmit_mitsubishi_state(state);
}

static void update_active_power(AcMultiRemoteState* state, bool power_on) {
    if(state->lg_supported) {
        const bool previous = state->power_on;
        state->power_on = power_on;
        if(!transmit_active_state(state)) state->power_on = previous;
        return;
    }
    if(state->fujitsu_supported) {
        if(power_on)
            state->fujitsu_frame[8] |= 0x01U;
        else
            state->fujitsu_frame[8] &= (uint8_t)~0x01U;
        ac_fujitsu_update_checksum(state->fujitsu_variant, state->fujitsu_frame);
        if(transmit_active_state(state)) state->power_on = power_on;
        return;
    }
    if(state->mhi_supported) {
        const bool previous_power = state->power_on;
        state->power_on = power_on;
        if(!transmit_mhi_state(state)) state->power_on = previous_power;
        return;
    }

    if(state->gree_supported) {
        ac_gree_set_power(state->gree_model, state->gree_frame, power_on);
        if(transmit_active_state(state)) state->power_on = power_on;
        return;
    }

    if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone) {
        ac_mitsubishi_legacy_set_power(
            state->mitsubishi_legacy_variant, state->mitsubishi_legacy_frame, power_on);
        if(transmit_active_state(state)) state->power_on = power_on;
        return;
    }

    if(power_on) {
        state->mitsubishi_frame[5] |= 0x20U;
    } else {
        state->mitsubishi_frame[5] &= (uint8_t)~0x20U;
    }
    ac_mitsubishi_update_checksum(state->mitsubishi_frame);
    if(transmit_mitsubishi_state(state)) state->power_on = power_on;
}

static void toggle_active_mode(AcMultiRemoteState* state) {
    if(state->lg_supported) {
        const uint8_t previous = state->lg_mode;
        state->lg_mode = previous == AC_LG_MODE_COOL ? AC_LG_MODE_HEAT : AC_LG_MODE_COOL;
        if(!transmit_active_state(state)) state->lg_mode = previous;
        return;
    }
    if(state->fujitsu_supported) {
        const uint8_t current = state->fujitsu_frame[9] & 0x07U;
        const uint8_t next = current == 1U ? 4U : 1U;
        state->fujitsu_frame[9] = (uint8_t)((state->fujitsu_frame[9] & 0xF8U) | next);
        ac_fujitsu_update_checksum(state->fujitsu_variant, state->fujitsu_frame);
        transmit_active_state(state);
        return;
    }
    if(state->mhi_supported) {
        state->mhi_mode = (state->mhi_mode == (uint8_t)AcMhiModeCool) ? (uint8_t)AcMhiModeHeat :
                                                                        (uint8_t)AcMhiModeCool;
        transmit_mhi_state(state);
        return;
    }

    if(state->gree_supported) {
        ac_gree_toggle_mode(state->gree_frame);
        transmit_active_state(state);
        return;
    }

    if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone) {
        ac_mitsubishi_legacy_toggle_mode(
            state->mitsubishi_legacy_variant, state->mitsubishi_legacy_frame);
        transmit_active_state(state);
        return;
    }

    const uint8_t current_mode = (state->mitsubishi_frame[6] >> 3) & 0x07U;
    const bool switch_to_heat = current_mode == 3U;
    const uint8_t next_mode = switch_to_heat ? 1U : 3U;

    state->mitsubishi_frame[6] =
        (uint8_t)((state->mitsubishi_frame[6] & (uint8_t)~0x38U) | (next_mode << 3));
    state->mitsubishi_frame[8] =
        (uint8_t)((state->mitsubishi_frame[8] & 0xF0U) | (switch_to_heat ? 0U : 6U));
    ac_mitsubishi_update_checksum(state->mitsubishi_frame);
    transmit_mitsubishi_state(state);
}

static void start_capture(AcMultiRemoteState* state, AppPage listen_page) {
    if(!state->capture_timings) {
        state->capture_timings = malloc(AC_CAPTURE_MAX_TIMINGS * sizeof(uint32_t));
    }
    if(!state->capture_timings ||
       furi_mutex_acquire(state->capture_mutex, FuriWaitForever) != FuriStatusOk) {
        state->page = PageLearnFailed;
        app_request_redraw(state);
        return;
    }

    state->capturing = true;
    state->capture_received = false;
    state->capture_timings_count = 0U;
    state->last_raw_signal_tick = 0U;
    state->capture_last_signal_tick = 0U;
    state->capture_is_raw = false;
    state->capture_overflow = false;
    furi_mutex_release(state->capture_mutex);

    state->page = listen_page;
    infrared_worker_rx_set_received_signal_callback(
        state->infrared_worker, received_signal_callback, state);
    infrared_worker_rx_enable_signal_decoding(state->infrared_worker, true);
    infrared_worker_rx_enable_blink_on_receiving(state->infrared_worker, true);
    infrared_worker_rx_start(state->infrared_worker);
    app_request_redraw(state);
}

static void stop_capture(AcMultiRemoteState* state) {
    if(state->capturing) {
        infrared_worker_rx_stop(state->infrared_worker);
        if(furi_mutex_acquire(state->capture_mutex, FuriWaitForever) == FuriStatusOk) {
            state->capturing = false;
            furi_mutex_release(state->capture_mutex);
        }
    }
}

static bool capture_activity_snapshot(AcMultiRemoteState* state, uint32_t* last_signal_tick) {
    if(furi_mutex_acquire(state->capture_mutex, FuriWaitForever) != FuriStatusOk) return false;
    const bool received = state->capture_received;
    *last_signal_tick = state->capture_last_signal_tick;
    furi_mutex_release(state->capture_mutex);
    return received;
}

static void capture_completed(AcMultiRemoteState* state) {
    stop_capture(state);

    if(state->capture_overflow) {
        state->page = PageLearnFailed;
        app_request_redraw(state);
        return;
    }
    if(state->capture_is_raw) {
        if(state->capture_timings_count == 0U) {
            state->page = PageLearnFailed;
            app_request_redraw(state);
            return;
        }
        infrared_signal_set_raw_signal(
            state->capture_signal,
            state->capture_timings,
            state->capture_timings_count,
            INFRARED_COMMON_CARRIER_FREQUENCY,
            INFRARED_COMMON_DUTY_CYCLE);
    }

    if(state->page == PageListenNormal) {
        infrared_signal_set_signal(state->normal_signal, state->capture_signal);
        state->page = PageLearnOff;
    } else if(state->page == PageListenOff) {
        state->page = save_new_profile(state) ? PageLearnDone : PageLearnFailed;
        if(state->page == PageLearnDone) {
            state->learned_result = ac_identify_signal(
                state->normal_signal, state->learned_matches, &state->learned_match_count);
            state->profile_count = load_profiles(state);
            state->active_profile_id = state->pending_profile_id;
            if(!load_profile_control_state(state, state->active_profile_id)) {
                state->power_on = false;
            }
        }
    }

    app_request_redraw(state);
}

static void begin_new_profile(AcMultiRemoteState* state) {
    state->pending_profile_id = find_free_profile_id();
    state->room_probe_count = 0U;
    state->room_probe_position = 0U;
    state->wizard_method_index = 0U;
    state->selected_manufacturer_index = 0U;
    state->has_expected_variant = false;
    state->learned_result = AcIdentifyNoMatch;
    state->learned_match_count = 0U;
    if(state->pending_profile_id == 0U) {
        state->page = PageLearnFailed;
    } else {
        state->page = PageWizardMethod;
    }
}

static void start_room_probe(AcMultiRemoteState* state, size_t manufacturer_index) {
    state->room_probe_count = 0U;
    state->room_probe_position = 0U;
    for(size_t i = 0; i < ac_variant_count(); i++) {
        const AcProtocolVariant* variant = ac_variant_get(i);
        const AcProtocolFamily* family = ac_variant_family(i);
        const bool manufacturer_matches = manufacturer_index == SIZE_MAX ||
                                          (family && family->manufacturer == manufacturer_index);
        if(variant && family && manufacturer_matches && variant->supports_room_probe &&
           variant->build_probe_signal && state->room_probe_count < AC_ROOM_PROBE_MAX_VARIANTS) {
            state->room_probe_variants[state->room_probe_count++] = i;
        }
    }

    if(state->room_probe_count) {
        state->page = PageRoomProbe;
    } else if(manufacturer_index == SIZE_MAX) {
        state->page = PageRoomProbeFailed;
    } else {
        state->page = PageNoBrandProtocols;
    }
}

static bool transmit_room_probe(AcMultiRemoteState* state) {
    if(state->room_probe_position >= state->room_probe_count) return false;
    const AcProtocolVariant* variant =
        ac_variant_get(state->room_probe_variants[state->room_probe_position]);
    if(!variant || !variant->build_probe_signal) return false;

    InfraredSignal* signal = infrared_signal_alloc();
    const bool built = variant->build_probe_signal(true, signal);
    if(built) infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    return built;
}

/* A successful room probe saves both the tested ON command and an OFF
 * command; the saved Normal signal is decoded again on profile activation. */
static bool save_room_probe_profile(AcMultiRemoteState* state) {
    if(state->room_probe_position >= state->room_probe_count) return false;
    const size_t variant_index = state->room_probe_variants[state->room_probe_position];
    const AcProtocolVariant* variant = ac_variant_get(variant_index);
    if(!variant || !variant->build_probe_signal ||
       !variant->build_probe_signal(true, state->normal_signal) ||
       !variant->build_probe_signal(false, state->capture_signal) || !save_new_profile(state)) {
        return false;
    }

    state->expected_variant = variant_index;
    state->has_expected_variant = true;
    state->learned_result = ac_identify_signal(
        state->normal_signal, state->learned_matches, &state->learned_match_count);
    state->profile_count = load_profiles(state);
    state->active_profile_id = state->pending_profile_id;
    state->power_on = true;
    state->state_control_supported = false;
    if(!load_profile_control_state(state, state->active_profile_id)) {
        state->power_on = true;
    }
    state->page = PageRoomProbeSaved;
    return true;
}

static void room_probe_rejected(AcMultiRemoteState* state) {
    state->room_probe_position++;
    state->page = (state->room_probe_position < state->room_probe_count) ? PageRoomProbe :
                                                                           PageRoomProbeFailed;
}

/* Fallback path: learn Normal/Off from the original remote. */
static void open_learn_remote(AcMultiRemoteState* state) {
    state->has_expected_variant = false;
    state->page = PageLearnNormal;
}

static void select_wizard_item(AcMultiRemoteState* state) {
    const size_t selected = wizard_menu_selected(state);

    if(state->page == PageWizardMethod) {
        if(selected == WIZARD_METHOD_IDENTIFY) {
            state->selected_manufacturer_index = 0U;
            state->page = PageWizardSelectBrand;
        } else {
            open_learn_remote(state);
        }
    } else if(state->page == PageWizardSelectBrand) {
        if(selected < ac_manufacturer_count()) {
            start_room_probe(state, selected);
        } else {
            start_room_probe(state, SIZE_MAX);
        }
    }
}

static void move_wizard_selection(AcMultiRemoteState* state, bool down) {
    size_t* selection = wizard_menu_selection(state);
    const size_t count = wizard_menu_count(state);
    if(!selection || count == 0U) return;

    if(down) {
        *selection = (*selection + 1U) % count;
    } else {
        *selection = (*selection == 0U) ? count - 1U : *selection - 1U;
    }
}

static void choose_selected_unit(AcMultiRemoteState* state) {
    if(state->selected_unit_index < state->profile_count) {
        state->active_profile_id = state->profile_ids[state->selected_unit_index];
        if(!load_profile_control_state(state, state->active_profile_id)) {
            state->power_on = false;
        }
        state->page = PageControl;
    }
}

static void select_edit_menu_item(AcMultiRemoteState* state) {
    if(state->edit_menu_index == 0U) {
        prompt_profile_name(state);
    } else {
        state->page = PageDeleteConfirm;
    }
}

static void handle_single_ok(AcMultiRemoteState* state) {
    switch(state->page) {
    case PageRoomProbe:
        if(transmit_room_probe(state)) {
            state->page = PageRoomProbeQuestion;
        } else {
            state->page = PageRoomProbeFailed;
        }
        break;
    case PageRoomProbeQuestion:
        break;
    case PageRoomProbeSaved:
        state->page = PageControl;
        break;
    case PageRoomProbeFailed:
    case PageNoBrandProtocols:
        open_learn_remote(state);
        break;
    case PageControl:
        if(state->active_profile_id && state->state_control_supported) {
            update_active_power(state, !state->power_on);
        } else if(state->active_profile_id) {
            const char* signal_name = state->power_on ? "Off" : "Normal";
            if(transmit_profile_signal(state->active_profile_id, signal_name)) {
                state->power_on = !state->power_on;
            }
        } else {
            state->preview_power_on = !state->preview_power_on;
        }
        break;
    case PageSelectUnit:
        choose_selected_unit(state);
        break;
    case PageEditUnit:
        select_edit_menu_item(state);
        break;
    case PageEditFailed:
        state->page = PageEditUnit;
        break;
    case PageDeleteConfirm:
        break;
    case PageWizardMethod:
    case PageWizardSelectBrand:
        select_wizard_item(state);
        break;
    case PageLearnNormal:
        start_capture(state, PageListenNormal);
        break;
    case PageLearnOff:
        start_capture(state, PageListenOff);
        break;
    case PageLearnDone:
        state->page = PageControl;
        break;
    case PageListenNormal:
    case PageListenOff:
    case PageLearnFailed:
    default:
        break;
    }
}

static void handle_back(AcMultiRemoteState* state) {
    switch(state->page) {
    case PageControl:
        state->running = false;
        break;
    case PageSelectUnit:
        state->page = PageControl;
        break;
    case PageEditUnit:
        state->page = PageSelectUnit;
        break;
    case PageDeleteConfirm:
        state->page = PageEditUnit;
        break;
    case PageEditFailed:
        state->page = PageEditUnit;
        break;
    case PageWizardMethod:
        state->page = PageSelectUnit;
        break;
    case PageRoomProbe:
    case PageRoomProbeQuestion:
    case PageRoomProbeFailed:
    case PageNoBrandProtocols:
        state->page = PageWizardSelectBrand;
        break;
    case PageRoomProbeSaved:
        state->page = PageControl;
        break;
    case PageWizardSelectBrand:
        state->page = PageWizardMethod;
        break;
    case PageListenNormal:
        stop_capture(state);
        state->page = PageLearnNormal;
        break;
    case PageListenOff:
        stop_capture(state);
        state->page = PageLearnOff;
        break;
    case PageLearnNormal:
    case PageLearnOff:
    case PageLearnFailed:
        state->page = PageSelectUnit;
        break;
    case PageLearnDone:
        state->page = PageControl;
        break;
    }
}

static void handle_input(AcMultiRemoteState* state, const InputEvent* event) {
    if(event->key == InputKeyBack) {
        handle_back(state);
        return;
    }

    if(event->key == InputKeyOk) {
        handle_single_ok(state);
        return;
    }

    if(state->page == PageDeleteConfirm) {
        if(event->key == InputKeyLeft) {
            state->page = PageEditUnit;
        } else if(event->key == InputKeyRight) {
            state->page = delete_selected_profile(state) ? PageSelectUnit : PageEditFailed;
        }
        return;
    }

    if(state->page == PageEditUnit) {
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            state->edit_menu_index = 1U - state->edit_menu_index;
        }
        return;
    }

    if(state->page == PageRoomProbeQuestion) {
        if(event->key == InputKeyLeft) {
            room_probe_rejected(state);
        } else if(event->key == InputKeyRight) {
            state->page = save_room_probe_profile(state) ? PageRoomProbeSaved : PageLearnFailed;
        }
        return;
    }

    if(state->page == PageControl && state->state_control_supported) {
        bool changed = false;
        const uint8_t minimum_temperature =
            state->lg_supported ?
                16U :
            state->mhi_supported ?
                (uint8_t)MHI_TEMP_MIN :
                (state->fujitsu_supported ?
                     ac_fujitsu_min_temperature() :
                     (state->gree_supported ?
                          16U :
                          (state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone ?
                               ac_mitsubishi_legacy_min_temperature(
                                   state->mitsubishi_legacy_variant) :
                               (uint8_t)MITSUBISHI_TEMP_MIN)));
        const uint8_t maximum_temperature =
            state->lg_supported ?
                30U :
            state->mhi_supported ?
                (uint8_t)MHI_TEMP_MAX :
                (state->fujitsu_supported ?
                     ac_fujitsu_max_temperature() :
                     (state->gree_supported ?
                          30U :
                          (state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone ?
                               ac_mitsubishi_legacy_max_temperature(
                                   state->mitsubishi_legacy_variant) :
                               (uint8_t)MITSUBISHI_TEMP_MAX)));
        const uint8_t fan_count = active_fan_count(state);
        switch(event->key) {
        case InputKeyUp:
            if(state->temperature_c < maximum_temperature) {
                state->temperature_c++;
                if(state->gree_supported) {
                    ac_gree_set_temperature(state->gree_frame, state->temperature_c);
                } else if(state->fujitsu_supported) {
                    const uint8_t temp_field = state->fujitsu_variant == AcFujitsuVariantLong31 ?
                                                   (uint8_t)((state->temperature_c - 8U) * 2U) :
                                                   (uint8_t)((state->temperature_c - 16U) * 4U);
                    state->fujitsu_frame[8] =
                        (uint8_t)((state->fujitsu_frame[8] & 0x03U) | (temp_field << 2));
                    ac_fujitsu_update_checksum(state->fujitsu_variant, state->fujitsu_frame);
                } else if(!state->mhi_supported && !state->lg_supported) {
                    if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone) {
                        ac_mitsubishi_legacy_set_temperature(
                            state->mitsubishi_legacy_variant,
                            state->mitsubishi_legacy_frame,
                            state->temperature_c);
                    } else {
                        state->mitsubishi_frame[7] =
                            (uint8_t)((state->mitsubishi_frame[7] & 0xE0U) |
                                      (state->temperature_c - MITSUBISHI_TEMP_MIN));
                    }
                }
                changed = true;
            }
            break;
        case InputKeyDown:
            if(state->temperature_c > minimum_temperature) {
                state->temperature_c--;
                if(state->gree_supported) {
                    ac_gree_set_temperature(state->gree_frame, state->temperature_c);
                } else if(state->fujitsu_supported) {
                    const uint8_t temp_field = state->fujitsu_variant == AcFujitsuVariantLong31 ?
                                                   (uint8_t)((state->temperature_c - 8U) * 2U) :
                                                   (uint8_t)((state->temperature_c - 16U) * 4U);
                    state->fujitsu_frame[8] =
                        (uint8_t)((state->fujitsu_frame[8] & 0x03U) | (temp_field << 2));
                    ac_fujitsu_update_checksum(state->fujitsu_variant, state->fujitsu_frame);
                } else if(!state->mhi_supported && !state->lg_supported) {
                    if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone) {
                        ac_mitsubishi_legacy_set_temperature(
                            state->mitsubishi_legacy_variant,
                            state->mitsubishi_legacy_frame,
                            state->temperature_c);
                    } else {
                        state->mitsubishi_frame[7] =
                            (uint8_t)((state->mitsubishi_frame[7] & 0xE0U) |
                                      (state->temperature_c - MITSUBISHI_TEMP_MIN));
                    }
                }
                changed = true;
            }
            break;
        case InputKeyLeft:
            if(state->fan_speed > 0U) {
                state->fan_speed--;
                changed = true;
            }
            break;
        case InputKeyRight:
            if(state->fan_speed + 1U < fan_count) {
                state->fan_speed++;
                changed = true;
            }
            break;
        default:
            break;
        }

        if(changed) {
            if(state->lg_supported) {
                transmit_active_state(state);
            } else if(state->fujitsu_supported) {
                static const uint8_t fujitsu_fan_codes[5] = {0U, 3U, 2U, 1U, 4U};
                state->fujitsu_frame[10] = (uint8_t)((state->fujitsu_frame[10] & 0xF8U) |
                                                     fujitsu_fan_codes[state->fan_speed]);
                /* Swing bits are deliberately never set or toggled. */
                state->fujitsu_frame[10] &= (uint8_t)~0x30U;
                ac_fujitsu_update_checksum(state->fujitsu_variant, state->fujitsu_frame);
                transmit_active_state(state);
            } else if(state->mhi_supported) {
                transmit_mhi_state(state);
            } else if(state->gree_supported) {
                ac_gree_set_fan(state->gree_frame, state->fan_speed);
                transmit_active_state(state);
            } else if(state->mitsubishi_legacy_variant != AcMitsubishiLegacyNone) {
                ac_mitsubishi_legacy_set_fan(
                    state->mitsubishi_legacy_variant,
                    state->mitsubishi_legacy_frame,
                    state->fan_speed);
                transmit_active_state(state);
            } else {
                const uint8_t fan_code = mitsubishi_fan_codes[state->fan_speed];
                state->mitsubishi_frame[9] =
                    (uint8_t)((state->mitsubishi_frame[9] & 0x78U) | (fan_code & 0x07U));
                if(fan_code == 0U) {
                    state->mitsubishi_frame[9] |= 0x80U;
                } else {
                    state->mitsubishi_frame[9] &= (uint8_t)~0x80U;
                }
                ac_mitsubishi_update_checksum(state->mitsubishi_frame);
                transmit_mitsubishi_state(state);
            }
        }
    } else if(state->page == PageControl && !state->active_profile_id) {
        switch(event->key) {
        case InputKeyUp:
            if(state->temperature_c < 30) state->temperature_c++;
            break;
        case InputKeyDown:
            if(state->temperature_c > 16) state->temperature_c--;
            break;
        case InputKeyLeft:
            if(state->fan_speed > 0) state->fan_speed--;
            break;
        case InputKeyRight:
            if(state->fan_speed < (uint8_t)(FAN_SPEED_COUNT - 1U)) state->fan_speed++;
            break;
        default:
            break;
        }
    } else if(state->page == PageSelectUnit) {
        if(event->key == InputKeyDown) {
            begin_new_profile(state);
        } else if(state->profile_count > 0 && event->key == InputKeyRight) {
            state->selected_unit_index = (state->selected_unit_index + 1U) % state->profile_count;
        } else if(state->profile_count > 0 && event->key == InputKeyLeft) {
            state->selected_unit_index = (state->selected_unit_index == 0U) ?
                                             state->profile_count - 1U :
                                             state->selected_unit_index - 1U;
        }
    } else if(state->page == PageWizardMethod || state->page == PageWizardSelectBrand) {
        if(event->key == InputKeyDown)
            move_wizard_selection(state, true);
        else if(event->key == InputKeyUp)
            move_wizard_selection(state, false);
    }
}

static void handle_input_event(AcMultiRemoteState* state, const InputEvent* input) {
    if(input->type == InputTypeLong) {
        if(state->page == PageControl) {
            if(input->key == InputKeyLeft) {
                cycle_active_unit(state, false);
                return;
            }
            if(input->key == InputKeyRight) {
                cycle_active_unit(state, true);
                return;
            }
            if(input->key == InputKeyOk && state->state_control_supported) {
                toggle_active_mode(state);
                return;
            }
        } else if(input->key == InputKeyOk && state->page == PageSelectUnit) {
            if(state->profile_count > 0U) {
                state->edit_menu_index = 0U;
                state->page = PageEditUnit;
            }
        } else if(input->key == InputKeyOk) {
            handle_single_ok(state);
        }

        return;
    }

    if(input->key == InputKeyOk && (state->page == PageControl || state->page == PageSelectUnit)) {
        return;
    }

    handle_input(state, input);
}

int32_t gbhobbytech_ac_multi_remote_app(void* p) {
    UNUSED(p);

    AcMultiRemoteState state = {
        .event_queue = furi_message_queue_alloc(EVENT_QUEUE_SIZE, sizeof(AppEvent)),
        .capture_mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .infrared_worker = infrared_worker_alloc(),
        .capture_signal = infrared_signal_alloc(),
        .normal_signal = infrared_signal_alloc(),
        .mode = KeypadModeControl,
        .page = PageControl,
        .temperature_c = 22,
        .fan_speed = 0,
        .power_on = false,
        .preview_power_on = false,
        .capturing = false,
        .capture_received = false,
        .running = true,
    };

    state.profile_count = load_profiles(&state);
    if(state.profile_count > 0) {
        state.active_profile_id = state.profile_ids[0];
        load_profile_control_state(&state, state.active_profile_id);
    }

    ViewPort* view_port = view_port_alloc();
    state.view_port = view_port;
    view_port_draw_callback_set(view_port, draw_callback, &state);
    view_port_input_callback_set(view_port, input_callback, &state);

    Gui* gui = furi_record_open(RECORD_GUI);
    state.gui = gui;
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    view_port_update(view_port);

    bool ok_pending = false;
    uint32_t ok_tick = 0;
    const uint32_t double_click_ticks = furi_ms_to_ticks(OK_DOUBLE_CLICK_MS);
    const uint32_t capture_quiet_ticks = furi_ms_to_ticks(AC_CAPTURE_QUIET_TIMEOUT_MS);

    while(state.running) {
        AppEvent event;
        uint32_t wait_ticks = FuriWaitForever;

        if(ok_pending) {
            const uint32_t elapsed = furi_get_tick() - ok_tick;
            wait_ticks = (elapsed < double_click_ticks) ? (double_click_ticks - elapsed) : 0;
        }

        if(state.page == PageListenNormal || state.page == PageListenOff) {
            uint32_t last_signal_tick = 0U;
            if(capture_activity_snapshot(&state, &last_signal_tick)) {
                const uint32_t elapsed = furi_get_tick() - last_signal_tick;
                const uint32_t capture_wait =
                    elapsed < capture_quiet_ticks ? capture_quiet_ticks - elapsed : 0U;
                if(wait_ticks == FuriWaitForever || capture_wait < wait_ticks) {
                    wait_ticks = capture_wait;
                }
            }
        }

        const FuriStatus status = furi_message_queue_get(state.event_queue, &event, wait_ticks);
        if(status == FuriStatusOk) {
            if(event.type == AppEventTypeSignalCaptured) {
                if(state.page == PageListenNormal || state.page == PageListenOff) {
                    app_request_redraw(&state);
                }
            } else if(
                event.input.key == InputKeyOk && event.input.type == InputTypeShort &&
                (state.page == PageControl || state.page == PageSelectUnit)) {
                const uint32_t now = furi_get_tick();
                if(ok_pending && ((now - ok_tick) < double_click_ticks)) {
                    if(state.page == PageControl) {
                        state.page = PageSelectUnit;
                        state.mode = KeypadModeSelectUnit;
                        state.selected_unit_index = 0;
                        for(size_t i = 0; i < state.profile_count; i++) {
                            if(state.profile_ids[i] == state.active_profile_id) {
                                state.selected_unit_index = i;
                                break;
                            }
                        }
                    } else {
                        state.page = PageControl;
                        state.mode = KeypadModeControl;
                    }
                    ok_pending = false;
                } else {
                    if(ok_pending) handle_single_ok(&state);
                    ok_pending = true;
                    ok_tick = now;
                }
            } else {
                if(ok_pending) {
                    handle_single_ok(&state);
                    ok_pending = false;
                }
                handle_input_event(&state, &event.input);
            }
            app_request_redraw(&state);
        } else {
            if(state.page == PageListenNormal || state.page == PageListenOff) {
                uint32_t last_signal_tick = 0U;
                if(capture_activity_snapshot(&state, &last_signal_tick) &&
                   (furi_get_tick() - last_signal_tick) >= capture_quiet_ticks) {
                    capture_completed(&state);
                    app_request_redraw(&state);
                }
            }
            if(ok_pending) {
                handle_single_ok(&state);
                ok_pending = false;
                app_request_redraw(&state);
            }
        }
    }

    stop_capture(&state);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    infrared_signal_free(state.normal_signal);
    infrared_signal_free(state.capture_signal);
    infrared_worker_free(state.infrared_worker);
    furi_mutex_free(state.capture_mutex);
    furi_message_queue_free(state.event_queue);
    free(state.capture_timings);

    return 0;
}
