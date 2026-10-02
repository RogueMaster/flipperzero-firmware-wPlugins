#include "protopirate_psa_bf_plugin.h"

#include "../../defines.h"
#include "../../protocols/psa_bf_core.h"
#include "../../protocols/renault_v1.h"
#include "../../protocols/protocols_common.h"
#include "../../helpers/protopirate_types.h"

#include <gui/modules/widget.h>
#include <gui/modules/widget_elements/widget_element.h>
#ifdef PROTOPIRATE_PSA_BF_PLUGIN_BUILD
#include "protopirate_psa_bf_plugin_icons.h"
#else
#include "proto_pirate_icons.h"
#endif

#define PSA_BF_PROGRESS_BAR_X 62
#define PSA_BF_PROGRESS_BAR_W 64
#define PSA_BF_PROGRESS_BAR_Y 24
#define PSA_BF_PROGRESS_BAR_H 8

typedef enum {
    ProtoPirateBfKindNone = 0,
    ProtoPirateBfKindPsa,
    ProtoPirateBfKindHitag2,
} ProtoPirateBfKind;

static const ProtoPiratePsaBfHostApi* g_host_api = NULL;

static ProtoPirateBfKind g_bf_kind = ProtoPirateBfKindNone;
static PsaBfState* g_bf_state = NULL;
static Hitag2BfState* g_hitag2_state = NULL;
static FuriThread* g_bf_thread = NULL;
static ProtoPiratePsaBfContext g_active_ctx = ProtoPiratePsaBfContextReceiverInfo;
static FlipperFormat* g_ff = NULL;
static Storage* g_storage = NULL;

static void show_bf_result(void* app, uint8_t status, ButtonCallback callback);
static void bf_finish_and_show_result(void* app, ButtonCallback result_callback);

static void bf_close_files() {
    //Free the ff file, we opened it on BF start if we also opened Storage.
    if(g_storage) {
        flipper_format_free(g_ff);
        g_ff = NULL;
        furi_record_close(RECORD_STORAGE);
        g_storage = NULL;
    }
}

static uint8_t bf_status(void) {
    if(g_bf_kind == ProtoPirateBfKindHitag2 && g_hitag2_state) {
        return g_hitag2_state->status;
    }
    if(g_bf_state) {
        return g_bf_state->status;
    }
    return PSA_BF_STATUS_IDLE;
}

static void bf_progress_values(uint32_t* cur, uint32_t* total) {
    if(g_bf_kind == ProtoPirateBfKindHitag2 && g_hitag2_state) {
        *cur = g_hitag2_state->progress_current;
        *total = g_hitag2_state->progress_total;
        return;
    }
    if(g_bf_state) {
        *cur = g_bf_state->progress_current;
        *total = g_bf_state->progress_total;
        return;
    }
    *cur = 0;
    *total = 0;
}

static void bf_set_cancel(void) {
    if(g_hitag2_state) {
        g_hitag2_state->cancel = 1;
    }
    if(g_bf_state) {
        g_bf_state->cancel = 1;
    }
}

static void bf_free_states(void) {
    if(g_bf_state) {
        free(g_bf_state);
        g_bf_state = NULL;
    }
    if(g_hitag2_state) {
        free(g_hitag2_state);
        g_hitag2_state = NULL;
    }
    g_bf_kind = ProtoPirateBfKindNone;
}

static bool psa_bf_needs_bruteforce(FlipperFormat* ff) {
    if(!ff) return false;
    FuriString* s = furi_string_alloc();

    flipper_format_rewind(ff);
    if(!flipper_format_read_string(ff, FF_PROTOCOL, s) || furi_string_cmp_str(s, "PSA") != 0) {
        furi_string_free(s);
        return false;
    }

    flipper_format_rewind(ff);
    bool has_key = flipper_format_read_string(ff, FF_KEY, s);
    if(!has_key) {
        furi_string_free(s);
        return false;
    }
    uint32_t serial = 0;
    flipper_format_rewind(ff);
    bool has_serial = flipper_format_read_uint32(ff, FF_SERIAL, &serial, 1);
    furi_string_free(s);
    return !has_serial;
}

static void show_bf_progress(void* app) {
    Widget* widget = g_host_api->get_widget(app);
    if(!widget || (!g_bf_state && !g_hitag2_state)) return;

    widget_reset(widget);
    widget_add_icon_element(widget, 0, 5, &I_DolphinWait_59x54);

    uint32_t cur = 0;
    uint32_t total = 0;
    bf_progress_values(&cur, &total);
    uint32_t pct_tenths = total ? (uint32_t)((uint64_t)cur * 1000 / total) : 0;
    if(pct_tenths > 1000) pct_tenths = 1000;

    if(g_bf_kind == ProtoPirateBfKindHitag2) {
        widget_add_string_element(widget, 62, 0, AlignLeft, AlignTop, FontPrimary, "Recover Key");
        widget_add_string_element(widget, 62, 12, AlignLeft, AlignTop, FontSecondary, "Max ETA:");
        widget_add_string_element(
            widget, 62, 22, AlignLeft, AlignTop, FontSecondary, "60 seconds");
    } else {
        widget_add_string_element(
            widget, 62, 0, AlignLeft, AlignTop, FontPrimary, "Bruteforcing...");
    }

    FuriString* pct_str =
        furi_string_alloc_printf("%lu.%u%%", pct_tenths / 10, (unsigned)(pct_tenths % 10));
    widget_add_string_element(
        widget,
        62,
        (g_bf_kind == ProtoPirateBfKindHitag2) ? 34 : 12,
        AlignLeft,
        AlignTop,
        FontSecondary,
        furi_string_get_cstr(pct_str));
    furi_string_free(pct_str);

    const uint8_t bar_y = (g_bf_kind == ProtoPirateBfKindHitag2) ? 46 : PSA_BF_PROGRESS_BAR_Y;
    widget_add_rect_element(
        widget,
        PSA_BF_PROGRESS_BAR_X,
        bar_y,
        PSA_BF_PROGRESS_BAR_W,
        PSA_BF_PROGRESS_BAR_H,
        2,
        false);

    uint8_t inner_w = PSA_BF_PROGRESS_BAR_W - 4;
    if(g_bf_kind == ProtoPirateBfKindHitag2) {
        uint8_t fill_w = total ? (uint8_t)(((uint64_t)cur * inner_w) / total) : 0;
        if(fill_w > 0) {
            widget_add_rect_element(
                widget,
                PSA_BF_PROGRESS_BAR_X + 2,
                bar_y + 2,
                fill_w,
                PSA_BF_PROGRESS_BAR_H - 4,
                0,
                true);
        }
    } else {
        static uint16_t bf_frame = 0;
        bf_frame++;
        uint8_t block_w = 16;
        uint8_t travel = inner_w - block_w;
        uint16_t phase = (bf_frame * 2) % (uint16_t)(2 * travel);
        uint8_t block_x = (phase <= travel) ? (uint8_t)phase : (uint8_t)(2 * travel - phase);
        widget_add_rect_element(
            widget,
            PSA_BF_PROGRESS_BAR_X + 2 + block_x,
            bar_y + 2,
            block_w,
            PSA_BF_PROGRESS_BAR_H - 4,
            0,
            true);
    }
}

static void bf_result_ok_callback(GuiButtonType result, InputType type, void* context) {
    void* app = context;
    if((type == InputTypeShort || type == InputTypeLong) && result == GuiButtonTypeCenter) {
        if(g_host_api && g_host_api->send_custom_event) {
            g_host_api->send_custom_event(app, ProtoPirateCustomEventBruteforceComplete);
        }
    }
}

static void show_bf_result(void* app, uint8_t status, ButtonCallback callback) {
    Widget* widget = g_host_api->get_widget(app);
    if(!widget) return;

    widget_reset(widget);
    const char* title = (status == PSA_BF_STATUS_FOUND)     ? "Found!" :
                        (status == PSA_BF_STATUS_CANCELLED) ? "Cancelled" :
                                                              "Not found";
    if(status == PSA_BF_STATUS_FOUND) {
        widget_add_icon_element(widget, 0, 3, &I_DolphinDone_80x58);
        widget_add_string_element(widget, 82, 32, AlignLeft, AlignCenter, FontPrimary, title);
        if(callback) {
            widget_add_button_element(widget, GuiButtonTypeCenter, "OK", callback, app);
        }
    } else if(status == PSA_BF_STATUS_CANCELLED) {
        widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, title);
        widget_add_icon_element(widget, (128 - 45) / 2, 14, &I_WarningDolphin_45x42);
    } else {
        widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, title);
    }
}

static void hitag2_refresh_history_text(void* app, FlipperFormat* ff) {
    if(!app || !ff || !g_host_api || !g_host_api->history_set_item_str) {
        return;
    }
    FuriString* text = furi_string_alloc();
    if(hitag2_flipper_format_get_string(ff, text)) {
        g_host_api->history_set_item_str(
            app, g_host_api->get_history_index(app), furi_string_get_cstr(text));
    }
    furi_string_free(text);
}

static void apply_success_to_ff(void* app) {
    uint16_t idx = g_host_api->get_history_index(app);
    if(g_bf_kind == ProtoPirateBfKindHitag2 && g_hitag2_state) {
        if(g_ff) {
            hitag2_bf_patch_flipper_format_on_success(g_ff, g_hitag2_state);
            hitag2_refresh_history_text(app, g_ff);
        }
        return;
    }
    if(!g_bf_state) {
        return;
    }
    PsaBfState* s = g_bf_state;
    if(g_ff) {
        g_host_api->patch_flipper_format_on_success(g_ff, s);
    }

    if(g_active_ctx != ProtoPiratePsaBfContextSavedInfo) {
        FuriString* new_str = furi_string_alloc_printf(
            "PSA 128bit\r\n"
            "Key1:%08lX%08lX\r\n"
            "Key2:%04X\r\n"
            "Btn:%02X\r\n"
            "Ser:%06lX\r\n"
            "Cnt:%lX\r\n"
            "Type:%02X\r\n"
            "Sd:%06lX",
            (unsigned long)s->key1_high,
            (unsigned long)s->key1_low,
            (unsigned int)(s->key2_low & 0xFFFF),
            (unsigned int)s->decrypted_button,
            (unsigned long)s->decrypted_serial,
            (unsigned long)s->decrypted_counter,
            (unsigned int)s->decrypted_type,
            (unsigned long)s->decrypted_seed);
        g_host_api->history_set_item_str(app, idx, furi_string_get_cstr(new_str));
        furi_string_free(new_str);
    }
}

static void bf_finish_and_show_result(void* app, ButtonCallback result_callback) {
    if(!g_bf_state && !g_hitag2_state) return;

    uint8_t status = bf_status();

    if(g_bf_thread) {
        furi_thread_join(g_bf_thread);
        furi_thread_free(g_bf_thread);
        g_bf_thread = NULL;
    }

    if(status == PSA_BF_STATUS_FOUND) {
        apply_success_to_ff(app);
        if(g_active_ctx == ProtoPiratePsaBfContextSavedInfo) {
            bf_close_files();
        }
        if(g_active_ctx == ProtoPiratePsaBfContextSubDecode ||
           g_active_ctx == ProtoPiratePsaBfContextSavedInfo) {
            g_host_api->notification_success(app);
        }
        ButtonCallback ok_cb = result_callback;
        if(!ok_cb && (g_active_ctx == ProtoPiratePsaBfContextReceiverInfo ||
                      g_active_ctx == ProtoPiratePsaBfContextSubDecode ||
                      g_active_ctx == ProtoPiratePsaBfContextSavedInfo)) {
            ok_cb = bf_result_ok_callback;
        }
        show_bf_result(app, status, ok_cb);
    } else {
        if(status == PSA_BF_STATUS_NOT_FOUND && g_bf_kind == ProtoPirateBfKindHitag2) {
            if(g_ff) {
                hitag2_bf_patch_flipper_format_on_miss(g_ff);
                hitag2_refresh_history_text(app, g_ff);
            }
        }
        if(g_active_ctx == ProtoPiratePsaBfContextSavedInfo) {
            bf_close_files();
        }

        show_bf_result(app, status, NULL);
    }
    bf_free_states();
}

static void bf_cancel_thread(void) {
    if(g_bf_thread) {
        bf_set_cancel();
        furi_thread_join(g_bf_thread);
        furi_thread_free(g_bf_thread);
        g_bf_thread = NULL;
    }
    bf_free_states();
}

static bool plugin_needs_bruteforce(FlipperFormat* ff) {
    return psa_bf_needs_bruteforce(ff) || hitag2_bf_needs_bruteforce(ff);
}

static bool plugin_is_running(void* app) {
    UNUSED(app);
    return g_bf_thread != NULL;
}

static void plugin_on_scene_enter(void* app, ProtoPiratePsaBfContext ctx) {
    g_active_ctx = ctx;
    if(g_bf_thread && (g_bf_state || g_hitag2_state)) {
        if(bf_status() == PSA_BF_STATUS_RUNNING) {
            show_bf_progress(app);
        } else {
            show_bf_result(app, bf_status(), NULL);
        }
    }
}

static bool start_bruteforce(void* app) {
    if(g_bf_thread) return false;

    if(g_active_ctx == ProtoPiratePsaBfContextSavedInfo) {
        g_storage = furi_record_open(RECORD_STORAGE);
        g_ff = flipper_format_file_alloc(g_storage);

        if(!flipper_format_file_open_existing(g_ff, g_host_api->get_loaded_file_path(app))) {
            furi_record_close(RECORD_STORAGE);
            g_storage = NULL;
            return false;
        }
    } else {
        g_ff = g_host_api->get_history_flipper_format(app);
        if(!g_ff) return false;
    }

    if(!plugin_needs_bruteforce(g_ff)) return false;
    if(psa_bf_needs_bruteforce(g_ff)) {
        PsaBfState* state = malloc(sizeof(PsaBfState));
        if(!state) {
            g_host_api->notification_error(app);
            return false;
        }
        if(!psa_bf_state_from_flipper_format(state, g_ff)) {
            free(state);
            g_host_api->notification_error(app);
            return false;
        }
        state->on_done = NULL;
        state->on_done_ctx = NULL;
        g_bf_state = state;
        g_bf_kind = ProtoPirateBfKindPsa;
        g_bf_thread = furi_thread_alloc_ex("PsaBf", 2048, psa_brute_force_thread_entry, state);
    } else if(hitag2_bf_needs_bruteforce(g_ff)) {
        Hitag2BfState* state = malloc(sizeof(Hitag2BfState));
        if(!state) {
            g_host_api->notification_error(app);
            return false;
        }
        if(!hitag2_bf_state_from_flipper_format(state, g_ff)) {
            free(state);
            g_host_api->notification_error(app);
            return false;
        }
        state->on_done = NULL;
        state->on_done_ctx = NULL;
        g_hitag2_state = state;
        g_bf_kind = ProtoPirateBfKindHitag2;
        g_bf_thread =
            furi_thread_alloc_ex("Hitag2Bf", 2048, hitag2_brute_force_thread_entry, state);
    } else {
        return false;
    }

    if(!g_bf_thread) {
        bf_free_states();
        g_host_api->notification_error(app);

        return false;
    }
    furi_thread_start(g_bf_thread);
    show_bf_progress(app);
    return true;
}

static bool
    plugin_on_scene_event(void* app, ProtoPiratePsaBfContext ctx, SceneManagerEvent event) {
    g_active_ctx = ctx;

    if(event.type == SceneManagerEventTypeBack) {
        if(bf_status() == PSA_BF_STATUS_FOUND) {
            if(ctx == ProtoPiratePsaBfContextReceiverInfo) {
                g_host_api->receiver_info_rebuild_widget(app);
            }
            bf_free_states();
            return true;
        }
        if(g_bf_thread && bf_status() == PSA_BF_STATUS_RUNNING) {
            bf_set_cancel();
            return true;
        }
        return false;
    } else if(event.type == SceneManagerEventTypeTick) {
        if(g_bf_thread && (g_bf_state || g_hitag2_state)) {
            uint8_t bfst = bf_status();
            if(bfst == PSA_BF_STATUS_IDLE || bfst == PSA_BF_STATUS_RUNNING) {
                show_bf_progress(app);
            } else {
                bf_finish_and_show_result(app, NULL);
            }
            return true;
        }
        return false;
    }

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    if(ctx == ProtoPiratePsaBfContextReceiverInfo) {
        if(event.event == ProtoPirateCustomEventBruteforceStart) {
            return start_bruteforce(app);
        }
        if(event.event == ProtoPirateCustomEventBruteforceComplete) {
            if(bf_status() == PSA_BF_STATUS_FOUND) {
                g_host_api->receiver_info_rebuild_widget(app);
                bf_free_states();
            } else if(bf_status() == PSA_BF_STATUS_RUNNING) {
                bf_set_cancel();
            } else {
                if(g_bf_state || g_hitag2_state) {
                    bf_finish_and_show_result(app, NULL);
                }
                g_host_api->scene_previous(app);
            }
            return true;
        }
    }

    if(ctx == ProtoPiratePsaBfContextSubDecode) {
        if(event.event == ProtoPirateCustomEventBruteforceStart) {
            if(start_bruteforce(app)) {
                return true;
            }
            return true;
        }
        if(event.event == ProtoPirateCustomEventBruteforceComplete) {
            if(bf_status() == PSA_BF_STATUS_FOUND) {
                bf_free_states();
            } else if(bf_status() == PSA_BF_STATUS_RUNNING) {
                bf_set_cancel();
            } else {
                if(g_bf_state || g_hitag2_state) {
                    bf_finish_and_show_result(app, NULL);
                }
            }
            return true;
        }
    }

    if(ctx == ProtoPiratePsaBfContextSavedInfo) {
        if(event.event == ProtoPirateCustomEventBruteforceStart) {
            if(start_bruteforce(app)) {
                return true;
            }
        } else if(event.event == ProtoPirateCustomEventBruteforceComplete) {
            if(bf_status() == PSA_BF_STATUS_FOUND) {
                bf_free_states();
            } else if(bf_status() == PSA_BF_STATUS_RUNNING) {
                bf_set_cancel();
                bf_close_files();
            } else {
                if(g_bf_state || g_hitag2_state) {
                    bf_finish_and_show_result(app, NULL);
                }
            }
            return true;
        }
    }
    return false;
}

static void plugin_on_scene_exit(void* app, ProtoPiratePsaBfContext ctx) {
    UNUSED(app);
    UNUSED(ctx);
    bf_cancel_thread();
}

static bool plugin_widget_left_should_bruteforce(void* app, FlipperFormat* ff) {
    if(!ff) {
        ff = g_host_api->get_history_flipper_format(app);
    }
    return !g_bf_thread && plugin_needs_bruteforce(ff);
}

static void plugin_context_release(void* app) {
    UNUSED(app);
    bf_cancel_thread();
}

static void plugin_set_host_api(const ProtoPiratePsaBfHostApi* api) {
    g_host_api = api;
}

static const ProtoPiratePsaBfPlugin protopirate_psa_bf_plugin = {
    .plugin_name = "ProtoPirate PSA BF",
    .set_host_api = plugin_set_host_api,
    .needs_bruteforce = plugin_needs_bruteforce,
    .is_running = plugin_is_running,
    .on_scene_enter = plugin_on_scene_enter,
    .on_scene_event = plugin_on_scene_event,
    .on_scene_exit = plugin_on_scene_exit,
    .widget_left_should_bruteforce = plugin_widget_left_should_bruteforce,
    .context_release = plugin_context_release,
};

static const FlipperAppPluginDescriptor protopirate_psa_bf_plugin_descriptor = {
    .appid = PROTOPIRATE_PSA_BF_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PSA_BF_PLUGIN_API_VERSION,
    .entry_point = &protopirate_psa_bf_plugin,
};

const FlipperAppPluginDescriptor* protopirate_psa_bf_plugin_ep(void) {
    return &protopirate_psa_bf_plugin_descriptor;
}
