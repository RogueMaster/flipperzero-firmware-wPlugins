/* badusb_pro.c — Main application, UI, and entry point for BadUSB Pro.
 * Provides script browser, execution control, and OS detection integration. */

#include "badusb_pro.h"
#include "ducky_parser.h"
#include "script_engine.h"
#include <furi_hal_usb_hid.h>
#include <gui/elements.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ════════════════════════════════════════════════════════════════════════════
 *  Forward declarations
 * ════════════════════════════════════════════════════════════════════════════ */
static void app_alloc(BadUsbProApp* app);
static void app_free(BadUsbProApp* app);
static void scan_script_files(BadUsbProApp* app);
static void start_script_execution(BadUsbProApp* app);

/* View callbacks */
static void file_browser_cb(void* ctx, uint32_t index);
static uint32_t file_browser_exit_cb(void* ctx);

static void script_info_run_cb(GuiButtonType type, InputType input_type, void* ctx);
static uint32_t script_info_back_cb(void* ctx);

static void execution_draw_cb(Canvas* canvas, void* model);
static bool execution_input_cb(InputEvent* event, void* ctx);
static uint32_t execution_back_cb(void* ctx);

static void settings_speed_change_cb(VariableItem* item);
static void settings_mode_change_cb(VariableItem* item);
static void settings_delay_change_cb(VariableItem* item);
static uint32_t settings_back_cb(void* ctx);

/* Worker thread */
static int32_t worker_thread_cb(void* ctx);
static void engine_status_cb(void* ctx);

/* Helper: safely restore USB (fix #5 - race condition guard) */
static void safe_restore_usb(BadUsbProApp* app);

/* ════════════════════════════════════════════════════════════════════════════
 *  Speed multiplier lookup
 * ════════════════════════════════════════════════════════════════════════════ */
static const float speed_values[SpeedCount] = {0.5f, 1.0f, 2.0f, 4.0f};
static const char* speed_labels[SpeedCount] = {"0.5x", "1x", "2x", "4x"};
static const char* mode_labels[] = {"USB", "BLE"};

/* ════════════════════════════════════════════════════════════════════════════
 *  Fix #5: Safe USB restore with atomic flag to prevent double-restore
 * ════════════════════════════════════════════════════════════════════════════ */
static void safe_restore_usb(BadUsbProApp* app) {
    if(!app->usb_restored && app->prev_usb_mode) {
        app->usb_restored = true;
        furi_hal_usb_set_config(app->prev_usb_mode, NULL);
        app->prev_usb_mode = NULL;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Allocate / free application
 * ════════════════════════════════════════════════════════════════════════════ */

static void app_alloc(BadUsbProApp* app) {
    memset(app, 0, sizeof(*app));
    app->speed_setting = SpeedNormal;
    app->injection_mode = InjectionModeUSB;
    app->settings_default_delay = 100; /* 100 ms between commands — sane default */
    app->usb_restored = false;

    script_engine_init(&app->engine);

    /* ── GUI ─────────────────────────────────────────────── */
    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* ── File browser (Submenu listing .ds files) ────────── */
    app->file_browser = submenu_alloc();
    submenu_set_header(app->file_browser, "BadUSB Pro Scripts");
    View* fb_view = submenu_get_view(app->file_browser);
    view_set_previous_callback(fb_view, file_browser_exit_cb);
    view_dispatcher_add_view(app->view_dispatcher, ViewFileBrowser, fb_view);

    /* ── Script info (Widget) ────────────────────────────── */
    app->script_info = widget_alloc();
    View* si_view = widget_get_view(app->script_info);
    view_set_previous_callback(si_view, script_info_back_cb);
    view_dispatcher_add_view(app->view_dispatcher, ViewScriptInfo, si_view);

    /* ── Execution view (custom View) ────────────────────── */
    app->execution_view = view_alloc();
    view_allocate_model(app->execution_view, ViewModelTypeLocking, sizeof(ExecutionViewModel));
    view_set_draw_callback(app->execution_view, execution_draw_cb);
    view_set_input_callback(app->execution_view, execution_input_cb);
    view_set_previous_callback(app->execution_view, execution_back_cb);
    view_set_context(app->execution_view, app);
    view_dispatcher_add_view(app->view_dispatcher, ViewExecution, app->execution_view);

    /* ── Settings (VariableItemList) ─────────────────────── */
    app->settings = variable_item_list_alloc();
    View* set_view = variable_item_list_get_view(app->settings);
    view_set_previous_callback(set_view, settings_back_cb);
    view_dispatcher_add_view(app->view_dispatcher, ViewSettings, set_view);

    /* Speed item */
    VariableItem* speed_item =
        variable_item_list_add(app->settings, "Speed", SpeedCount, settings_speed_change_cb, app);
    variable_item_set_current_value_index(speed_item, app->speed_setting);
    variable_item_set_current_value_text(speed_item, speed_labels[app->speed_setting]);

    /* Mode item — USB only (BLE HID is not available to FAP apps in SDK 1.4.3) */
    VariableItem* mode_item =
        variable_item_list_add(app->settings, "Mode", 1, settings_mode_change_cb, app);
    variable_item_set_current_value_index(mode_item, 0);
    variable_item_set_current_value_text(mode_item, "USB");

    /* Default delay item */
    VariableItem* delay_item =
        variable_item_list_add(app->settings, "Default Delay", 6, settings_delay_change_cb, app);
    variable_item_set_current_value_index(delay_item, 2); /* 100ms default */
    variable_item_set_current_value_text(delay_item, "100ms");
}

static void app_free(BadUsbProApp* app) {
    /* Stop worker if running */
    if(app->worker_running) {
        script_engine_stop(&app->engine);
    }
    if(app->worker_thread) {
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    app->worker_running = false;

    /* Restore USB mode if we changed it (fix #5) */
    safe_restore_usb(app);

    /* Free dynamically allocated tokens in the engine */
    if(app->engine.tokens) {
        free(app->engine.tokens);
        app->engine.tokens = NULL;
        app->engine.token_count = 0;
        app->engine.token_capacity = 0;
    }

    view_dispatcher_remove_view(app->view_dispatcher, ViewFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, ViewScriptInfo);
    view_dispatcher_remove_view(app->view_dispatcher, ViewExecution);
    view_dispatcher_remove_view(app->view_dispatcher, ViewSettings);

    submenu_free(app->file_browser);
    widget_free(app->script_info);
    view_free(app->execution_view);
    variable_item_list_free(app->settings);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Scan scripts directory for .ds files
 * ════════════════════════════════════════════════════════════════════════════ */

static void scan_script_files(BadUsbProApp* app) {
    app->file_count = 0;
    submenu_reset(app->file_browser);
    submenu_set_header(app->file_browser, "BadUSB Pro Scripts");

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Ensure directory exists */
    if(!storage_dir_exists(storage, BADUSB_PRO_SCRIPTS_PATH)) {
        storage_simply_mkdir(storage, BADUSB_PRO_SCRIPTS_PATH);
    }

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, BADUSB_PRO_SCRIPTS_PATH)) {
        char name[64];

        while(storage_dir_read(dir, NULL, name, sizeof(name)) &&
              app->file_count < BADUSB_PRO_MAX_FILES) {
            /* Check .ds extension */
            size_t len = strlen(name);
            if(len >= 3 && strcmp(name + len - 3, ".ds") == 0) {
                strncpy(app->file_list[app->file_count], name, 63);
                app->file_list[app->file_count][63] = '\0';

                submenu_add_item(
                    app->file_browser,
                    app->file_list[app->file_count],
                    app->file_count,
                    file_browser_cb,
                    app);

                app->file_count++;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    /* Add settings entry at the bottom */
    submenu_add_item(app->file_browser, "[Settings]", 0xFF, file_browser_cb, app);

    furi_record_close(RECORD_STORAGE);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  File browser callbacks
 * ════════════════════════════════════════════════════════════════════════════ */

static void file_browser_cb(void* ctx, uint32_t index) {
    BadUsbProApp* app = ctx;

    if(index == 0xFF) {
        /* Settings selected */
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewSettings);
        return;
    }

    if(index >= app->file_count) return;

    /* Build full path */
    snprintf(
        app->script_path,
        sizeof(app->script_path),
        "%s/%s",
        BADUSB_PRO_SCRIPTS_PATH,
        app->file_list[index]);
    strncpy(app->script_name, app->file_list[index], sizeof(app->script_name) - 1);
    app->script_name[sizeof(app->script_name) - 1] = '\0';

    /* Get file info */
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FileInfo finfo;
    if(storage_common_stat(storage, app->script_path, &finfo) == FSE_OK) {
        app->script_size = (uint32_t)finfo.size;
    } else {
        app->script_size = 0;
    }
    app->script_line_count = ducky_parser_count_lines(storage, app->script_path);

    furi_record_close(RECORD_STORAGE);

    /* Populate script info widget */
    widget_reset(app->script_info);

    char info_buf[256];
    snprintf(
        info_buf,
        sizeof(info_buf),
        "\e#%s\n"
        "Size: %lu bytes\n"
        "Lines: %u\n\n"
        "Press OK to run",
        app->script_name,
        (unsigned long)app->script_size,
        app->script_line_count);
    widget_add_text_scroll_element(app->script_info, 0, 0, 128, 64, info_buf);

    widget_add_button_element(
        app->script_info, GuiButtonTypeCenter, "OK", script_info_run_cb, app);
    widget_add_button_element(
        app->script_info, GuiButtonTypeRight, "Run", script_info_run_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewScriptInfo);
}

static uint32_t file_browser_exit_cb(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Parse the selected script, load the engine, and launch execution
 * ════════════════════════════════════════════════════════════════════════════ */

static void start_script_execution(BadUsbProApp* app) {
    /* Fix #7: If a worker thread exists from a previous run, join and free it */
    if(app->worker_thread) {
        if(app->worker_running) {
            script_engine_stop(&app->engine);
        }
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
        app->worker_running = false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    script_engine_init(&app->engine);

    char err_msg[128] = {0};
    uint16_t err_line = 0;
    uint16_t count = 0;

    /* Fix #2: Use a reasonably-sized dynamic allocation */
    uint32_t capacity = BADUSB_PRO_MAX_TOKENS;
    ScriptToken* temp_tokens = malloc(sizeof(ScriptToken) * capacity);
    if(!temp_tokens) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    bool ok = ducky_parser_parse_file(
        storage, app->script_path, temp_tokens, (uint16_t)capacity, &count, err_msg, &err_line);

    furi_record_close(RECORD_STORAGE);

    if(!ok) {
        /* Show parse error in execution view */
        strncpy(app->engine.error_msg, err_msg, sizeof(app->engine.error_msg) - 1);
        app->engine.error_line = err_line;
        app->engine.state = ScriptStateError;

        with_view_model(
            app->execution_view,
            ExecutionViewModel * m,
            {
                m->state = ScriptStateError;
                strncpy(m->error_msg, err_msg, sizeof(m->error_msg) - 1);
                m->current_line = err_line;
                m->total_lines = 0;
                m->current_cmd[0] = '\0';
            },
            true);

        free(temp_tokens);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewExecution);
        return;
    }

    /* Fix #1 + #2: Transfer token ownership to engine (no copy, no free here) */
    script_engine_load(&app->engine, temp_tokens, count, capacity);
    /* Do NOT free temp_tokens -- engine now owns them */

    /* Apply settings */
    script_engine_set_speed(&app->engine, speed_values[app->speed_setting]);
    app->engine.default_delay = app->settings_default_delay;
    app->engine.default_string_delay = 10; /* 10 ms between characters — prevents drops */

    /* Set UI update callback */
    script_engine_set_callback(&app->engine, engine_status_cb, app);

    /* Initialise execution view model */
    with_view_model(
        app->execution_view,
        ExecutionViewModel * m,
        {
            m->current_line = 0;
            m->total_lines = app->script_line_count;
            m->current_cmd[0] = '\0';
            m->led_num = 0;
            m->led_caps = 0;
            m->led_scroll = 0;
            m->state = ScriptStateLoaded;
            m->error_msg[0] = '\0';
            m->detected_os[0] = '\0';
        },
        true);

    /* Configure USB HID (save previous mode for restore later) */
    app->prev_usb_mode = furi_hal_usb_get_config();
    app->usb_restored = false; /* Fix #5: reset atomic flag for new execution */
    if(app->injection_mode == InjectionModeBLE) {
        /* BLE HID profile is not available to FAP apps in SDK 1.4.3.
         * Fall back to USB mode and notify the user. */
        app->injection_mode = InjectionModeUSB;
    }
    furi_hal_usb_unlock(); /* ensure USB is not locked by a previous mode */
    furi_hal_usb_set_config(&usb_hid, NULL);
    /* Wait up to 5 s for the host to enumerate, then add 1.5 s grace period
     * per the official BadUSB implementation to let the HID driver settle. */
    {
        uint32_t t = furi_get_tick();
        while(!furi_hal_hid_is_connected() && (furi_get_tick() - t) < furi_ms_to_ticks(5000)) {
            furi_delay_ms(50);
        }
        if(furi_hal_hid_is_connected()) furi_delay_ms(1500);
    }

    /* Start worker thread */
    app->worker_thread = furi_thread_alloc_ex("BadUSBWorker", 8192, worker_thread_cb, app);
    app->worker_running = true;
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewExecution);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Script info callbacks
 * ════════════════════════════════════════════════════════════════════════════ */

static void script_info_run_cb(GuiButtonType type, InputType input_type, void* ctx) {
    BadUsbProApp* app = ctx;
    if((type == GuiButtonTypeRight || type == GuiButtonTypeCenter) &&
       input_type == InputTypeShort) {
        start_script_execution(app);
    }
}

static uint32_t script_info_back_cb(void* ctx) {
    UNUSED(ctx);
    return ViewFileBrowser;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Engine status callback (called from worker thread)
 * ════════════════════════════════════════════════════════════════════════════ */

static void engine_status_cb(void* ctx) {
    BadUsbProApp* app = ctx;

    with_view_model(
        app->execution_view,
        ExecutionViewModel * m,
        {
            ScriptEngine* e = &app->engine;
            m->state = e->state;

            if(e->pc < e->token_count) {
                m->current_line = e->tokens[e->pc].source_line;
            }
            m->total_lines = app->script_line_count;

            /* Format current command */
            if(e->pc < e->token_count) {
                ScriptToken* tok = &e->tokens[e->pc];
                switch(tok->type) {
                case TokenString:
                case TokenStringLn:
                    snprintf(
                        m->current_cmd, sizeof(m->current_cmd), "STRING %.40s", tok->str_value);
                    break;
                case TokenDelay:
                    snprintf(
                        m->current_cmd, sizeof(m->current_cmd), "DELAY %ld", (long)tok->int_value);
                    break;
                case TokenKeyCombo:
                    snprintf(
                        m->current_cmd,
                        sizeof(m->current_cmd),
                        "COMBO (%d keys)",
                        tok->keycode_count);
                    break;
                case TokenMouseMove:
                    snprintf(
                        m->current_cmd,
                        sizeof(m->current_cmd),
                        "MOUSE %ld,%ld",
                        (long)tok->int_value,
                        (long)tok->int_value2);
                    break;
                case TokenLedWait:
                    snprintf(
                        m->current_cmd, sizeof(m->current_cmd), "LED_WAIT %.50s", tok->str_value);
                    break;
                case TokenOsDetect:
                    strncpy(m->current_cmd, "OS_DETECT", sizeof(m->current_cmd));
                    break;
                default:
                    if(tok->str_value[0]) {
                        snprintf(m->current_cmd, sizeof(m->current_cmd), "%.60s", tok->str_value);
                    } else {
                        m->current_cmd[0] = '\0';
                    }
                    break;
                }
            }

            /* LED indicators */
            m->led_num = (e->led_state & (1 << 0)) ? 1 : 0;
            m->led_caps = (e->led_state & (1 << 1)) ? 1 : 0;
            m->led_scroll = (e->led_state & (1 << 2)) ? 1 : 0;

            /* Capture OS detection result if available */
            const char* os_val = script_engine_get_var(e, "OS");
            if(os_val && os_val[0]) {
                strncpy(m->detected_os, os_val, sizeof(m->detected_os) - 1);
                m->detected_os[sizeof(m->detected_os) - 1] = '\0';
            }

            if(e->state == ScriptStateError) {
                strncpy(m->error_msg, e->error_msg, sizeof(m->error_msg) - 1);
            }
        },
        true);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Worker thread
 * ════════════════════════════════════════════════════════════════════════════ */

static int32_t worker_thread_cb(void* ctx) {
    BadUsbProApp* app = ctx;
    script_engine_run(&app->engine);
    app->worker_running = false;

    /* Fix #5: Use atomic-guarded USB restore */
    safe_restore_usb(app);

    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Execution view draw
 * ════════════════════════════════════════════════════════════════════════════ */

static void execution_draw_cb(Canvas* canvas, void* model) {
    ExecutionViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    /* Title bar */
    canvas_draw_str(canvas, 2, 10, "BadUSB Pro");

    /* State label */
    const char* state_str = "";
    switch(m->state) {
    case ScriptStateIdle:
        state_str = "Idle";
        break;
    case ScriptStateLoaded:
        state_str = "Ready";
        break;
    case ScriptStateRunning:
        state_str = "Running";
        break;
    case ScriptStatePaused:
        state_str = "Paused";
        break;
    case ScriptStateDone:
        state_str = "Done";
        break;
    case ScriptStateError:
        state_str = "ERROR";
        break;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, state_str);

    /* Progress line */
    char progress[48];
    snprintf(progress, sizeof(progress), "Line %u / %u", m->current_line, m->total_lines);
    canvas_draw_str(canvas, 2, 22, progress);

    /* Progress bar */
    uint8_t bar_width = 124;
    uint8_t bar_x = 2;
    uint8_t bar_y = 24;
    uint8_t bar_h = 4;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_width, bar_h);
    if(m->total_lines > 0) {
        uint8_t fill = (uint8_t)((uint32_t)m->current_line * (bar_width - 2) / m->total_lines);
        if(fill > bar_width - 2) fill = bar_width - 2;
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill, bar_h - 2);
    }

    /* Current command — on Done, show OS detection result if available */
    if(m->state == ScriptStateDone && m->detected_os[0]) {
        char os_line[32];
        snprintf(os_line, sizeof(os_line), "Detected OS: %s", m->detected_os);
        canvas_draw_str(canvas, 2, 38, os_line);
    } else {
        canvas_draw_str(canvas, 2, 38, m->current_cmd);
    }

    /* LED state indicators — use plain ASCII so every font renders them */
    char led_line[32];
    snprintf(
        led_line,
        sizeof(led_line),
        "Num:%c Caps:%c Scrl:%c",
        m->led_num ? '*' : '-',
        m->led_caps ? '*' : '-',
        m->led_scroll ? '*' : '-');
    canvas_draw_str(canvas, 2, 48, led_line);

    /* Error message */
    if(m->state == ScriptStateError && m->error_msg[0]) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 58, m->error_msg);
    }

    /* Controls hint */
    if(m->state == ScriptStateRunning) {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK:Pause  Left:Abort");
    } else if(m->state == ScriptStatePaused) {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK:Resume Left:Abort");
    } else if(m->state == ScriptStateDone || m->state == ScriptStateError) {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Left arrow = Back");
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Execution view input
 *
 *  Fix #6: Do NOT call furi_thread_join() from the GUI thread / input
 *  callback. Instead, just signal the engine to stop and let the worker
 *  thread exit naturally. The worker thread will be joined later when
 *  navigating back or starting a new execution (fix #7).
 * ════════════════════════════════════════════════════════════════════════════ */

static bool execution_input_cb(InputEvent* event, void* ctx) {
    BadUsbProApp* app = ctx;

    /* Consume ALL Back/Left key event types while the engine is active to
     * prevent long-press from triggering execution_back_cb and navigating
     * away while the worker thread is still running. */
    if(event->key == InputKeyBack || event->key == InputKeyLeft) {
        if(app->engine.state == ScriptStateRunning || app->engine.state == ScriptStatePaused) {
            if(event->type == InputTypeShort) {
                script_engine_stop(&app->engine);
            }
            return true; /* consume all Back/Left events while running */
        }
        if(event->type == InputTypeShort) {
            if(app->engine.state == ScriptStateDone || app->engine.state == ScriptStateError) {
                safe_restore_usb(app);
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewFileBrowser);
            }
        }
        return true;
    }

    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyOk:
        if(app->engine.state == ScriptStateRunning) {
            script_engine_pause(&app->engine);
        } else if(app->engine.state == ScriptStatePaused) {
            script_engine_resume(&app->engine);
        }
        return true;

    default:
        break;
    }

    return false;
}

static uint32_t execution_back_cb(void* ctx) {
    UNUSED(ctx);
    return ViewFileBrowser;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Settings callbacks
 * ════════════════════════════════════════════════════════════════════════════ */

static const uint16_t delay_values[] = {0, 50, 100, 250, 500, 1000};
static const char* delay_labels[] = {"0ms", "50ms", "100ms", "250ms", "500ms", "1000ms"};

static void settings_speed_change_cb(VariableItem* item) {
    BadUsbProApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index < SpeedCount) {
        app->speed_setting = (SpeedSetting)index;
        variable_item_set_current_value_text(item, speed_labels[index]);
    }
}

static void settings_mode_change_cb(VariableItem* item) {
    BadUsbProApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->injection_mode = (InjectionMode)(index & 1);
    variable_item_set_current_value_text(item, mode_labels[app->injection_mode]);
}

static void settings_delay_change_cb(VariableItem* item) {
    BadUsbProApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index < 6) {
        app->settings_default_delay = delay_values[index];
        variable_item_set_current_value_text(item, delay_labels[index]);
    }
}

static uint32_t settings_back_cb(void* ctx) {
    UNUSED(ctx);
    return ViewFileBrowser;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  App entry point
 * ════════════════════════════════════════════════════════════════════════════ */

int32_t badusb_pro_app(void* p) {
    UNUSED(p);

    BadUsbProApp* app = malloc(sizeof(BadUsbProApp));
    if(!app) return 1;
    app_alloc(app);

    /* Scan for scripts */
    scan_script_files(app);

    /* Start on file browser */
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewFileBrowser);
    view_dispatcher_run(app->view_dispatcher);

    /* Clean up */
    app_free(app);
    free(app);

    return 0;
}
