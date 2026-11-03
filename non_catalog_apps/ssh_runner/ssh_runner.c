/*
 * SSH Runner — Flipper UI for the SSH Runner ESP32-S3 + W5500 add-on board.
 * ViewDispatcher + SceneManager, one scene per screen, like the stock apps.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "ssh_runner_i.h"
#include "core/heapcheck.h"
#include "scenes/scene_util.h"

#include <dialogs/dialogs.h>

#include <ctype.h>

static void backend_event(void* ctx, const BackendEvent* e) {
    SshRunnerApp* app = ctx;
    /* The Output screen draws the OutBuf in place (no copy): it changes only under the
     * TextView's lock, which re-points the view at it before a frame can see it. */
    bool out = e->type == BackendEvtStream || e->type == BackendEvtExecDone;
    if(out) text_view_lock(app->text_view);
    uint32_t chg = session_on_event(app->session, e);
    if(out) {
        if(app->output_bound) output_bind(app);
        text_view_unlock(app->text_view, false);
    }
    switch(e->type) {
    case BackendEvtScanResult:
        /* up to 8 networks per event: merged into one list */
        if(!app->scan) {
            app->scan = malloc(sizeof(ScanList));
            scan_list_reset(app->scan);
        }
        scan_list_merge(app->scan, &e->scan);
        chg |= APP_CHG_SCAN;
        break;
    case BackendEvtWifiJoinResult:
        app->join = e->join; /* read with the NetStatus that follows (scene_network.c) */
        app->join_known = true;
        break;
    case BackendEvtPing:
        app->ping = e->ping;
        app->ping_done = true;
        chg |= APP_CHG_PING;
        break;
    case BackendEvtKeyBlob:
        keys_on_blob(app, &e->blob);
        break;
    case BackendEvtKeyPub:
        keys_on_pub(app, &e->key);
        break;
    case BackendEvtLink:
        if(e->link.state == BackendLinkUp) app->sec_relinked = true;
        break;
    case BackendEvtSecStatus:
        app->sec = e->sec;
        app->sec_known = true;
        chg |= APP_CHG_SEC;
        break;
    case BackendEvtSecState:
        app->sec_state = e->sec_state;
        chg |= APP_CHG_SEC;
        break;
    case BackendEvtHostkeyList:
        app->hostkeys = e->hostkeys;
        app->hostkeys_loaded = true;
        chg |= APP_CHG_HOSTKEYS;
        break;
    case BackendEvtOpDone:
        chg |= APP_CHG_OPDONE;
        break;
    case BackendEvtError:
        app->last_error = e->error;
        chg |= APP_CHG_ERROR;
        break;
    default:
        break;
    }
    if(chg & SESSION_CHG_OUTPUT) app->output_dirty = true;
    app->chg |= chg;
}

/* The lowest free heap seen while the app runs, for the RAM check of the walks (both
 * builds, reported only while the walk tool's marker exists). Sampled every tick; a spike inside
 * one event shows in the firmware's since-boot minimum instead. */
static volatile size_t heap_low;

static void tick_timer_cb(void* ctx) {
    SshRunnerApp* app = ctx;
    size_t f = memmgr_get_free_heap();
    if(f < heap_low) heap_low = f;
    /* At most one tick in flight, so a busy GUI thread never backs up the queue. */
    if(!app->tick_pending) {
        app->tick_pending = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, EvtChanged);
    }
}

static bool custom_event_cb(void* ctx, uint32_t event) {
    SshRunnerApp* app = ctx;
    if(event == EvtChanged) {
        app->tick_pending = false;
        /* Tick: pump the backend on the GUI thread, then let the scene react. */
        uint32_t credit = session_take_credit(app->session);
        if(credit) app->backend.ops->flow_credit(app->backend.self, credit);
        backend_tick(&app->backend, furi_get_tick());
        app->chg |= keys_process(app);
        util_submenu_tick();
        /* The link answers every frame from this tick (one stream frame in flight, then its
         * ACK; credit likewise): while a command runs, a 20 ms tick held output to ~15 KiB/s. */
        bool fast = app->session->running;
        if(fast != app->tick_fast) {
            app->tick_fast = fast;
            furi_timer_start(app->tick_timer, furi_ms_to_ticks(fast ? TICK_RUN_MS : TICK_MS));
        }
        if(!app->chg && !app->output_dirty && !app->want_ticks) return true;
        /* Scenes read the bits through app_take_changes() during this dispatch. */
        app->cur_chg = app->chg;
        app->chg = 0;
        bool r = scene_manager_handle_custom_event(app->scene_manager, event);
        app->cur_chg = 0;
        return r;
    }
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool back_event_cb(void* ctx) {
    SshRunnerApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

uint32_t app_take_changes(SshRunnerApp* app) {
    return app->cur_chg;
}

void app_backend_restart(SshRunnerApp* app) {
    if(app->backend.self) backend_free(&app->backend);
    session_init(app->session);
    app->key_job = KeyJobNone;
#if APP_HAS_MOCK
    app->backend = app->settings.mock ? mock_backend_create(backend_event, app) :
                                        uart_backend_create(backend_event, app);
#else
    app->backend = uart_backend_create(backend_event, app);
#endif
    furi_check(app->backend.self);
    app->backend.ops->start(app->backend.self);
}

void app_notify_success(SshRunnerApp* app) {
    notification_message(app->notifications, &sequence_success);
}

void app_notify_error(SshRunnerApp* app) {
    notification_message(app->notifications, &sequence_error);
}

void app_blink_start(SshRunnerApp* app) {
    if(app->blinking) return;
    notification_message(app->notifications, &sequence_blink_start_blue);
    app->blinking = true;
}

void app_blink_stop(SshRunnerApp* app) {
    if(!app->blinking) return;
    notification_message(app->notifications, &sequence_blink_stop);
    app->blinking = false;
}

void app_end_session(SshRunnerApp* app) {
    app_blink_stop(app);
    Session* s = app->session;
    if(s->phase != SessionIdle && s->phase != SessionClosed && s->phase != SessionFailed)
        app->backend.ops->disconnect(app->backend.self);
    if(app->script_file) {
        storage_file_close(app->script_file);
        storage_file_free(app->script_file);
        app->script_file = NULL;
    }
    if(app->key_file) {
        storage_file_close(app->key_file);
        storage_file_free(app->key_file);
        app->key_file = NULL;
    }
    session_reset(s);
    app->have_output = false;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, app->return_scene);
}

void app_fill_home_status(SshRunnerApp* app, HomeStatus* st) {
    memset(st, 0, sizeof(*st));
    Session* s = app->session;
#ifdef SSH_RUNNER_DEMO_BUILD
    st->demo = true; /* the demo build is never mistaken for the real app */
#else
    st->demo = app->settings.mock;
#endif
    st->board_ok = s->link.state == BackendLinkUp;
    st->lan_up = session_iface_up(s, ProtoIfaceLan);
    st->wifi_up = session_iface_up(s, ProtoIfaceWifi);
    if(s->link.state == BackendLinkMismatch) {
        snprintf(st->text, sizeof(st->text), "Firmware mismatch");
    } else if(!st->board_ok) {
        /* the mock says "Starting..." until it links, except in its "board gone" scenario */
        bool starting = app->settings.mock && app->mock_scenario != MockScenarioBoardGone;
        snprintf(st->text, sizeof(st->text), starting ? "Starting..." : "No board");
    } else if(st->lan_up) {
        ip4_format(s->net.lan.ip, st->text, sizeof(st->text));
    } else if(st->wifi_up) {
        ip4_format(s->net.wifi.ip, st->text, sizeof(st->text));
    } else {
        snprintf(st->text, sizeof(st->text), "No network");
    }
}

static void show_message(
    SshRunnerApp* app,
    const char* header,
    bool warn,
    const char* text,
    const char* mono,
    uint32_t next_scene) {
    snprintf(app->msg_header, sizeof(app->msg_header), "%s", header);
    snprintf(app->msg_text, sizeof(app->msg_text), "%s", text ? text : "");
    snprintf(app->msg_mono, sizeof(app->msg_mono), "%s", mono ? mono : "");
    app->msg_warn = warn;
    app->msg_next_scene = next_scene;
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneMessage);
}

void app_show_message(SshRunnerApp* app, const char* header, const char* text, uint32_t next_scene) {
    show_message(app, header, false, text, NULL, next_scene);
}

void app_show_message_mono(
    SshRunnerApp* app,
    const char* header,
    const char* text,
    const char* mono,
    uint32_t next_scene) {
    show_message(app, header, false, text, mono, next_scene);
}

void app_show_error(SshRunnerApp* app, const char* header, const char* text, uint32_t next_scene) {
    show_message(app, header, true, text, NULL, next_scene);
}

/* Output screen: the body is rendered straight into the TextView's buffer (no second
 * copy); this sets the fixed status line. Logs are streamed by store_save_output(). */
void app_output_footer(SshRunnerApp* app, char* footer, size_t footer_size) {
    Session* s = app->session;
    if(s->have_done) {
        char ex[40];
        format_exit_line(ex, sizeof(ex), s->done.exit_code, s->done.flags, s->done.duration_ms);
        bool failed = s->done.exit_code != 0 || s->done.flags != 0;
        snprintf(footer, footer_size, "%s%s", failed ? "Failed: " : "", ex);
        /* a cut in the middle is marked in the body, where it is ("[... 1.2 MB skipped ...]") */
        footer[0] = (char)toupper((unsigned char)footer[0]);
    } else {
        snprintf(footer, footer_size, "Running");
    }
    app->output_dirty = false;
    app->output_rendered_at = furi_get_tick();
}

static SshRunnerApp* app_alloc(void) {
    SshRunnerApp* app = malloc(sizeof(SshRunnerApp));
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->session = malloc(sizeof(Session));
    session_init(app->session);
    app->output_text = furi_string_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ssh_runner_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_event_cb);

    app->home = home_view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewHome, home_view_get_view(app->home));
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewSubmenu, submenu_get_view(app->submenu));
    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ViewVarList, variable_item_list_get_view(app->var_list));
    app->progress = progress_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ViewProgress, progress_view_get_view(app->progress));
    /* No Output buffer of its own: the TextView shows the session's OutBuf in place (this saves
     * a 4 KB copy and its allocation in a fragmented heap) */
    app->text_view = text_view_alloc();

    view_dispatcher_add_view(app->view_dispatcher, ViewText, text_view_get_view(app->text_view));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
    app->loading = loading_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewLoading, loading_get_view(app->loading));
    app->kbd = ssh_kbd_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewKbd, ssh_kbd_get_view(app->kbd));

    store_init(app);
    store_load_settings(app);
    if(!app->settings.examples_seeded) {
        /* first run: the examples where the user has nothing yet, once; deleted ones stay deleted */
        (void)store_examples(app, true);
        app->settings.examples_seeded = true;
        store_save_settings(app);
    }
    store_load_recent(app);

    app_backend_restart(app);
    app->tick_timer = furi_timer_alloc(tick_timer_cb, FuriTimerTypePeriodic, app);
    return app;
}

static void app_free(SshRunnerApp* app) {
    furi_timer_stop(app->tick_timer);
    furi_timer_free(app->tick_timer);
    app_blink_stop(app);
    if(app->script_file) {
        storage_file_close(app->script_file);
        storage_file_free(app->script_file);
    }
    if(app->key_file) {
        storage_file_close(app->key_file);
        storage_file_free(app->key_file);
    }
    /* Disconnects, releases USART1 and re-enables the Expansion service. */
    backend_free(&app->backend);

    view_dispatcher_remove_view(app->view_dispatcher, ViewHome);
    view_dispatcher_remove_view(app->view_dispatcher, ViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, ViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, ViewText);
    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, ViewKbd);
    if(app->wifi_list) view_dispatcher_remove_view(app->view_dispatcher, ViewWifiList);
    home_view_free(app->home);
    submenu_free(app->submenu);
    variable_item_list_free(app->var_list);
    progress_view_free(app->progress);
    text_view_free(app->text_view);
    widget_free(app->widget);
    loading_free(app->loading);
    ssh_kbd_free(app->kbd);
    if(app->wifi_list) wifi_list_view_free(app->wifi_list);
    free(app->scan);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->output_text);
    /* the password buffer may still hold a secret */
    memset(app->input, 0, sizeof(app->input));
    memset(app->gen_pass, 0, sizeof(app->gen_pass));
    free(app->keys);
    free(app->file_buf); /* NULL unless a release was missed */
    free(app->commands);
    if(app->key_in) memset(app->key_in, 0, app->key_in_len);
    free(app->key_in);
    free(app->key_in_line);
    free(app->session);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

/* What the app needs after entry at its peak, measured on the device with the RPC session
 * open (entry free minus the lowest free, spikes included; 2026-10-23): the demo build at most
 * 26.5 KB over all 11 mock walks; the release build 27.3 KB over the hardware key walk (m4
 * 26.2 KB, m5 with 1 MB of output 26.2 KB). The margin is what the app leaves the system while
 * it runs: below need + margin it shows Low memory instead of crashing. */
#ifdef SSH_RUNNER_DEMO_BUILD
#define APP_HEAP_NEED 26600u
#else
#define APP_HEAP_NEED 27500u /* 2026-10-24: at most 27.2 KB (hwkeys), with today's features */
#endif
#define APP_HEAP_MARGIN 8000u
#define MAX_SZ(a, b)    ((a) > (b) ? (a) : (b))

static bool heap_enough(size_t free_now) {
    const HeapNeeds needs = {
        .need = APP_HEAP_NEED,
        .margin = APP_HEAP_MARGIN,
        .biggest = MAX_SZ(sizeof(SshRunnerApp), sizeof(Session)),
    };
    return heap_start_ok(free_now, memmgr_heap_get_max_free_block(), &needs);
}

/* Too little memory: say so with a stock dialog and leave, instead of running out
 * half-way (the Flipper reboots then). */
static void low_memory_dialog(size_t free_now) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* m = dialog_message_alloc();
    dialog_message_set_header(m, "Low Memory", 64, 2, AlignCenter, AlignTop);
    /* the numbers tell a report from a guess (KB, rounded down) */
    char text[72];
    snprintf(
        text,
        sizeof(text),
        "Restart the Flipper\nand try again.\nNeeds %u KB, %u KB free",
        (unsigned)((APP_HEAP_NEED + APP_HEAP_MARGIN) / 1024u),
        (unsigned)(free_now / 1024u));
    dialog_message_set_text(m, text, 64, 34, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, "OK", NULL);
    dialog_message_show(dialogs, m);
    dialog_message_free(m);
    furi_record_close(RECORD_DIALOGS);
}

/* One line per run, appended while the walk tool's marker file exists (never otherwise). */
static void
    heap_report(size_t entry, size_t low, size_t min_entry, size_t min_exit, uint32_t stack_free) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    if(storage_file_exists(st, DATA_DIR "/.demo_heap_report")) {
        File* f = storage_file_alloc(st);
        if(storage_file_open(f, DATA_DIR "/.heap_report", FSAM_WRITE, FSOM_OPEN_APPEND)) {
            char line[96];
            /* stack: the app thread's lowest free stack over the run (bytes), to size
             * stack_size in application.fam */
            int n = snprintf(
                line,
                sizeof(line),
                "entry %u low %u min_entry %u min_exit %u stack %lu\n",
                (unsigned)entry,
                (unsigned)low,
                (unsigned)min_entry,
                (unsigned)min_exit,
                (unsigned long)stack_free);
            if(n > 0) storage_file_write(f, line, (size_t)n);
        }
        storage_file_close(f);
        storage_file_free(f);
    }
    furi_record_close(RECORD_STORAGE);
}

int32_t ssh_runner_app(void* p) {
    UNUSED(p);
    size_t free_now = memmgr_get_free_heap();
    bool force_low = false;
#ifdef SSH_RUNNER_DEMO_BUILD
    /* demo build only: shows the Low memory screen on request, for the screenshot walk */
    Storage* st = furi_record_open(RECORD_STORAGE);
    force_low = storage_file_exists(st, DATA_DIR "/.demo_low_memory");
    /* demo build only: the next Protect secrets fails, for the failure screenshot */
    mock_backend_sec_fail_next(storage_file_exists(st, DATA_DIR "/.demo_sec_fail"));
    furi_record_close(RECORD_STORAGE);
    if(force_low)
        free_now = (APP_HEAP_NEED + APP_HEAP_MARGIN) * 3u / 4u; /* a demo: plausible numbers */
#endif
    if(force_low || !heap_enough(free_now)) {
        low_memory_dialog(free_now);
        return 0;
    }
    heap_low = free_now;
    size_t min_entry = memmgr_get_minimum_free_heap();
    SshRunnerApp* app = app_alloc();
    app->heap_at_start = free_now;
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneHome);
    furi_timer_start(app->tick_timer, furi_ms_to_ticks(TICK_MS));
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    heap_report(
        free_now,
        heap_low,
        min_entry,
        memmgr_get_minimum_free_heap(),
        furi_thread_get_stack_space(furi_thread_get_current_id()));
    return 0;
}
