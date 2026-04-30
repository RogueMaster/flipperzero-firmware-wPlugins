/* ccid_emulator.c — Main application, UI, and entry point for CCID Emulator.
 * Emulates a USB CCID smart card reader with selectable card profiles. */

#include "ccid_emulator.h"
#include "card_parser.h"
#include "ccid_handler.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Extern preset table from ccid_handler.c
 * --------------------------------------------------------------------------- */

extern const CcidUsbPreset ccid_usb_presets[];
extern const uint8_t ccid_usb_preset_count;

/* =========================================================================
 * APDU Monitor custom View
 * ========================================================================= */

#define APDU_MON_LINE_HEIGHT 10
#define APDU_MON_MAX_VISIBLE 3

typedef struct {
    CcidEmulatorApp* app;
    uint16_t scroll_offset; /* index of first visible *pair* */
    bool auto_scroll; /* follow new entries until user scrolls up */
} ApduMonitorModel;

static void apdu_monitor_draw(Canvas* canvas, void* model_ptr) {
    ApduMonitorModel* model = model_ptr;
    CcidEmulatorApp* app = model->app;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    /* Title bar */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "APDU Monitor");
    canvas_set_color(canvas, ColorBlack);

    furi_mutex_acquire(app->log_mutex, FuriWaitForever);

    uint32_t total = app->log_count;
    if(total > CCID_EMU_LOG_MAX_ENTRIES) total = CCID_EMU_LOG_MAX_ENTRIES;

    if(total == 0) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Waiting for APDUs...");
        furi_mutex_release(app->log_mutex);
        return;
    }

    /* Compute which entries to show. We show the most recent ones first
       (auto-scroll), unless the user has scrolled up. */
    uint16_t visible_lines =
        APDU_MON_MAX_VISIBLE; /* each entry = 2 lines (C> + R>), we show pairs */
    uint16_t max_offset = (total > visible_lines) ? (total - visible_lines) : 0;

    /* Auto-scroll: if offset is at (or beyond) the previous maximum, snap to new max */
    if(model->scroll_offset >= max_offset) {
        model->scroll_offset = max_offset;
    }

    uint16_t start_entry_idx;
    if(app->log_count > CCID_EMU_LOG_MAX_ENTRIES) {
        /* Ring buffer has wrapped. The oldest stored entry index in the
           ring is (app->log_count % CCID_EMU_LOG_MAX_ENTRIES). */
        start_entry_idx =
            (uint16_t)((app->log_count - total + model->scroll_offset) % (uint32_t)CCID_EMU_LOG_MAX_ENTRIES);
    } else {
        start_entry_idx = model->scroll_offset;
    }

    int y = 13;
    for(uint16_t i = 0; i < visible_lines && i + model->scroll_offset < total; i++) {
        uint16_t ring_idx = (start_entry_idx + i) % CCID_EMU_LOG_MAX_ENTRIES;
        const CcidApduLogEntry* entry = &app->log_entries[ring_idx];

        /* Seconds since boot (rough) */
        uint32_t sec = entry->timestamp / 1000;

        /* C> line  (command) */
        char line[80];
        snprintf(line, sizeof(line), "%lu C>%.50s", (unsigned long)sec, entry->command_hex);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, y, line);
        y += APDU_MON_LINE_HEIGHT;
        if(y > 62) break;

        /* R> line  (response) -- highlight if not matched */
        snprintf(
            line, sizeof(line), "  R>%.52s%s", entry->response_hex, entry->matched ? "" : " *");
        canvas_draw_str(canvas, 0, y, line);
        y += APDU_MON_LINE_HEIGHT;
        if(y > 62) break;
    }

    /* Scroll indicator */
    if(total > visible_lines) {
        uint8_t bar_h = 52;
        uint8_t thumb_h = (visible_lines * bar_h) / total;
        if(thumb_h < 3) thumb_h = 3;
        uint8_t thumb_y = 12 + (model->scroll_offset * (bar_h - thumb_h)) / max_offset;
        canvas_draw_box(canvas, 126, thumb_y, 2, thumb_h);
    }

    furi_mutex_release(app->log_mutex);
}

static bool apdu_monitor_input(InputEvent* event, void* context) {
    CcidEmulatorApp* app = context;
    furi_assert(app);

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                app->apdu_monitor,
                ApduMonitorModel * model,
                {
                    if(model->scroll_offset > 0) {
                        model->scroll_offset--;
                        model->auto_scroll = false;
                    }
                },
                true);
            return true;
        } else if(event->key == InputKeyDown) {
            furi_mutex_acquire(app->log_mutex, FuriWaitForever);
            uint32_t total = app->log_count;
            furi_mutex_release(app->log_mutex);
            if(total > CCID_EMU_LOG_MAX_ENTRIES) total = CCID_EMU_LOG_MAX_ENTRIES;
            with_view_model(
                app->apdu_monitor,
                ApduMonitorModel * model,
                {
                    uint16_t max_off =
                        (total > APDU_MON_MAX_VISIBLE) ? total - APDU_MON_MAX_VISIBLE : 0;
                    if(model->scroll_offset < max_off) model->scroll_offset++;
                    if(model->scroll_offset >= max_off) model->auto_scroll = true;
                },
                true);
            return true;
        } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
            /* Right (short) -> export APDU log to SD card */
            view_dispatcher_send_custom_event(app->view_dispatcher, CcidEmulatorEventExportLog);
            return true;
        }
    }

    /* Let Back propagate to ViewDispatcher so navigation works */
    return false;
}

/* =========================================================================
 * APDU Log Export
 * ========================================================================= */

/**
 * Dump the current in-memory APDU ring buffer to a timestamped file on the SD
 * card under /ext/ccid_emulator/logs/.
 *
 * Format per entry:
 *   [001] CMD: 00 A4 04 00 -> RSP: 6F 19 ... 90 00 (matched)
 *   [002] CMD: 00 B2 01 0C -> RSP: 6A 82 (default)
 *
 * Returns true on success, false on any I/O error.
 */
static bool ccid_emulator_export_log(CcidEmulatorApp* app) {
    furi_assert(app);

    furi_mutex_acquire(app->log_mutex, FuriWaitForever);

    uint32_t total = app->log_count;
    uint16_t stored = (total > CCID_EMU_LOG_MAX_ENTRIES) ? CCID_EMU_LOG_MAX_ENTRIES : (uint16_t)total;

    if(stored == 0) {
        furi_mutex_release(app->log_mutex);
        return false;
    }

    /* Ensure logs directory exists */
    storage_simply_mkdir(app->storage, CCID_EMU_LOGS_DIR);

    /* Build timestamped file path */
    DateTime dt;
    datetime_timestamp_to_datetime(furi_hal_rtc_get_timestamp(), &dt);

    char path[128];
    snprintf(
        path,
        sizeof(path),
        CCID_EMU_LOGS_DIR "/apdu_%04d%02d%02d_%02d%02d%02d.log",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    Stream* stream = file_stream_alloc(app->storage);
    bool ok = file_stream_open(stream, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) {
        FURI_LOG_E("CcidApp", "Cannot create log file: %s", path);
        stream_free(stream);
        furi_mutex_release(app->log_mutex);
        return false;
    }

    /* Write header */
    char header[128];
    snprintf(
        header,
        sizeof(header),
        "# CCID APDU log -- %04d-%02d-%02d %02d:%02d:%02d\n"
        "# %u entr%s\n\n",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second,
        stored,
        stored == 1 ? "y" : "ies");
    stream_write_cstring(stream, header);

    /* The ring buffer oldest entry when it has wrapped:
       ring_start = log_count % LOG_MAX_ENTRIES           (wrapped case)
       ring_start = 0                                     (not yet wrapped)  */
    uint16_t ring_start = (app->log_count > CCID_EMU_LOG_MAX_ENTRIES) ?
                              (uint16_t)(app->log_count % (uint32_t)CCID_EMU_LOG_MAX_ENTRIES) :
                              0;

    /* Write entries using fixed-size prefix + streamed hex strings to avoid
       large stack allocations (CCID_EMU_MAX_HEX_STR can be ~1536 bytes). */
    for(uint16_t i = 0; i < stored; i++) {
        uint16_t ring_idx = (ring_start + i) % CCID_EMU_LOG_MAX_ENTRIES;
        const CcidApduLogEntry* e = &app->log_entries[ring_idx];

        char prefix[16];
        snprintf(prefix, sizeof(prefix), "[%03u] CMD: ", (unsigned)(i + 1));
        stream_write_cstring(stream, prefix);
        stream_write_cstring(stream, e->command_hex);
        stream_write_cstring(stream, " -> RSP: ");
        stream_write_cstring(stream, e->response_hex);
        stream_write_cstring(stream, e->matched ? " (matched)\n" : " (default)\n");
    }

    stream_free(stream);
    furi_mutex_release(app->log_mutex);

    FURI_LOG_I("CcidApp", "APDU log exported to %s (%u entries)", path, stored);
    return true;
}

/* =========================================================================
 * Card file discovery
 * ========================================================================= */

static void discover_card_files(CcidEmulatorApp* app) {
    /* Free any previous list */
    if(app->card_paths) {
        for(uint16_t i = 0; i < app->card_path_count; i++) {
            free(app->card_paths[i]);
        }
        free(app->card_paths);
        app->card_paths = NULL;
        app->card_path_count = 0;
    }

    File* dir = storage_file_alloc(app->storage);
    if(!storage_dir_open(dir, CCID_EMU_CARDS_DIR)) {
        FURI_LOG_W("CcidApp", "Cannot open cards directory");
        storage_dir_close(dir);
        storage_file_free(dir);
        return;
    }

    /* First pass: count .ccid files.
       Note: We skip the FileInfo parameter (pass NULL) and only match by
       file extension, avoiding the non-existent FileInfo.flags field. */
    char name_buf[256];
    uint16_t count = 0;

    while(storage_dir_read(dir, NULL, name_buf, sizeof(name_buf))) {
        size_t nlen = strlen(name_buf);
        if(nlen > 5 && strcmp(name_buf + nlen - 5, ".ccid") == 0) {
            count++;
        }
    }

    if(count == 0) {
        storage_dir_close(dir);
        storage_file_free(dir);
        return;
    }

    app->card_paths = malloc(sizeof(char*) * count);
    if(!app->card_paths) {
        FURI_LOG_E("CcidApp", "OOM allocating card_paths");
        storage_dir_close(dir);
        storage_file_free(dir);
        return;
    }
    app->card_path_count = 0;

    /* Rewind directory and collect paths */
    storage_dir_close(dir);
    storage_dir_open(dir, CCID_EMU_CARDS_DIR);

    while(storage_dir_read(dir, NULL, name_buf, sizeof(name_buf))) {
        size_t nlen = strlen(name_buf);
        if(nlen > 5 && strcmp(name_buf + nlen - 5, ".ccid") == 0) {
            /* Build full path */
            size_t path_len = strlen(CCID_EMU_CARDS_DIR) + 1 + nlen + 1;
            char* path = malloc(path_len);
            if(!path) {
                FURI_LOG_E("CcidApp", "OOM allocating card path");
                break;
            }
            snprintf(path, path_len, "%s/%s", CCID_EMU_CARDS_DIR, name_buf);
            app->card_paths[app->card_path_count++] = path;
            if(app->card_path_count >= count) break;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);

    FURI_LOG_I("CcidApp", "Found %u .ccid files", app->card_path_count);
}

/* =========================================================================
 * Card info Widget builder
 * ========================================================================= */

/* Forward declaration -- callback defined after the submenu section */
static void card_info_button_callback(GuiButtonType result, InputType type, void* context);

static void build_card_info_widget(CcidEmulatorApp* app) {
    widget_reset(app->card_info);

    if(!app->card) {
        widget_add_string_element(
            app->card_info, 64, 32, AlignCenter, AlignCenter, FontSecondary, "No card loaded");
        return;
    }

    /* Card name */
    widget_add_string_element(
        app->card_info, 64, 2, AlignCenter, AlignTop, FontPrimary, app->card->name);

    /* Description */
    if(app->card->description[0]) {
        widget_add_string_element(
            app->card_info, 64, 16, AlignCenter, AlignTop, FontSecondary, app->card->description);
    }

    /* ATR */
    char atr_str[CCID_EMU_MAX_ATR_LEN * 3 + 8];
    {
        int pos = snprintf(atr_str, sizeof(atr_str), "ATR: ");
        for(uint8_t i = 0; i < app->card->atr_len && pos + 3 < (int)sizeof(atr_str); i++) {
            if(i > 0) atr_str[pos++] = ' ';
            snprintf(atr_str + pos, sizeof(atr_str) - pos, "%02X", app->card->atr[i]);
            pos += 2;
        }
        atr_str[pos] = '\0';
    }
    widget_add_string_element(app->card_info, 0, 28, AlignLeft, AlignTop, FontSecondary, atr_str);

    /* Rule count */
    char rules_str[32];
    snprintf(rules_str, sizeof(rules_str), "Rules: %u", app->card->rule_count);
    widget_add_string_element(
        app->card_info, 0, 40, AlignLeft, AlignTop, FontSecondary, rules_str);

    /* Instruction */
    widget_add_string_element(
        app->card_info, 64, 55, AlignCenter, AlignBottom, FontSecondary, "Press OK to activate");

    /* Re-add button element (widget_reset clears all elements) */
    widget_add_button_element(
        app->card_info, GuiButtonTypeCenter, "Activate", card_info_button_callback, app);
}

/* =========================================================================
 * Submenu (card browser) callbacks
 * ========================================================================= */

static void card_browser_callback(void* context, uint32_t index) {
    CcidEmulatorApp* app = context;
    furi_assert(app);

    if(index >= app->card_path_count) return;

    /* Free previously loaded card */
    if(app->card) {
        ccid_card_free(app->card);
        app->card = NULL;
    }

    app->card = ccid_card_load(app->storage, app->card_paths[index]);
    if(!app->card) {
        FURI_LOG_E("CcidApp", "Failed to load card from %s", app->card_paths[index]);
        return;
    }

    build_card_info_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CcidEmulatorViewCardInfo);
}

/* =========================================================================
 * Navigation callbacks
 * ========================================================================= */

static uint32_t nav_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t nav_to_browser_callback(void* context) {
    UNUSED(context);
    return CcidEmulatorViewCardBrowser;
}

/* nav_to_card_info_callback removed — unused */

/* =========================================================================
 * Card Info widget callback (OK button -> activate emulation)
 * ========================================================================= */

static void card_info_button_callback(GuiButtonType result, InputType type, void* context) {
    CcidEmulatorApp* app = context;
    if(type != InputTypeShort) return;

    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CcidEmulatorEventActivateCard);
    }
}

/* =========================================================================
 * Settings (Variable Item List)
 * ========================================================================= */

static void settings_build(CcidEmulatorApp* app) {
    variable_item_list_reset(app->settings);

    /* USB VID/PID: SDK usb_ccid interface does not support custom descriptors.
     * Show a single non-toggleable label so users know it's fixed. */
    VariableItem* item = variable_item_list_add(app->settings, "USB Device", 1, NULL, app);
    variable_item_set_current_value_text(item, "Default (SDK fixed)");
    UNUSED(app->usb_preset_index); /* retained for future SDK support */
}

/* =========================================================================
 * Submenu population
 * ========================================================================= */

/* populate_card_browser superseded by populate_card_browser_final */

static void browser_submenu_callback_wrapper(void* context, uint32_t index) {
    CcidEmulatorApp* app = context;

    if(index == 0xFFFE) {
        /* Settings */
        view_dispatcher_switch_to_view(app->view_dispatcher, CcidEmulatorViewSettings);
        return;
    }

    card_browser_callback(context, index);
}

static void populate_card_browser_final(CcidEmulatorApp* app) {
    submenu_reset(app->card_browser);

    for(uint16_t i = 0; i < app->card_path_count; i++) {
        const char* slash = strrchr(app->card_paths[i], '/');
        const char* display_name = slash ? slash + 1 : app->card_paths[i];
        submenu_add_item(
            app->card_browser, display_name, i, browser_submenu_callback_wrapper, app);
    }

    if(app->card_path_count == 0) {
        submenu_add_item(
            app->card_browser,
            "(no .ccid files found)",
            0xFFFF,
            browser_submenu_callback_wrapper,
            app);
    }

    submenu_add_item(
        app->card_browser, "[Settings]", 0xFFFE, browser_submenu_callback_wrapper, app);
}

/* =========================================================================
 * ViewDispatcher custom event handler
 * ========================================================================= */

static bool custom_event_handler(void* context, uint32_t event) {
    CcidEmulatorApp* app = context;

    switch(event) {
    case CcidEmulatorEventActivateCard:
        if(app->card && !app->emulating) {
            /* Reset log */
            furi_mutex_acquire(app->log_mutex, FuriWaitForever);
            app->log_count = 0;
            furi_mutex_release(app->log_mutex);

            /* Reset scroll */
            with_view_model(
                app->apdu_monitor,
                ApduMonitorModel * model,
                {
                    model->scroll_offset = 0;
                    model->auto_scroll = true;
                },
                false);

            /* Start CCID emulation */
            ccid_handler_start(app);

            /* Switch to APDU monitor view */
            view_dispatcher_switch_to_view(app->view_dispatcher, CcidEmulatorViewApduMonitor);
        }
        return true;

    case CcidEmulatorEventApduExchange:
        /* Legacy event — no longer sent from the USB callback.  The
         * apdu_refresh_timer now handles redraws.  Keep the case to avoid
         * compiler warnings about the enum value. */
        return true;

    case CcidEmulatorEventExportLog: {
        /* Export APDU log to /ext/ccid_emulator/logs/ and vibrate on success */
        bool exported = ccid_emulator_export_log(app);
        NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
        if(exported) {
            notification_message(notif, &sequence_success);
        } else {
            notification_message(notif, &sequence_error);
        }
        furi_record_close(RECORD_NOTIFICATION);
        return true;
    }

    default:
        return false;
    }
}

/* =========================================================================
 * Navigation callback for APDU Monitor -- stop emulation on Back
 * ========================================================================= */

static uint32_t apdu_monitor_back_callback(void* context) {
    UNUSED(context);
    /* Returning the card info view; the ViewDispatcher will call us before
       switching.  We rely on the navigation event handler to stop
       emulation. */
    return CcidEmulatorViewCardBrowser;
}

static bool navigation_event_handler(void* context) {
    CcidEmulatorApp* app = context;

    /* If we are currently emulating, stop on any Back navigation */
    if(app->emulating) {
        ccid_handler_stop(app);
    }

    /* Return false to allow the ViewDispatcher to handle view switching */
    return false;
}

/* =========================================================================
 * App alloc / free
 * ========================================================================= */

/* =========================================================================
 * APDU monitor refresh timer callback — polls log_dirty flag every 200 ms.
 *
 * Replaces the direct view_dispatcher_send_custom_event() that used to be
 * called from the USB CCID callback thread (which could overflow the event
 * queue and cause furi_check failures under heavy APDU load).
 * ========================================================================= */
static void ccid_apdu_refresh_timer_cb(void* ctx) {
    CcidEmulatorApp* app = (CcidEmulatorApp*)ctx;

    if(!app->log_dirty) return;
    app->log_dirty = false;

    if(app->apdu_monitor) {
        with_view_model(
            app->apdu_monitor,
            ApduMonitorModel * model,
            {
                if(model->auto_scroll) {
                    uint32_t total = app->log_count;
                    if(total > CCID_EMU_LOG_MAX_ENTRIES) total = CCID_EMU_LOG_MAX_ENTRIES;
                    uint16_t max_off =
                        (total > APDU_MON_MAX_VISIBLE) ? total - APDU_MON_MAX_VISIBLE : 0;
                    model->scroll_offset = max_off;
                }
            },
            true);
    }
}

static CcidEmulatorApp* ccid_emulator_app_alloc(void) {
    CcidEmulatorApp* app = malloc(sizeof(CcidEmulatorApp));
    furi_assert(app);
    memset(app, 0, sizeof(CcidEmulatorApp));

    /* Storage */
    app->storage = furi_record_open(RECORD_STORAGE);

    /* Ensure directory tree exists and write sample cards on first run */
    storage_simply_mkdir(app->storage, EXT_PATH("ccid_emulator"));
    storage_simply_mkdir(app->storage, CCID_EMU_CARDS_DIR);
    storage_simply_mkdir(app->storage, CCID_EMU_LOGS_DIR);
    ccid_card_write_sample(app->storage);

    /* GUI */
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    /* view_dispatcher_enable_queue is deprecated in SDK 1.4+ — no longer needed */
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_handler);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_event_handler);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* --- Card Browser (Submenu) --- */
    app->card_browser = submenu_alloc();
    submenu_set_header(app->card_browser, "CCID Emulator");
    View* browser_view = submenu_get_view(app->card_browser);
    view_set_previous_callback(browser_view, nav_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, CcidEmulatorViewCardBrowser, browser_view);

    /* --- Card Info (Widget) --- */
    app->card_info = widget_alloc();
    View* info_view = widget_get_view(app->card_info);
    view_set_previous_callback(info_view, nav_to_browser_callback);
    view_dispatcher_add_view(app->view_dispatcher, CcidEmulatorViewCardInfo, info_view);

    /* --- APDU Monitor (custom View) --- */
    app->apdu_monitor = view_alloc();
    view_allocate_model(app->apdu_monitor, ViewModelTypeLocking, sizeof(ApduMonitorModel));
    view_set_draw_callback(app->apdu_monitor, apdu_monitor_draw);
    view_set_input_callback(app->apdu_monitor, apdu_monitor_input);
    view_set_context(app->apdu_monitor, app);
    view_set_previous_callback(app->apdu_monitor, apdu_monitor_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, CcidEmulatorViewApduMonitor, app->apdu_monitor);

    /* Initialize model */
    with_view_model(
        app->apdu_monitor,
        ApduMonitorModel * model,
        {
            model->app = app;
            model->scroll_offset = 0;
            model->auto_scroll = true;
        },
        false);

    /* --- Settings (VariableItemList) --- */
    app->settings = variable_item_list_alloc();
    View* settings_view = variable_item_list_get_view(app->settings);
    view_set_previous_callback(settings_view, nav_to_browser_callback);
    view_dispatcher_add_view(app->view_dispatcher, CcidEmulatorViewSettings, settings_view);

    /* Log mutex */
    app->log_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    /* APDU monitor refresh timer — 200 ms periodic, polls log_dirty flag. */
    app->apdu_refresh_timer =
        furi_timer_alloc(ccid_apdu_refresh_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->apdu_refresh_timer, furi_ms_to_ticks(200));

    /* Default preset */
    app->usb_preset_index = 0;

    /* Build settings */
    settings_build(app);

    return app;
}

static void ccid_emulator_app_free(CcidEmulatorApp* app) {
    furi_assert(app);

    /* Stop emulation if active */
    if(app->emulating) {
        ccid_handler_stop(app);
    }

    /* Remove views before freeing modules */
    view_dispatcher_remove_view(app->view_dispatcher, CcidEmulatorViewCardBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, CcidEmulatorViewCardInfo);
    view_dispatcher_remove_view(app->view_dispatcher, CcidEmulatorViewApduMonitor);
    view_dispatcher_remove_view(app->view_dispatcher, CcidEmulatorViewSettings);

    /* Stop the refresh timer FIRST — it accesses apdu_monitor via
     * with_view_model, so it must be stopped before freeing views. */
    if(app->apdu_refresh_timer) {
        furi_timer_stop(app->apdu_refresh_timer);
        furi_timer_free(app->apdu_refresh_timer);
        app->apdu_refresh_timer = NULL;
    }

    /* Free modules */
    submenu_free(app->card_browser);
    widget_free(app->card_info);
    view_free(app->apdu_monitor);
    variable_item_list_free(app->settings);

    view_dispatcher_free(app->view_dispatcher);

    /* Free card */
    if(app->card) {
        ccid_card_free(app->card);
        app->card = NULL;
    }

    /* Free card paths */
    if(app->card_paths) {
        for(uint16_t i = 0; i < app->card_path_count; i++) {
            free(app->card_paths[i]);
        }
        free(app->card_paths);
    }

    /* Mutex — freed after timer is stopped to avoid timer callback
     * accessing freed mutex via ccid_xfr_datablock path. */
    furi_mutex_free(app->log_mutex);

    /* Close records */
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

/* =========================================================================
 * Entry point
 * ========================================================================= */

int32_t ccid_emulator_app(void* p) {
    UNUSED(p);

    CcidEmulatorApp* app = ccid_emulator_app_alloc();

    /* Discover .ccid files and populate the browser */
    discover_card_files(app);
    populate_card_browser_final(app);

    /* Start on the card browser */
    view_dispatcher_switch_to_view(app->view_dispatcher, CcidEmulatorViewCardBrowser);
    view_dispatcher_run(app->view_dispatcher);

    ccid_emulator_app_free(app);

    return 0;
}
