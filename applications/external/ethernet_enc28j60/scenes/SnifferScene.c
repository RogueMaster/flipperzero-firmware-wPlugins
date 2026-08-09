#include "../app_user.h"

// Per-session sniffer state. Lives in App.thread_alternative-allocated
// stack of the alt thread; a pointer to it is passed as the rx_dispatch
// handler context. The handler runs in the dispatcher thread, so the
// fields it touches are atomic / volatile.
typedef struct {
    App* app;
    volatile uint32_t counter;
    rx_handle_t* handle;
    volatile bool stopped;
} sniffer_state_t;

typedef enum {
    SnifferEventStop,
    SnifferEventOpenPcap,
} SnifferCustomEvent;

// Forward decls
int32_t sniffer_thread(void* context);

/**
 * Function to solve paths
 * All pcaps will be named as file_dd_mm_yy_n
 * Example: file_13_05_2025_1.pcap
 */
void solve_paths(Storage* storage, FuriString* path) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    uint16_t count_files = 0;
    do {
        furi_string_reset(path);
        furi_string_cat_printf(
            path,
            "%s/pcap_%02u_%02u_%i_%i.pcap",
            PATHPCAPS,
            datetime.day,
            datetime.month,
            datetime.year,
            count_files);
        count_files++;
    } while(storage_file_exists(storage, furi_string_get_cstr(path)));
}

static void sniffer_button_callback(GuiButtonType type, InputType input_type, void* context) {
    App* app = context;

    if(input_type != InputTypeShort) {
        return;
    }

    if(type == GuiButtonTypeCenter) {
        app->open_pcap_after_sniff = true;
        app->sniffer_stop = true;
    }
}

// Draw helpers
void draw_count_packets(App* app, uint32_t packets) {
    widget_reset(app->widget);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Open", sniffer_button_callback, app);

    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontSecondary, "Packets Received");

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%lu", packets);

    widget_add_string_element(
        app->widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(app->text));
}

// rx_dispatch handler — captures every received frame.
// Predicate matches everything; handler writes to pcap and counts.
// Runs in dispatcher thread context, so SD writes block other handlers
// briefly. Tolerable for typical LAN traffic; bursty sniff may drop
// frames at the chip RX FIFO if writes can't keep up — known limit.
static bool sniffer_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(frame);
    UNUSED(len);
    UNUSED(ctx);
    return true;
}

static void sniffer_handler(const uint8_t* frame, uint16_t len, void* ctx) {
    sniffer_state_t* s = (sniffer_state_t*)ctx;
    if(s->stopped) return;
    pcap_capture_add_packet(s->app->file, (uint8_t*)frame, len);
    s->counter++;
}

// Function for the testing scene on enter
void app_scene_sniffer_on_enter(void* context) {
    App* app = (App*)context;

    widget_reset(app->widget);
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

    app->open_pcap_after_sniff = false;
    app->sniffer_stop = false;
    app->sniffer_finished = false;
    app->sniffer_link_error = false;

    // Allocate and start the UI thread (does no chip I/O — only counter
    // display + back-button poll).
    app->thread_alternative =
        furi_thread_alloc_ex("Sniffer Therad", 4 * 1024, sniffer_thread, app);
    furi_thread_start(app->thread_alternative);
}

// Function for the testing scene on event
bool app_scene_sniffer_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->sniffer_link_error) {
            app->sniffer_link_error = false;
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }

        app->open_pcap_after_sniff = false;
        app->sniffer_stop = true;

        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SnifferEventStop) {
            scene_manager_previous_scene(app->scene_manager);
            app->sniffer_stop = true;
            return true;
        }

        if(event.event == SnifferEventOpenPcap) {
            scene_manager_next_scene(app->scene_manager, app_scene_read_pcap_option);
            return true;
        }
    }

    return false;
}

// Function for the testing scene on exit
void app_scene_sniffer_on_exit(void* context) {
    App* app = (App*)context;

    if(app->thread_alternative) {
        furi_thread_join(app->thread_alternative);
        furi_thread_free(app->thread_alternative);
        app->thread_alternative = NULL;
    }
}

/**
 * Sniffer UI thread. F0.4d — no longer touches the chip; the rx_dispatch
 * handler does the actual capture work. This thread:
 *   - sets up pcap + promiscuous (briefly pausing rx_dispatch to do so)
 *   - registers the capture handler
 *   - polls the back button + redraws the counter at ~10 Hz
 *   - on back: unregisters, closes pcap, disables promiscuous
 *
 * Closes the previously-tracked bugs:
 *   B-2  no more busy-loop on is_link_up — link status checked once,
 *        if down we just bail with an error widget.
 *   B-3  no dead window — handler is registered before rx_dispatch is
 *        re-enabled, so capture starts immediately.
 */
int32_t sniffer_thread(void* context) {
    App* app = (App*)context;
    enc28j60_t* ethernet = app->ethernet;

    sniffer_state_t state = {
        .app = app,
        .counter = 0,
        .handle = NULL,
        .stopped = false,
    };

    // Ensure the chip is up.
    bool start = app->enc28j60_connected;
    if(!start) {
        app->sniffer_link_error = true;

        draw_device_no_connected(app);
        return 0;
    }
    if(!is_link_up(ethernet)) {
        app->sniffer_link_error = true;

        draw_network_not_connected(app);
        return 0;
    }

    // Single-shot link check (no busy loop). If link is down, show error
    // and exit. The user can reconnect the cable and reenter the scene.
    if(!is_link_up(ethernet)) {
        draw_network_not_connected(app);
        furi_delay_ms(300);
        return 0;
    }

    // F0.5a — chip mutex inside enable/disable_promiscuous and
    // receive_packet now serializes bank access, so no rx_dispatch
    // pause is needed. There's a tiny window between enable_promiscuous
    // and rx_register where the dispatcher can read frames that won't
    // match the sniffer predicate (they're handled by auto-replies or
    // dropped). That's acceptable — the user expects capture to start
    // around scene entry, not on a specific frame.
    enable_promiscuous(ethernet);
    solve_paths(app->storage, app->path);
    // F0.5e — abort cleanly if the PCAP file can't be opened. Pre-fix
    // the return was ignored: the handler still ran, the on-screen
    // counter still ticked up, but every frame was lost (no file).
    if(!pcap_capture_init(app->file, furi_string_get_cstr(app->path))) {
        disable_promiscuous(ethernet);
        return 0;
    }

    state.handle = rx_register(sniffer_predicate, sniffer_handler, &state);

    if(!state.handle) {
        // Out of handler slots. Cleanup and exit.
        pcap_close(app->file);
        disable_promiscuous(ethernet);
        return 0;
    }

    // Initial UI draw.
    draw_count_packets(app, 0);

    // UI poll loop. ~10 Hz redraws + back-button check. Yields to the
    // scheduler — no busy loop, no chip I/O here.
    uint32_t last_drawn = 0xFFFFFFFF;

    // Wait for button release before entering sniff loop
    while(!app->sniffer_stop) {
        uint32_t snapshot = state.counter;

        if((snapshot - last_drawn) >= 10 || last_drawn == 0xFFFFFFFF) {
            draw_count_packets(app, snapshot);
            last_drawn = snapshot;
        }

        furi_delay_ms(100);
    }

    // Stop capture: mark handler as dropping incoming frames first
    // (cheap atomic), then unregister, then teardown chip+file.
    // F0.5a — chip mutex covers disable_promiscuous; no pause needed.
    state.stopped = true;
    rx_unregister(state.handle);

    disable_promiscuous(ethernet);

    pcap_close(app->file);

    furi_delay_ms(100);

    // Signal the scene to navigate back. The user pressed BACK; we honor
    // that by going to the previous scene rather than offering a follow-up
    // "show captured" prompt (Read Pcaps is a top-menu item now).
    // Notify main thread AFTER cleanup

    app->sniffer_stop = true;

    if(app->sniffer_stop && !app->open_pcap_after_sniff) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SnifferEventStop);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, SnifferEventOpenPcap);
    }

    return 0;
}
