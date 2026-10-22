#include "genie_send.h"

#include "genie_app.h"
#include "genie_submenu.h"
#include "genie_subghz_receive.h"
#include "genie_file.h"

#include <flipper_format/flipper_format_i.h>
#include <lib/subghz/transmitter.h>

#ifdef TAG
#undef TAG
#endif
#define TAG "GenieSend"

#define GENIE_SEND_CANCEL     1U
#define GENIE_SEND_TIMEOUT_MS 5000U

typedef enum {
    GenieTxFailed,
    GenieTxComplete,
    GenieTxCancelled,
    GenieTxTimeout,
} GenieTxResult;

struct GenieSend {
    View* view;
    GenieApp* app;
    FuriThread* worker;
    FuriString* path;
    uint32_t key_hi;
    uint32_t key_lo;
    uint32_t frequency;
    uint16_t next_index;
    bool pending_save; /* Accessed by the GUI only after joining the worker. */
};

typedef struct {
    GenieFile* file;
    bool loaded;
    bool sending;
    const char* status;
} GenieRefs;

static bool genie_send_cancelled(void) {
    return (furi_thread_flags_get() & GENIE_SEND_CANCEL) != 0;
}

static void
    set_genie(FlipperFormat* flipper_format, uint32_t key_hi, uint32_t key_lo, uint32_t repeat) {
    FuriString* genie_settings = furi_string_alloc_printf(
        "Protocol: Genie\n"
        "Bit: 64\n"
        "Key: %02X %02X %02X %02X %02X %02X %02X %02X\n"
        "Increment: 0\n"
        "Repeat: %lu\n",
        (uint8_t)((key_hi >> 24) & 0xFFU),
        (uint8_t)((key_hi >> 16) & 0xFFU),
        (uint8_t)((key_hi >> 8) & 0xFFU),
        (uint8_t)((key_hi) & 0xFFU),
        (uint8_t)((key_lo >> 24) & 0xFFU),
        (uint8_t)((key_lo >> 16) & 0xFFU),
        (uint8_t)((key_lo >> 8) & 0xFFU),
        (uint8_t)((key_lo) & 0xFFU),
        repeat);
    Stream* stream = flipper_format_get_raw_stream(flipper_format);
    stream_clean(stream);
    stream_write_cstring(stream, furi_string_get_cstr(genie_settings));
    stream_seek(stream, 0, StreamOffsetFromStart);
    furi_string_free(genie_settings);
}

const SubGhzProtocol* send_subghz_protocol_registry_items[] = {
    &subghz_protocol_genie,
};

const SubGhzProtocolRegistry send_subghz_protocol_registry = {
    .items = send_subghz_protocol_registry_items,
    .size = COUNT_OF(send_subghz_protocol_registry_items)};

static GenieTxResult send_genie(uint32_t key_hi, uint32_t key_lo, uint32_t frequency) {
    GenieTxResult result = GenieTxFailed;
    const SubGhzDevice* device;
    bool device_started = false;
    bool charge_suppressed = false;
    subghz_devices_init();
    device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    SubGhzEnvironment* environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(environment, (void*)&send_subghz_protocol_registry);
    SubGhzTransmitter* transmitter = subghz_transmitter_alloc_init(environment, "Genie");
    FlipperFormat* flipper_format = flipper_format_string_alloc();
    do {
        if(genie_send_cancelled()) {
            result = GenieTxCancelled;
            break;
        }
        if(!device || !transmitter || !subghz_devices_is_frequency_valid(device, frequency)) break;
        set_genie(flipper_format, key_hi, key_lo, 5);
        if(subghz_transmitter_deserialize(transmitter, flipper_format) != SubGhzProtocolStatusOk)
            break;
        subghz_devices_begin(device);
        device_started = true;
        subghz_devices_reset(device);
        subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
        subghz_devices_set_frequency(device, frequency);
        if(genie_send_cancelled()) {
            result = GenieTxCancelled;
            break;
        }
        furi_hal_power_suppress_charge_enter();
        charge_suppressed = true;
        if(!subghz_devices_start_async_tx(device, subghz_transmitter_yield, transmitter)) break;
        uint32_t started = furi_get_tick();
        for(;;) {
            if(subghz_devices_is_async_complete_tx(device)) {
                result = GenieTxComplete;
                break;
            }
            if(genie_send_cancelled()) {
                result = GenieTxCancelled;
                break;
            }
            if(furi_get_tick() - started >= furi_ms_to_ticks(GENIE_SEND_TIMEOUT_MS)) {
                result = GenieTxTimeout;
                break;
            }
            /* Keep the cancellation bit set until this worker exits. */
            furi_thread_flags_wait(
                GENIE_SEND_CANCEL, FuriFlagWaitAny | FuriFlagNoClear, furi_ms_to_ticks(20));
        }
        subghz_devices_stop_async_tx(device);
    } while(false);
    if(transmitter) {
        subghz_transmitter_stop(transmitter);
        subghz_transmitter_free(transmitter);
    }
    if(device_started) {
        subghz_devices_sleep(device);
        subghz_devices_end(device);
    }
    subghz_devices_deinit();
    if(charge_suppressed) furi_hal_power_suppress_charge_exit();
    flipper_format_free(flipper_format);
    subghz_environment_free(environment);
    return result;
}

static void genie_send_refresh_file(GenieSend* sender) {
    GenieFile* file = genie_file_load(genie_app_get_file_path(sender->app));
    with_view_model(
        sender->view,
        GenieRefs * refs,
        {
            genie_file_free(refs->file);
            refs->file = file;
            refs->loaded = file != NULL;
        },
        true);
}

static int32_t genie_send_worker(void* context) {
    GenieSend* sender = context;
    GenieTxResult result = send_genie(sender->key_hi, sender->key_lo, sender->frequency);
    const char* status = NULL;
    if(result == GenieTxComplete) {
        sender->pending_save =
            !genie_file_set_last_sent(furi_string_get_cstr(sender->path), sender->next_index);
        if(sender->pending_save) status = "Save failed. OK to retry.";
        genie_send_refresh_file(sender);
    } else if(result == GenieTxCancelled) {
        status = "Cancelled. OK to retry.";
    } else if(result == GenieTxTimeout) {
        status = "TX timeout. OK to retry.";
    } else {
        status = "Send failed. OK to retry.";
    }
    with_view_model(
        sender->view,
        GenieRefs * refs,
        {
            refs->status = status;
            refs->sending = false;
        },
        true);
    return 0;
}

static void genie_send_join(GenieSend* sender, bool cancel) {
    if(!sender->worker) return;
    if(cancel && furi_thread_get_state(sender->worker) != FuriThreadStateStopped) {
        furi_thread_flags_set(furi_thread_get_id(sender->worker), GENIE_SEND_CANCEL);
    }
    furi_thread_join(sender->worker);
    furi_thread_free(sender->worker);
    sender->worker = NULL;
}

/* Retry a failed index write without transmitting the same code again. */
static bool genie_send_retry_save(GenieSend* sender) {
    if(!sender->pending_save) return true;
    sender->pending_save =
        !genie_file_set_last_sent(furi_string_get_cstr(sender->path), sender->next_index);
    with_view_model(
        sender->view,
        GenieRefs * refs,
        { refs->status = sender->pending_save ? "Save failed. OK to retry." : NULL; },
        true);
    genie_send_refresh_file(sender);
    return !sender->pending_save;
}

static void genie_send_draw_callback(Canvas* canvas, void* model) {
    GenieRefs* refs = (GenieRefs*)model;
    GenieFile* file = refs->file;
    char buffer[36] = {0};

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 10, "Genie Sub-Ghz Sender!!!");
    canvas_set_font(canvas, FontSecondary);
    if(refs->loaded) {
        if(!file) {
            return;
        }
        snprintf(
            buffer,
            COUNT_OF(buffer),
            "KEY: %08lx %08lx",
            genie_file_get_key_hi(file),
            genie_file_get_key_lo(file));
        canvas_draw_str(canvas, 1, 23, buffer);

        if(genie_file_get_rec_count(file) < 0xFFFD) {
            snprintf(
                buffer,
                COUNT_OF(buffer),
                "Last sent: %d of %d",
                genie_file_get_last_sent(file),
                genie_file_get_rec_count(file));
            canvas_draw_str(canvas, 1, 33, buffer);

            snprintf(buffer, COUNT_OF(buffer), "Remote not fully captured!");
            canvas_draw_str(canvas, 1, 49, buffer);
        } else {
            snprintf(buffer, COUNT_OF(buffer), "Last sent: %d", genie_file_get_last_sent(file));
            canvas_draw_str(canvas, 1, 33, buffer);
        }

        if(refs->status) {
            canvas_draw_str(canvas, 1, 63, refs->status);
        } else if(genie_file_get_last_sent(file) < genie_file_get_rec_count(file)) {
            canvas_draw_str(canvas, 1, 63, "Press OK to send next code.");
        } else {
            canvas_draw_str(canvas, 1, 61, "Long press OK to reset code.");
        }
    } else {
        canvas_draw_str(canvas, 20, 36, "NO .GNE FILE LOADED");
    }
}

static bool genie_send_input_callback(InputEvent* event, void* context) {
    GenieSend* sender = context;
    if(event->key != InputKeyOk) return false;
    bool sending = false;
    with_view_model(sender->view, GenieRefs * refs, { sending = refs->sending; }, false);
    if(sending) return true;
    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;
    genie_send_join(sender, false);
    if(sender->pending_save) {
        genie_send_retry_save(sender);
        return true;
    }
    genie_send_refresh_file(sender);
    bool start = false;
    bool reset = false;
    with_view_model(
        sender->view,
        GenieRefs * refs,
        {
            if(refs->file) {
                uint16_t last = genie_file_get_last_sent(refs->file);
                uint16_t count = genie_file_get_rec_count(refs->file);
                if(event->type == InputTypeShort && last < count) {
                    sender->key_hi = genie_file_get_key_hi(refs->file);
                    sender->key_lo = genie_file_get_key_lo(refs->file);
                    sender->next_index = last + 1;
                    refs->sending = true;
                    refs->status = "Sending... Back to cancel";
                    start = true;
                } else if(event->type == InputTypeLong && last >= count) {
                    reset = true;
                }
            }
        },
        true);
    if(start) {
        furi_string_set(sender->path, genie_app_get_file_path(sender->app));
        sender->frequency = genie_app_get_frequency(sender->app);
        sender->worker = furi_thread_alloc_ex("GenieSend", 4096, genie_send_worker, sender);
        furi_thread_start(sender->worker);
    } else if(reset) {
        bool saved = genie_file_set_last_sent(genie_app_get_file_path(sender->app), 0);
        with_view_model(
            sender->view,
            GenieRefs * refs,
            { refs->status = saved ? NULL : "Unable to reset index"; },
            true);
        genie_send_refresh_file(sender);
    }
    return true;
}

static void genie_send_enter_callback(void* context) {
    GenieSend* sender = context;
    genie_send_join(sender, true);
    genie_send_retry_save(sender);
    genie_send_refresh_file(sender);
}

static void genie_send_exit_callback(void* context) {
    genie_send_join(context, true);
}

GenieSend* genie_send_alloc(GenieApp* app) {
    GenieSend* sender = malloc(sizeof(GenieSend));
    sender->app = app;
    sender->worker = NULL;
    sender->pending_save = false;
    sender->path = furi_string_alloc();
    View* view = view_alloc();
    sender->view = view;
    view_set_draw_callback(view, genie_send_draw_callback);
    view_set_input_callback(view, genie_send_input_callback);
    view_set_previous_callback(view, genie_navigation_submenu_callback);
    view_set_enter_callback(view, genie_send_enter_callback);
    view_set_exit_callback(view, genie_send_exit_callback);
    view_set_context(view, sender);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(GenieRefs));
    with_view_model(
        view,
        GenieRefs * refs,
        {
            refs->file = NULL;
            refs->loaded = false;
            refs->sending = false;
            refs->status = NULL;
        },
        false);
    return sender;
}

void genie_send_free(GenieSend* sender) {
    furi_assert(sender);
    genie_send_join(sender, true);
    with_view_model(
        sender->view,
        GenieRefs * refs,
        {
            genie_file_free(refs->file);
            refs->file = NULL;
        },
        false);
    view_free(sender->view);
    furi_string_free(sender->path);
    free(sender);
}

View* genie_send_get_view(GenieSend* sender) {
    furi_assert(sender);
    return sender->view;
}
