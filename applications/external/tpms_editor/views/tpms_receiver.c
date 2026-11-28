#include "tpms_receiver.h"
#include "../tpms_app_i.h"
#include <tpmse_icons.h>
#include <math.h>

#include <input/input.h>
#include <gui/elements.h>
#include <m-array.h>

#define TAG "TPMSReceiver"

#define FRAME_HEIGHT 12
#define MAX_LEN_PX 112
#define MENU_ITEMS 4u
#define UNLOCK_CNT 3

#define SUBGHZ_RAW_THRESHOLD_MIN -100.0f
#define TPMS_RSSI_BAR_WIDTH 28U
typedef struct {
    FuriString* item_str;
    uint8_t type;
} TPMSReceiverMenuItem;

ARRAY_DEF(TPMSReceiverMenuItemArray, TPMSReceiverMenuItem, M_POD_OPLIST)

#define M_OPL_TPMSReceiverMenuItemArray_t() ARRAY_OPLIST(TPMSReceiverMenuItemArray, M_POD_OPLIST)

struct TPMSReceiverHistory {
    TPMSReceiverMenuItemArray_t data;
};

typedef struct TPMSReceiverHistory TPMSReceiverHistory;

// static const Icon* ReceiverItemIcons[] = {
//     [SubGhzProtocolTypeUnknown] = &I_Quest_7x8,
//     [SubGhzProtocolTypeStatic] = &I_Unlock_7x8,
//     [SubGhzProtocolTypeDynamic] = &I_Lock_7x8,
//     //[SubGhzProtocolWeatherStation] = &I_station_icon,
// };

typedef enum {
    TPMSReceiverBarShowDefault,
    TPMSReceiverBarShowLock,
    TPMSReceiverBarShowToUnlockPress,
    TPMSReceiverBarShowUnlock,
} TPMSReceiverBarShow;

struct TPMSReceiver {
    TPMSLock lock;
    uint8_t lock_count;
    FuriTimer* lock_timer;
    FuriTimer* relearn_timer;
    FuriThread* relearn_thread;
    bool relearn_thread_started;
    volatile bool relearn_stop_requested;
    TPMSFordLFProfile relearn_ford_profile;
    uint32_t relearn_ford_duration_ms;
    uint32_t relearn_cw_frequency_hz;
    TPMSRelearnType relearn_last_type;
    bool relearn_repeat_available;
    bool relearn_enabled;
    volatile bool relearn_active;
    View* view;
    TPMSReceiverCallback callback;
    void* context;
};

typedef struct {
    FuriString* frequency_str;
    FuriString* preset_str;
    FuriString* history_stat_str;
    TPMSReceiverHistory* history;
    uint16_t idx;
    uint16_t list_offset;
    uint16_t history_item;
    TPMSReceiverBarShow bar_show;
    uint8_t u_rssi;
    int16_t rssi_dbm;
    int16_t rssi_max_dbm;
    bool rssi_valid;
    bool rssi_max_valid;
    bool external_radio;
    bool relearn_active;
    bool relearn_el50448;
    bool relearn_ford;
    bool relearn_auto_rx;
    bool relearn_repeat_available;
    bool relearn_enabled;
} TPMSReceiverModel;

void tpms_view_receiver_set_rssi(TPMSReceiver* instance, float rssi) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        TPMSReceiverModel * model,
        {
            const bool valid = rssi > -127.0f && rssi <= 20.0f;
            if(!valid) {
                model->u_rssi = 0U;
                model->rssi_valid = false;
            } else {
                const int16_t dbm =
                    (int16_t)(rssi < 0.0f ? rssi - 0.5f : rssi + 0.5f);
                model->rssi_dbm = dbm;
                model->rssi_valid = true;
                if(!model->rssi_max_valid || dbm > model->rssi_max_dbm) {
                    model->rssi_max_dbm = dbm;
                    model->rssi_max_valid = true;
                }

                int16_t level = dbm - (int16_t)SUBGHZ_RAW_THRESHOLD_MIN;
                if(level < 0) level = 0;
                if(level > 70) level = 70;
                model->u_rssi = (uint8_t)((level * TPMS_RSSI_BAR_WIDTH) / 70);
            }
        },
        true);
}

void tpms_view_receiver_reset_rssi(TPMSReceiver* instance) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        TPMSReceiverModel * model,
        {
            model->u_rssi = 0U;
            model->rssi_dbm = -127;
            model->rssi_max_dbm = -127;
            model->rssi_valid = false;
            model->rssi_max_valid = false;
        },
        true);
}

void tpms_view_receiver_set_lock(TPMSReceiver* tpms_receiver, TPMSLock lock) {
    furi_assert(tpms_receiver);
    tpms_receiver->lock_count = 0;
    if(lock == TPMSLockOn) {
        tpms_receiver->lock = lock;
        with_view_model(
            tpms_receiver->view,
            TPMSReceiverModel * model,
            { model->bar_show = TPMSReceiverBarShowLock; },
            true);
        furi_timer_start(tpms_receiver->lock_timer, furi_ms_to_ticks(1000));
    } else {
        with_view_model(
            tpms_receiver->view,
            TPMSReceiverModel * model,
            { model->bar_show = TPMSReceiverBarShowDefault; },
            true);
    }
}


void tpms_view_receiver_set_auto_rx(TPMSReceiver* tpms_receiver, bool auto_rx) {
    furi_assert(tpms_receiver);
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        { model->relearn_auto_rx = auto_rx; },
        true);
}


void tpms_view_receiver_set_relearn_enabled(TPMSReceiver* tpms_receiver, bool enabled) {
    furi_assert(tpms_receiver);
    tpms_receiver->relearn_enabled = enabled;
    tpms_receiver->relearn_repeat_available = false;
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            model->relearn_enabled = enabled;
            model->relearn_repeat_available = false;
        },
        true);
}

void tpms_view_receiver_set_ford_profile(
    TPMSReceiver* tpms_receiver,
    TPMSFordLFProfile profile) {
    furi_assert(tpms_receiver);
    if(profile >= TPMSFordLFProfileCount) profile = TPMSFordLFProfileAuto;
    tpms_receiver->relearn_ford_profile = profile;
}

void tpms_view_receiver_set_ford_duration(
    TPMSReceiver* tpms_receiver,
    uint32_t duration_ms) {
    furi_assert(tpms_receiver);
    if(duration_ms < 500U) duration_ms = TPMS_FORD_EL50449_DURATION_MS;
    tpms_receiver->relearn_ford_duration_ms = duration_ms;
}

void tpms_view_receiver_set_cw_frequency(
    TPMSReceiver* tpms_receiver,
    uint32_t frequency_hz) {
    furi_assert(tpms_receiver);
    if(frequency_hz != TPMS_LF_CARRIER_1342_HZ) frequency_hz = TPMS_LF_CARRIER_HZ;
    tpms_receiver->relearn_cw_frequency_hz = frequency_hz;
}

bool tpms_view_receiver_relearn_is_active(TPMSReceiver* tpms_receiver) {
    furi_assert(tpms_receiver);
    return tpms_receiver->relearn_active;
}

void tpms_view_receiver_set_callback(
    TPMSReceiver* tpms_receiver,
    TPMSReceiverCallback callback,
    void* context) {
    furi_assert(tpms_receiver);
    furi_assert(callback);
    tpms_receiver->callback = callback;
    tpms_receiver->context = context;
}

static void tpms_view_receiver_update_offset(TPMSReceiver* tpms_receiver) {
    furi_assert(tpms_receiver);

    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            size_t history_item = model->history_item;
            uint16_t bounds = history_item > 3 ? 2 : history_item;

            if(history_item > 3 && model->idx >= (int16_t)(history_item - 1)) {
                model->list_offset = model->idx - 3;
            } else if(model->list_offset < model->idx - bounds) {
                model->list_offset =
                    CLAMP(model->list_offset + 1, (int16_t)(history_item - bounds), 0);
            } else if(model->list_offset > model->idx - bounds) {
                model->list_offset = CLAMP(model->idx - 1, (int16_t)(history_item - bounds), 0);
            }
        },
        true);
}

void tpms_view_receiver_add_item_to_menu(
    TPMSReceiver* tpms_receiver,
    const char* name,
    uint8_t type) {
    furi_assert(tpms_receiver);
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            TPMSReceiverMenuItem* item_menu =
                TPMSReceiverMenuItemArray_push_raw(model->history->data);
            item_menu->item_str = furi_string_alloc_set(name);
            item_menu->type = type;
            if((model->idx == model->history_item - 1)) {
                model->history_item++;
                model->idx++;
            } else {
                model->history_item++;
            }
        },
        true);
    tpms_view_receiver_update_offset(tpms_receiver);
}

void tpms_view_receiver_update_item(
    TPMSReceiver* tpms_receiver,
    uint16_t idx,
    const char* name,
    uint8_t type) {
    furi_assert(tpms_receiver);
    furi_assert(name);
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            if(idx < TPMSReceiverMenuItemArray_size(model->history->data)) {
                TPMSReceiverMenuItem* item =
                    TPMSReceiverMenuItemArray_get(model->history->data, idx);
                furi_string_set_str(item->item_str, name);
                item->type = type;
            }
        },
        true);
}

void tpms_view_receiver_add_data_statusbar(
    TPMSReceiver* tpms_receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external) {
    furi_assert(tpms_receiver);
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            furi_string_set_str(model->frequency_str, frequency_str);
            furi_string_set_str(model->preset_str, preset_str);
            furi_string_set_str(model->history_stat_str, history_stat_str);
            model->external_radio = external;
        },
        true);
}

static void tpms_view_receiver_draw_frame(Canvas* canvas, uint16_t idx, bool scrollbar) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0 + idx * FRAME_HEIGHT, scrollbar ? 122 : 127, FRAME_HEIGHT);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, 0, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 1, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 1);

    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 11);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, (0 + idx * FRAME_HEIGHT) + 11);
}

static void tpms_view_rssi_draw(Canvas* canvas, TPMSReceiverModel* model) {
    char buffer[32];
    /* Compact live bar plus numeric LIVE/MAX. Bottom line continues to show
       the current frequency and modulation, which matters in AUTO mode. */
    canvas_draw_frame(canvas, 45, 48, TPMS_RSSI_BAR_WIDTH + 3U, 5);
    if(model->u_rssi > 0U) {
        canvas_draw_box(canvas, 47, 50, model->u_rssi, 1);
    }
    if(model->rssi_valid) {
        if(model->rssi_max_valid) {
            snprintf(
                buffer, sizeof(buffer), "R%d M%d", (int)model->rssi_dbm, (int)model->rssi_max_dbm);
        } else {
            snprintf(buffer, sizeof(buffer), "R%d", (int)model->rssi_dbm);
        }
    } else {
        snprintf(buffer, sizeof(buffer), "R--- M---");
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 53, AlignRight, AlignBottom, buffer);
}

void tpms_view_receiver_draw(Canvas* canvas, TPMSReceiverModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    elements_button_left(canvas, "Config");

    bool scrollbar = model->history_item > 4;
    FuriString* str_buff;
    str_buff = furi_string_alloc();

    TPMSReceiverMenuItem* item_menu;

    for(size_t i = 0; i < MIN(model->history_item, MENU_ITEMS); ++i) {
        size_t idx = CLAMP((uint16_t)(i + model->list_offset), model->history_item, 0);
        item_menu = TPMSReceiverMenuItemArray_get(model->history->data, idx);
        furi_string_set(str_buff, item_menu->item_str);
        elements_string_fit_width(canvas, str_buff, scrollbar ? MAX_LEN_PX - 6 : MAX_LEN_PX);
        if(model->idx == idx) {
            tpms_view_receiver_draw_frame(canvas, i, scrollbar);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        // canvas_draw_icon(canvas, 4, 2 + i * FRAME_HEIGHT, ReceiverItemIcons[item_menu->type]);
        canvas_draw_str(canvas, 4, 9 + i * FRAME_HEIGHT, furi_string_get_cstr(str_buff));
        furi_string_reset(str_buff);
    }
    if(scrollbar) {
        elements_scrollbar_pos(canvas, 128, 0, 49, model->idx, model->history_item);
    }
    furi_string_free(str_buff);

    canvas_set_color(canvas, ColorBlack);

    if(model->history_item == 0) {
        canvas_draw_icon(
            canvas, 0, 0, model->external_radio ? &I_Fishing_123x52 : &I_Scanning_123x52);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 63, 46, "Scanning...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 44, 10, model->external_radio ? "Ext" : "Int");
        canvas_draw_str(
            canvas,
            48,
            9,
            model->relearn_active ?
                (model->relearn_ford ?
                     "FORD LF+RX" :
                     (model->relearn_el50448 ? "EL50448+RX" : "CW LF + RX")) :
                (model->relearn_repeat_available ?
                     "RIGHT/OK: REPEAT" :
                     (model->relearn_auto_rx ?
                          "AUTO RX" :
                          (model->relearn_enabled ? "RIGHT: RELEARN" : ""))));
    }

    // Draw RSSI
    tpms_view_rssi_draw(canvas, model);

    switch(model->bar_show) {
    case TPMSReceiverBarShowLock:
        canvas_draw_icon(canvas, 64, 55, &I_Lock_7x8);
        canvas_draw_str(canvas, 74, 62, "Locked");
        break;
    case TPMSReceiverBarShowToUnlockPress:
        canvas_draw_str(canvas, 44, 62, furi_string_get_cstr(model->frequency_str));
        canvas_draw_str(canvas, 79, 62, furi_string_get_cstr(model->preset_str));
        canvas_draw_str(canvas, 96, 62, furi_string_get_cstr(model->history_stat_str));
        canvas_set_font(canvas, FontSecondary);
        elements_bold_rounded_frame(canvas, 14, 8, 99, 48);
        elements_multiline_text(canvas, 65, 26, "To unlock\npress:");
        canvas_draw_icon(canvas, 65, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 80, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 95, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 16, 13, &I_WarningDolphin_45x42);
        canvas_draw_dot(canvas, 17, 61);
        break;
    case TPMSReceiverBarShowUnlock:
        canvas_draw_icon(canvas, 64, 55, &I_Unlock_7x8);
        canvas_draw_str(canvas, 74, 62, "Unlocked");
        break;
    default:
        canvas_draw_str(canvas, 44, 62, furi_string_get_cstr(model->frequency_str));
        canvas_draw_str(canvas, 79, 62, furi_string_get_cstr(model->preset_str));
        canvas_draw_str(canvas, 96, 62, furi_string_get_cstr(model->history_stat_str));
        break;
    }
}

static void tpms_view_receiver_lock_timer_callback(void* context) {
    furi_assert(context);
    TPMSReceiver* tpms_receiver = context;
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        { model->bar_show = TPMSReceiverBarShowDefault; },
        true);
    if(tpms_receiver->lock_count < UNLOCK_CNT) {
        tpms_receiver->callback(TPMSCustomEventViewReceiverOffDisplay, tpms_receiver->context);
    } else {
        tpms_receiver->lock = TPMSLockOff;
        tpms_receiver->callback(TPMSCustomEventViewReceiverUnlock, tpms_receiver->context);
    }
    tpms_receiver->lock_count = 0;
}

static inline void tpms_ford_lf_half_symbol(bool burst_on) {
    /*
     * One EL-50449 half-cell is nominally 128 us.
     * With the RFID timer at 125 kHz and 12.5% duty, an ON half-cell contains
     * exactly 16 drive pulses: ~1 us HIGH + ~7 us LOW per 8 us carrier cycle.
     * An OFF half-cell is 128 us of silence.
     *
     * Consecutive ON/OFF half-cells naturally merge into the K/K'/M/M'/L
     * pulse blocks measured from the original EL-50449.
     */
    if(burst_on) {
        furi_hal_rfid_tim_read_continue();
    } else {
        furi_hal_rfid_tim_read_pause();
    }
    furi_delay_us(TPMS_FORD_EL50449_HALF_BIT_US);
}

static void tpms_ford_lf_manchester_bit(bool bit) {
    /*
     * EL-50449 capture polarity (after the fixed sync):
     *   data 0 -> ON / OFF
     *   data 1 -> OFF / ON
     *
     * This polarity reproduces the measured K/K'/M/M' sequence for
     * 5A 5A AB AA. It is intentionally opposite to the first V3.2/V3.3
     * experiment.
     */
    if(bit) {
        tpms_ford_lf_half_symbol(false);
        tpms_ford_lf_half_symbol(true);
    } else {
        tpms_ford_lf_half_symbol(true);
        tpms_ford_lf_half_symbol(false);
    }
}

static void tpms_ford_lf_byte(uint8_t value) {
    for(uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
        tpms_ford_lf_manchester_bit((value & mask) != 0U);
    }
}

static void tpms_ford_lf_frame(TPMSFordLFProfile profile) {
    static const bool sync[] = {
        true, true, true, false, false, false, true, false, true,
        true, false, false, true, true, false, false, true, false,
    };
    static const uint8_t ford_5a5a[] = {0x5AU, 0x5AU, 0xABU, 0xAAU};
    static const uint8_t ford_vdo[] = {0x61U, 0x5EU, 0x13U, 0xC6U, 0x6CU, 0x39U};

    /* Only the short telegram is scheduler-locked. Interrupts remain enabled,
       so the already-running CC1101 async RX can still capture a reply. */
    const int32_t kernel_state = furi_kernel_lock();

    /* 17-bit preamble measured on EL-50449: 17 x K.
       K = one 128 us burst (16 x 8 us carrier periods) followed by
       one 128 us silent half-cell. This is deliberately emitted as the
       physical ON/OFF K symbol, not through the Manchester data mapper. */
    for(uint8_t i = 0U; i < 17U; i++) {
        tpms_ford_lf_half_symbol(true);
        tpms_ford_lf_half_symbol(false);
    }

    /* Fixed 18 half-symbol synchronization pattern; it intentionally contains
       illegal Manchester pairs 11 and 00. */
    for(size_t i = 0U; i < sizeof(sync) / sizeof(sync[0]); i++) {
        tpms_ford_lf_half_symbol(sync[i]);
    }

    const uint8_t* data = ford_5a5a;
    size_t data_size = sizeof(ford_5a5a);
    if(profile == TPMSFordLFProfileVDO) {
        data = ford_vdo;
        data_size = sizeof(ford_vdo);
    }
    for(size_t i = 0U; i < data_size; i++) {
        tpms_ford_lf_byte(data[i]);
    }

    /* One trailing OFF half-cell completes the final measured M block. */
    tpms_ford_lf_half_symbol(false);
    furi_hal_rfid_tim_read_pause();
    furi_kernel_restore_lock(kernel_state);
}

static int32_t tpms_ford_lf_worker(void* context) {
    TPMSReceiver* tpms_receiver = context;
    furi_assert(tpms_receiver);

    const uint32_t duration_ms = tpms_receiver->relearn_ford_duration_ms ?
                                     tpms_receiver->relearn_ford_duration_ms :
                                     TPMS_FORD_EL50449_DURATION_MS;
    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(duration_ms);

    /*
     * Start the 125 kHz generator once. The original EL-50449 PIC drive is
     * approximately 1 us ON / 7 us OFF, so use 12.5% PWM duty. Envelope data
     * is applied only with pause/continue; never restart the RFID timer for
     * individual pulses or symbols.
     */
    furi_hal_rfid_tim_read_start(
        TPMS_LF_CARRIER_HZ, TPMS_FORD_EL50449_CARRIER_DUTY);
    furi_hal_rfid_tim_read_pause();

    TPMSFordLFProfile frame_profile = tpms_receiver->relearn_ford_profile;
    /* Defensive fallback only. Scene code resolves AUTO to a concrete family
       before starting the worker, so one LF cycle contains one family only. */
    if(frame_profile == TPMSFordLFProfileAuto) frame_profile = TPMSFordLFProfile5A5A;

    while(!tpms_receiver->relearn_stop_requested &&
          (int32_t)(furi_get_tick() - deadline) < 0) {
        tpms_ford_lf_frame(frame_profile);
        if(tpms_receiver->relearn_stop_requested) break;
        furi_delay_ms(TPMS_FORD_EL50449_FRAME_GAP_MS);
    }

    furi_hal_rfid_tim_read_pause();
    furi_hal_rfid_tim_read_stop();
    tpms_receiver->relearn_active = false;
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            model->relearn_active = false;
            model->relearn_ford = false;
        },
        true);
    return 0;
}

static void tpms_relearn_join_ford_thread(TPMSReceiver* tpms_receiver) {
    if(!tpms_receiver->relearn_thread_started) return;
    tpms_receiver->relearn_stop_requested = true;
    furi_thread_join(tpms_receiver->relearn_thread);
    tpms_receiver->relearn_thread_started = false;
}

static void tpms_relearn_stop(void* context) {
    furi_assert(context);
    TPMSReceiver* tpms_receiver = context;

    if(tpms_receiver->relearn_thread_started) {
        tpms_relearn_join_ford_thread(tpms_receiver);
    }

    if(tpms_receiver->relearn_active) {
        tpms_receiver->relearn_active = false;
        furi_timer_stop(tpms_receiver->relearn_timer);
        furi_hal_rfid_tim_read_stop();
    }

    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            model->relearn_active = false;
            model->relearn_el50448 = false;
            model->relearn_ford = false;
        },
        true);
}

void tpms_view_receiver_relearn_stop(TPMSReceiver* tpms_receiver) {
    furi_assert(tpms_receiver);
    tpms_relearn_stop(tpms_receiver);
}

void tpms_view_receiver_relearn_start(
    TPMSReceiver* tpms_receiver,
    TPMSRelearnType relearn_type) {
    furi_assert(tpms_receiver);

    /* TPMS 3.8: never truncate an LF transmission because the user pressed
       RIGHT/OK again. Let the current 1/5 s cycle finish first. */
    if(tpms_receiver->relearn_active) return;

    /* A naturally completed Ford worker is still joinable. Reap it before
       starting the next requested cycle. */
    if(tpms_receiver->relearn_thread_started) {
        tpms_relearn_join_ford_thread(tpms_receiver);
    }

    const bool el50448 = (relearn_type == TPMSRelearnTypeEL50448);
    const bool ford = (relearn_type == TPMSRelearnTypeFordEL50449);

    tpms_receiver->relearn_last_type = relearn_type;
    tpms_receiver->relearn_repeat_available = true;
    tpms_receiver->relearn_active = true;
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            model->relearn_active = true;
            model->relearn_el50448 = el50448;
            model->relearn_ford = ford;
            model->relearn_repeat_available = true;
        },
        true);

    if(ford) {
        tpms_receiver->relearn_stop_requested = false;
        tpms_receiver->relearn_thread_started = true;
        furi_thread_start(tpms_receiver->relearn_thread);
        return;
    }

    /* CW Relearn and EL-50448 use a continuous LF carrier while the CC1101
       async RX remains active. EL-50448 is fixed to 125 kHz; CW Relearn may
       use 125.0 or 134.2 kHz. */
    /* EL-50449 and EL-50448 modes use 12.5% carrier duty, CW uses 50% */
    const uint32_t duration_ms =
        el50448 ? TPMS_EL50448_DURATION_MS : TPMS_RELEARN_COMMON_DURATION_MS;
    const uint32_t carrier_hz =
        el50448 ? TPMS_LF_CARRIER_HZ : tpms_receiver->relearn_cw_frequency_hz;
    const float carrier_duty =
        el50448 ? TPMS_EL50448_CARRIER_DUTY : TPMS_RELEARN_COMMON_CARRIER_DUTY;
    furi_hal_rfid_tim_read_start((float)carrier_hz, carrier_duty);
    furi_timer_start(tpms_receiver->relearn_timer, furi_ms_to_ticks(duration_ms));
}

static void tpms_view_receiver_relearn_timer_callback(void* context) {
    furi_assert(context);
    tpms_relearn_stop(context);
}

bool tpms_view_receiver_input(InputEvent* event, void* context) {
    furi_assert(context);
    TPMSReceiver* tpms_receiver = context;

    if(tpms_receiver->lock == TPMSLockOn) {
        with_view_model(
            tpms_receiver->view,
            TPMSReceiverModel * model,
            { model->bar_show = TPMSReceiverBarShowToUnlockPress; },
            true);
        if(tpms_receiver->lock_count == 0) {
            furi_timer_start(tpms_receiver->lock_timer, furi_ms_to_ticks(1000));
        }
        if(event->key == InputKeyBack && event->type == InputTypeShort) {
            tpms_receiver->lock_count++;
        }
        if(tpms_receiver->lock_count >= UNLOCK_CNT) {
            tpms_receiver->callback(TPMSCustomEventViewReceiverUnlock, tpms_receiver->context);
            with_view_model(
                tpms_receiver->view,
                TPMSReceiverModel * model,
                { model->bar_show = TPMSReceiverBarShowUnlock; },
                true);
            tpms_receiver->lock = TPMSLockOff;
            furi_timer_start(tpms_receiver->lock_timer, furi_ms_to_ticks(650));
        }

        return true;
    }

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        tpms_receiver->callback(TPMSCustomEventViewReceiverBack, tpms_receiver->context);
    } else if(
        event->key == InputKeyUp &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        with_view_model(
            tpms_receiver->view,
            TPMSReceiverModel * model,
            {
                if(model->idx != 0) model->idx--;
            },
            true);
    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        with_view_model(
            tpms_receiver->view,
            TPMSReceiverModel * model,
            {
                if(model->history_item && model->idx != model->history_item - 1) model->idx++;
            },
            true);
    } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        tpms_receiver->callback(TPMSCustomEventViewReceiverConfig, tpms_receiver->context);
    } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
        if(tpms_receiver->relearn_enabled) {
            tpms_receiver->callback(TPMSCustomEventViewReceiverRelearn, tpms_receiver->context);
        }
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        bool open_item = false;
        bool repeat_relearn = false;
        with_view_model(
            tpms_receiver->view,
            TPMSReceiverModel * model,
            {
                open_item = model->history_item != 0;
                repeat_relearn =
                    !open_item && model->relearn_enabled &&
                    model->relearn_repeat_available && !model->relearn_active;
            },
            false);
        if(open_item) {
            tpms_receiver->callback(TPMSCustomEventViewReceiverOK, tpms_receiver->context);
        } else if(repeat_relearn) {
            tpms_receiver->callback(TPMSCustomEventViewReceiverRelearn, tpms_receiver->context);
        }
    }

    tpms_view_receiver_update_offset(tpms_receiver);

    return true;
}

void tpms_view_receiver_enter(void* context) {
    furi_assert(context);
}

void tpms_view_receiver_exit(void* context) {
    furi_assert(context);
    TPMSReceiver* tpms_receiver = context;
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            furi_string_reset(model->frequency_str);
            furi_string_reset(model->preset_str);
            furi_string_reset(model->history_stat_str);
                for
                    M_EACH(item_menu, model->history->data, TPMSReceiverMenuItemArray_t) {
                        furi_string_free(item_menu->item_str);
                        item_menu->type = 0;
                    }
                TPMSReceiverMenuItemArray_reset(model->history->data);
                model->idx = 0;
                model->list_offset = 0;
                model->history_item = 0;
                model->u_rssi = 0U;
                model->rssi_dbm = -127;
                model->rssi_max_dbm = -127;
                model->rssi_valid = false;
                model->rssi_max_valid = false;
        },
        false);
    furi_timer_stop(tpms_receiver->lock_timer);
    tpms_relearn_stop(tpms_receiver);
}

TPMSReceiver* tpms_view_receiver_alloc() {
    TPMSReceiver* tpms_receiver = malloc(sizeof(TPMSReceiver));

    // View allocation and configuration
    tpms_receiver->view = view_alloc();

    tpms_receiver->lock = TPMSLockOff;
    tpms_receiver->lock_count = 0;
    tpms_receiver->relearn_thread_started = false;
    tpms_receiver->relearn_stop_requested = false;
    tpms_receiver->relearn_ford_profile = TPMSFordLFProfileAuto;
    tpms_receiver->relearn_ford_duration_ms = TPMS_FORD_EL50449_DURATION_MS;
    tpms_receiver->relearn_cw_frequency_hz = TPMS_LF_CARRIER_HZ;
    tpms_receiver->relearn_last_type = TPMSRelearnTypeCommon;
    tpms_receiver->relearn_repeat_available = false;
    tpms_receiver->relearn_enabled = false;
    tpms_receiver->relearn_active = false;
    view_allocate_model(tpms_receiver->view, ViewModelTypeLocking, sizeof(TPMSReceiverModel));
    view_set_context(tpms_receiver->view, tpms_receiver);
    view_set_draw_callback(tpms_receiver->view, (ViewDrawCallback)tpms_view_receiver_draw);
    view_set_input_callback(tpms_receiver->view, tpms_view_receiver_input);
    view_set_enter_callback(tpms_receiver->view, tpms_view_receiver_enter);
    view_set_exit_callback(tpms_receiver->view, tpms_view_receiver_exit);

    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            model->frequency_str = furi_string_alloc();
            model->preset_str = furi_string_alloc();
            model->history_stat_str = furi_string_alloc();
            model->u_rssi = 0U;
            model->rssi_dbm = -127;
            model->rssi_max_dbm = -127;
            model->rssi_valid = false;
            model->rssi_max_valid = false;
            model->bar_show = TPMSReceiverBarShowDefault;
            model->history = malloc(sizeof(TPMSReceiverHistory));
            model->external_radio = false;
            model->relearn_active = false;
            model->relearn_el50448 = false;
            model->relearn_ford = false;
            model->relearn_auto_rx = false;
            model->relearn_repeat_available = false;
            model->relearn_enabled = false;
            TPMSReceiverMenuItemArray_init(model->history->data);
        },
        true);
    tpms_receiver->lock_timer =
        furi_timer_alloc(tpms_view_receiver_lock_timer_callback, FuriTimerTypeOnce, tpms_receiver);
    tpms_receiver->relearn_timer = furi_timer_alloc(
        tpms_view_receiver_relearn_timer_callback, FuriTimerTypeOnce, tpms_receiver);
    tpms_receiver->relearn_thread =
        furi_thread_alloc_ex("TPMSFordLF", 2048U, tpms_ford_lf_worker, tpms_receiver);
    return tpms_receiver;
}

void tpms_view_receiver_free(TPMSReceiver* tpms_receiver) {
    furi_assert(tpms_receiver);

    tpms_relearn_stop(tpms_receiver);

    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            furi_string_free(model->frequency_str);
            furi_string_free(model->preset_str);
            furi_string_free(model->history_stat_str);
                for
                    M_EACH(item_menu, model->history->data, TPMSReceiverMenuItemArray_t) {
                        furi_string_free(item_menu->item_str);
                        item_menu->type = 0;
                    }
                TPMSReceiverMenuItemArray_clear(model->history->data);
                free(model->history);
        },
        false);
    furi_timer_free(tpms_receiver->lock_timer);
    furi_timer_free(tpms_receiver->relearn_timer);
    furi_thread_free(tpms_receiver->relearn_thread);
    view_free(tpms_receiver->view);
    free(tpms_receiver);
}

View* tpms_view_receiver_get_view(TPMSReceiver* tpms_receiver) {
    furi_assert(tpms_receiver);
    return tpms_receiver->view;
}

uint16_t tpms_view_receiver_get_idx_menu(TPMSReceiver* tpms_receiver) {
    furi_assert(tpms_receiver);
    uint32_t idx = 0;
    with_view_model(
        tpms_receiver->view, TPMSReceiverModel * model, { idx = model->idx; }, false);
    return idx;
}

void tpms_view_receiver_set_idx_menu(TPMSReceiver* tpms_receiver, uint16_t idx) {
    furi_assert(tpms_receiver);
    with_view_model(
        tpms_receiver->view,
        TPMSReceiverModel * model,
        {
            model->idx = idx;
            if(model->idx > 2) model->list_offset = idx - 2;
        },
        true);
    tpms_view_receiver_update_offset(tpms_receiver);
}
