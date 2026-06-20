#include "protopirate_psa_bf_plugin.h"

#include "../../defines.h"
#include "../../protocols/psa_bf_core.h"
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

static const ProtoPiratePsaBfHostApi* g_host_api = NULL;

static PsaBfState* g_bf_state = NULL;
static FuriThread* g_bf_thread = NULL;
static ProtoPiratePsaBfContext g_active_ctx = ProtoPiratePsaBfContextReceiverInfo;

static void show_bf_result(void* app, uint8_t status, ButtonCallback callback);
static void bf_finish_and_show_result(void* app, ButtonCallback result_callback);

static void psa_bf_done_cb(void* context) {
    if(g_host_api && g_host_api->send_custom_event) {
        g_host_api->send_custom_event(context, ProtoPirateCustomEventPsaBruteforceComplete);
    }
}

static bool item_needs_bruteforce_from_ff(FlipperFormat* ff, bool require_psa_protocol) {
    if(!ff) return false;
    FuriString* s = furi_string_alloc();
    flipper_format_rewind(ff);
    if(require_psa_protocol) {
        if(!flipper_format_read_string(ff, FF_PROTOCOL, s) || furi_string_cmp_str(s, "PSA") != 0) {
            furi_string_free(s);
            return false;
        }
        flipper_format_rewind(ff);
    }
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
    if(!widget || !g_bf_state) return;

    widget_reset(widget);
    widget_add_icon_element(widget, 0, 5, &I_DolphinWait_59x54);
    widget_add_string_element(widget, 62, 0, AlignLeft, AlignTop, FontPrimary, "Bruteforcing...");

    uint32_t cur = g_bf_state->progress_current;
    uint32_t total = g_bf_state->progress_total;
    uint32_t pct_tenths = total ? (uint32_t)((uint64_t)cur * 1000 / total) : 0;
    if(pct_tenths > 1000) pct_tenths = 1000;

    FuriString* pct_str =
        furi_string_alloc_printf("%lu.%u%%", pct_tenths / 10, (unsigned)(pct_tenths % 10));
    widget_add_string_element(
        widget, 62, 12, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(pct_str));
    furi_string_free(pct_str);

    widget_add_frame_element(
        widget,
        PSA_BF_PROGRESS_BAR_X,
        PSA_BF_PROGRESS_BAR_Y,
        PSA_BF_PROGRESS_BAR_W,
        PSA_BF_PROGRESS_BAR_H,
        2);
    static uint16_t bf_frame = 0;
    bf_frame++;
    uint8_t inner_w = PSA_BF_PROGRESS_BAR_W - 4;
    uint8_t block_w = 16;
    uint8_t travel = inner_w - block_w;
    uint16_t phase = (bf_frame * 2) % (uint16_t)(2 * travel);
    uint8_t block_x = (phase <= travel) ? (uint8_t)phase : (uint8_t)(2 * travel - phase);
    widget_add_frame_element(
        widget,
        PSA_BF_PROGRESS_BAR_X + 2 + block_x,
        PSA_BF_PROGRESS_BAR_Y + 2,
        block_w,
        PSA_BF_PROGRESS_BAR_H - 4,
        0);
}

static void bf_result_ok_callback(GuiButtonType result, InputType type, void* context) {
    void* app = context;
    if((type == InputTypeShort || type == InputTypeLong) && result == GuiButtonTypeCenter) {
        if(g_host_api && g_host_api->send_custom_event) {
            g_host_api->send_custom_event(app, ProtoPirateCustomEventReceiverInfoBruteforceCancel);
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

static void apply_success_to_history(void* app, PsaBfState* s) {
    FlipperFormat* ff = g_host_api->get_history_flipper_format(app);
    uint16_t idx = g_host_api->get_history_index(app);
    if(ff) {
        g_host_api->patch_flipper_format_on_success(ff, s);
    }
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

static void bf_finish_and_show_result(void* app, ButtonCallback result_callback) {
    if(!g_bf_state) return;

    PsaBfState* s = g_bf_state;
    uint8_t status = s->status;

    if(g_bf_thread) {
        furi_thread_join(g_bf_thread);
        furi_thread_free(g_bf_thread);
        g_bf_thread = NULL;
    }

    if(status == PSA_BF_STATUS_FOUND) {
        apply_success_to_history(app, s);
        if(g_active_ctx == ProtoPiratePsaBfContextSubDecode) {
            g_host_api->notification_success(app);
        }
        ButtonCallback ok_cb = result_callback;
        if(!ok_cb && g_active_ctx == ProtoPiratePsaBfContextReceiverInfo) {
            ok_cb = bf_result_ok_callback;
        }
        show_bf_result(app, status, ok_cb);
    } else {
        free(g_bf_state);
        g_bf_state = NULL;
        show_bf_result(app, status, NULL);
    }
}

static void bf_cancel_thread(void) {
    if(g_bf_thread) {
        if(g_bf_state) g_bf_state->cancel = 1;
        furi_thread_join(g_bf_thread);
        furi_thread_free(g_bf_thread);
        g_bf_thread = NULL;
    }
    if(g_bf_state) {
        free(g_bf_state);
        g_bf_state = NULL;
    }
}

static bool plugin_needs_bruteforce(void* app, ProtoPiratePsaBfContext ctx) {
    FlipperFormat* ff = g_host_api->get_history_flipper_format(app);
    return item_needs_bruteforce_from_ff(ff, ctx == ProtoPiratePsaBfContextReceiverInfo);
}

static bool plugin_is_running(void* app) {
    UNUSED(app);
    return g_bf_thread != NULL;
}

static void plugin_on_scene_enter(void* app, ProtoPiratePsaBfContext ctx) {
    g_active_ctx = ctx;
    if(g_bf_thread && g_bf_state) {
        if(g_bf_state->status == PSA_BF_STATUS_RUNNING) {
            show_bf_progress(app);
        } else {
            show_bf_result(app, g_bf_state->status, NULL);
        }
    }
}

static bool start_bruteforce(void* app) {
    if(g_bf_thread) return false;

    FlipperFormat* ff = g_host_api->get_history_flipper_format(app);
    if(!ff || !plugin_needs_bruteforce(app, g_active_ctx)) return false;

    PsaBfState* state = malloc(sizeof(PsaBfState));
    if(!state) {
        g_host_api->notification_error(app);
        return false;
    }
    if(!psa_bf_state_from_flipper_format(state, ff)) {
        free(state);
        g_host_api->notification_error(app);
        return false;
    }
    state->on_done = psa_bf_done_cb;
    state->on_done_ctx = app;
    g_bf_state = state;
    g_bf_thread = furi_thread_alloc_ex("PsaBf", 2048, psa_brute_force_thread_entry, state);
    if(!g_bf_thread) {
        free(state);
        g_bf_state = NULL;
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
        if(g_bf_state && g_bf_state->status == PSA_BF_STATUS_FOUND) {
            if(ctx == ProtoPiratePsaBfContextReceiverInfo) {
                g_host_api->receiver_info_rebuild_widget(app);
            }
            free(g_bf_state);
            g_bf_state = NULL;
            return true;
        }
        if(g_bf_thread && g_bf_state && g_bf_state->status == PSA_BF_STATUS_RUNNING) {
            g_bf_state->cancel = 1;
            return true;
        }
        return false;
    }

    if(event.type == SceneManagerEventTypeTick) {
        if(g_bf_thread && g_bf_state) {
            uint8_t bfst = g_bf_state->status;
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

    if(event.event == ProtoPirateCustomEventPsaBruteforceComplete) {
        if(g_bf_state) {
            bf_finish_and_show_result(app, NULL);
            if(g_active_ctx == ProtoPiratePsaBfContextSubDecode) {
                g_host_api->subdecode_signal_info_refresh(app);
            }
        }
        return true;
    }

    if(ctx == ProtoPiratePsaBfContextReceiverInfo) {
        if(event.event == ProtoPirateCustomEventReceiverInfoBruteforceStart) {
            return start_bruteforce(app);
        }
        if(event.event == ProtoPirateCustomEventReceiverInfoBruteforceCancel) {
            if(g_bf_state && g_bf_state->status == PSA_BF_STATUS_FOUND) {
                g_host_api->receiver_info_rebuild_widget(app);
                free(g_bf_state);
                g_bf_state = NULL;
            } else if(g_bf_state && g_bf_state->status == PSA_BF_STATUS_RUNNING) {
                g_bf_state->cancel = 1;
            } else {
                if(g_bf_state) {
                    bf_finish_and_show_result(app, NULL);
                }
                g_host_api->scene_previous(app);
            }
            return true;
        }
    }

    if(ctx == ProtoPiratePsaBfContextSubDecode) {
        if(event.event == ProtoPirateCustomEventSubDecodeBruteforceStart) {
            if(start_bruteforce(app)) {
                return true;
            }
            return true;
        }
    }

    return false;
}

static void plugin_on_scene_exit(void* app, ProtoPiratePsaBfContext ctx) {
    UNUSED(app);
    UNUSED(ctx);
}

static bool plugin_widget_left_should_bruteforce(void* app, ProtoPiratePsaBfContext ctx) {
    return !g_bf_thread && plugin_needs_bruteforce(app, ctx);
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
