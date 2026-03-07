/**
 * flipperpwn.c — Main application, UI, and entry point for FlipperPwn.
 *
 * View hierarchy
 * ~~~~~~~~~~~~~~
 *   FPwnViewMainMenu     Submenu           — root menu
 *   FPwnViewCategoryMenu Submenu           — payload category picker
 *   FPwnViewModuleList   Submenu           — filtered module list
 *   FPwnViewModuleInfo   Widget            — module detail + Run/Options/Back buttons
 *   FPwnViewOptions      VariableItemList  — per-module option list
 *   FPwnViewOptionEdit   TextInput         — inline value editor for a single option
 *   FPwnViewExecute      View              — live execution status
 *
 * Navigation is tracked via a file-static g_current_view so the navigation
 * callback can implement a proper back-stack without adding fields to the
 * frozen FPwnApp struct.
 */

#include "flipperpwn.h"
#include <string.h>
#include <stdio.h>

#define TAG "FPwn"

/* --------------------------------------------------------------------------
 * Custom events (sent via view_dispatcher_send_custom_event)
 * -------------------------------------------------------------------------- */
typedef enum {
    FPwnCustomEventRunModule = 0, /* "Run" button pressed on module info widget */
    FPwnCustomEventExecDone = 1, /* execution thread signalled completion       */
    FPwnCustomEventWifiConnected = 2, /* ESP32 first UART line received          */
} FPwnCustomEvent;
/* NOTE: FPWN_CUSTOM_EVENT_WIFI_CONNECTED in flipperpwn.h must equal 2 */

/* View-stack tracker shared with wifi_views.c via fpwn_set_current_view().
 * Reset to the main menu at each app start. */
FPwnView g_current_view = FPwnViewMainMenu;

void fpwn_set_current_view(FPwnView view) {
    g_current_view = view;
}

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static bool fpwn_navigation_callback(void* ctx);
static bool fpwn_custom_event_callback(void* ctx, uint32_t event);

static void fpwn_main_menu_callback(void* ctx, uint32_t index);
static void fpwn_category_menu_callback(void* ctx, uint32_t index);
static void fpwn_module_list_callback(void* ctx, uint32_t index);
static void fpwn_widget_run_callback(GuiButtonType btn, InputType type, void* ctx);
static void fpwn_widget_back_callback(GuiButtonType btn, InputType type, void* ctx);
static void fpwn_execute_draw_callback(Canvas* canvas, void* model);
static bool fpwn_execute_input_callback(InputEvent* event, void* ctx);
static void fpwn_rebuild_main_menu(FPwnApp* app);
static void fpwn_populate_module_list(FPwnApp* app);
static void fpwn_populate_module_info(FPwnApp* app);
static void fpwn_widget_options_callback(GuiButtonType btn, InputType type, void* ctx);
static void fpwn_options_enter_callback(void* ctx, uint32_t index);
static void fpwn_option_edit_done_callback(void* ctx);
static void fpwn_populate_options_list(FPwnApp* app);

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/* Returns the OS to use for payload execution.
 * Manual override wins; falls back to the detected result.
 * Called from payload_engine.c via the app pointer — not directly from this TU. */
FPwnOS fpwn_effective_os(const FPwnApp* app) {
    return (app->manual_os != FPwnOSUnknown) ? app->manual_os : app->detected_os;
}

/* Label for the "Detect OS" main-menu item — shows last result. */
static const char* fpwn_detect_os_label(FPwnOS os) {
    switch(os) {
    case FPwnOSWindows:
        return "Detected: Windows";
    case FPwnOSMac:
        return "Detected: macOS";
    case FPwnOSLinux:
        return "Detected: Linux";
    default:
        return "Detect OS";
    }
}

/* Cycling label for the "Set OS" main-menu item. */
static const char* fpwn_manual_os_label(FPwnOS os) {
    switch(os) {
    case FPwnOSWindows:
        return "Set OS: Windows";
    case FPwnOSMac:
        return "Set OS: macOS";
    case FPwnOSLinux:
        return "Set OS: Linux";
    default:
        return "Set OS: Auto";
    }
}

/* --------------------------------------------------------------------------
 * Execute view — draw callback
 *
 * model points to an FPwnExecModel allocated inside the View.
 * -------------------------------------------------------------------------- */
static void fpwn_execute_draw_callback(Canvas* canvas, void* model) {
    const FPwnExecModel* m = (const FPwnExecModel*)model;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "FlipperPwn — Executing");

    canvas_set_font(canvas, FontSecondary);

    if(m->finished) {
        canvas_draw_str(canvas, 2, 26, m->error ? "Status: ERROR" : "Status: Done!");
        canvas_draw_str(canvas, 2, 38, m->status);
        canvas_draw_str(canvas, 2, 56, "Press Back to return");
    } else {
        char prog[40];
        snprintf(
            prog,
            sizeof(prog),
            "Line %lu / %lu",
            (unsigned long)m->lines_done,
            (unsigned long)m->lines_total);
        canvas_draw_str(canvas, 2, 26, prog);
        canvas_draw_str(canvas, 2, 38, m->status);
        canvas_draw_str(canvas, 2, 56, "Back = abort");
    }
}

/* Execute view — input callback.
 *
 * Consumes the Back key to set abort_requested instead of triggering
 * navigation.  After the thread finishes the user presses Back again;
 * this time m->finished is true so we let the navigation_callback handle it
 * by NOT consuming the event (return false).
 */
static bool fpwn_execute_input_callback(InputEvent* event, void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    if(event->type != InputTypeShort || event->key != InputKeyBack) {
        return false;
    }

    /* Check if execution has already finished (model read under lock). */
    bool finished = false;
    with_view_model(app->execute_view, FPwnExecModel * m, { finished = m->finished; }, false);

    if(finished) {
        /* Let the navigation_callback pop back to the module list. */
        return false;
    }

    /* Execution in progress — request abort and consume the key press. */
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->abort_requested = true;
    furi_mutex_release(app->mutex);
    return true;
}

/* --------------------------------------------------------------------------
 * Navigation callback
 *
 * Called by view_dispatcher when a view does NOT consume a Back key press.
 * g_current_view tracks which view is active so we can pop the right level.
 * Return false only from the main menu to trigger app exit.
 * -------------------------------------------------------------------------- */
static bool fpwn_navigation_callback(void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    switch(g_current_view) {
    case FPwnViewExecute:
        /* Reached here only when m->finished is true (input callback passes
         * through the Back press).  Return to module list. */
        g_current_view = FPwnViewModuleList;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewModuleList);
        return true;

    case FPwnViewModuleInfo:
        g_current_view = FPwnViewModuleList;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewModuleList);
        return true;

    case FPwnViewOptionEdit:
        g_current_view = FPwnViewOptions;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewOptions);
        return true;

    case FPwnViewOptions:
        g_current_view = FPwnViewModuleInfo;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewModuleInfo);
        return true;

    case FPwnViewModuleList:
        g_current_view = FPwnViewCategoryMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewCategoryMenu);
        return true;

    case FPwnViewCategoryMenu:
        g_current_view = FPwnViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewMainMenu);
        return true;

    case FPwnViewWifiMenu:
        g_current_view = FPwnViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewMainMenu);
        return true;

    case FPwnViewWifiScan:
        /* Stop any active scan before leaving */
        if(fpwn_marauder_get_state(app->marauder) == FPwnMarauderStateScanning) {
            fpwn_marauder_stop_scan(app->marauder);
        }
        g_current_view = FPwnViewWifiMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiMenu);
        return true;

    case FPwnViewWifiPassword:
        g_current_view = FPwnViewWifiScan;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiScan);
        return true;

    case FPwnViewPingScan:
        g_current_view = FPwnViewWifiMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiMenu);
        return true;

    case FPwnViewPortScan:
        g_current_view = FPwnViewPingScan;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewPingScan);
        return true;

    case FPwnViewWifiStatus:
        /* Stop any active operation when dismissing the status log */
        if(fpwn_marauder_get_state(app->marauder) != FPwnMarauderStateIdle) {
            fpwn_marauder_stop(app->marauder);
        }
        g_current_view = FPwnViewWifiMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiMenu);
        return true;

    case FPwnViewMainMenu:
    default:
        /* Back from the root menu exits the application. */
        view_dispatcher_stop(app->view_dispatcher);
        return false;
    }
}

/* --------------------------------------------------------------------------
 * Custom event callback
 * -------------------------------------------------------------------------- */
static bool fpwn_custom_event_callback(void* ctx, uint32_t event) {
    FPwnApp* app = (FPwnApp*)ctx;

    switch((FPwnCustomEvent)event) {
    case FPwnCustomEventRunModule: {
        uint32_t idx = (uint32_t)app->selected_module_index;
        if(idx >= app->module_count) return false;

        /* Lazily load full module details (options). */
        if(!app->modules[idx].options_loaded) {
            fpwn_module_load_full(app, idx);
        }

        /* Reset abort flag before starting. */
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->abort_requested = false;
        furi_mutex_release(app->mutex);

        /* Seed the execute-view model. */
        with_view_model(
            app->execute_view,
            FPwnExecModel * m,
            {
                memset(m, 0, sizeof(FPwnExecModel));
                strncpy(m->status, "Starting...", sizeof(m->status) - 1);
            },
            true);

        /* Switch to the execute view before starting the thread so the
         * display is live from the very first keystroke. */
        g_current_view = FPwnViewExecute;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewExecute);

        /* Guard against a stale thread (should not happen in normal flow). */
        if(app->exec_thread) {
            furi_thread_join(app->exec_thread);
            furi_thread_free(app->exec_thread);
            app->exec_thread = NULL;
        }

        app->exec_thread =
            furi_thread_alloc_ex("FPwnExec", 4096, fpwn_payload_execute_thread, app);
        furi_thread_start(app->exec_thread);
        return true;
    }

    case FPwnCustomEventExecDone: {
        /* Join the finished thread, then force a view redraw to show Done. */
        if(app->exec_thread) {
            furi_thread_join(app->exec_thread);
            furi_thread_free(app->exec_thread);
            app->exec_thread = NULL;
        }
        /* Touch the model with update=true to trigger a canvas redraw. */
        with_view_model(app->execute_view, FPwnExecModel * m, { (void)m; }, true);
        return true;
    }

    case FPwnCustomEventWifiConnected:
        /* ESP32 responded on UART — update the main menu label. */
        fpwn_rebuild_main_menu(app);
        return true;
    }

    return false;
}

/* --------------------------------------------------------------------------
 * Main menu
 * -------------------------------------------------------------------------- */
typedef enum {
    FPwnMainMenuBrowse = 0,
    FPwnMainMenuDetectOS = 1,
    FPwnMainMenuSetOS = 2,
    FPwnMainMenuWifi = 3,
} FPwnMainMenuItem;

static void fpwn_rebuild_main_menu(FPwnApp* app) {
    submenu_reset(app->main_menu);
    submenu_set_header(app->main_menu, "FlipperPwn");
    submenu_add_item(
        app->main_menu, "Browse Modules", FPwnMainMenuBrowse, fpwn_main_menu_callback, app);
    submenu_add_item(
        app->main_menu,
        fpwn_detect_os_label(app->detected_os),
        FPwnMainMenuDetectOS,
        fpwn_main_menu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        fpwn_manual_os_label(app->manual_os),
        FPwnMainMenuSetOS,
        fpwn_main_menu_callback,
        app);

    /* Show connection state in the label so the user knows if ESP32 is present */
    const char* wifi_label = (app->wifi_uart && fpwn_wifi_uart_is_connected(app->wifi_uart)) ?
                                 "WiFi Tools" :
                                 "WiFi Tools (No ESP32)";
    submenu_add_item(app->main_menu, wifi_label, FPwnMainMenuWifi, fpwn_main_menu_callback, app);
}

static void fpwn_main_menu_callback(void* ctx, uint32_t index) {
    FPwnApp* app = (FPwnApp*)ctx;

    switch((FPwnMainMenuItem)index) {
    case FPwnMainMenuBrowse:
        g_current_view = FPwnViewCategoryMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewCategoryMenu);
        break;

    case FPwnMainMenuDetectOS:
        /* Detection is fast (< 400 ms) so blocking the GUI thread is fine. */
        app->detected_os = fpwn_os_detect();
        FURI_LOG_I(TAG, "Detected OS: %s", fpwn_os_name(app->detected_os));
        notification_message(app->notifications, &sequence_success);
        /* Rebuild the menu so the "Detect OS" item shows the result label. */
        fpwn_rebuild_main_menu(app);
        /* Keep cursor on the Detect OS item so the user sees the result. */
        submenu_set_selected_item(app->main_menu, FPwnMainMenuDetectOS);
        break;

    case FPwnMainMenuSetOS:
        /* Cycle: Auto → Windows → macOS → Linux → Auto */
        switch(app->manual_os) {
        case FPwnOSUnknown:
            app->manual_os = FPwnOSWindows;
            break;
        case FPwnOSWindows:
            app->manual_os = FPwnOSMac;
            break;
        case FPwnOSMac:
            app->manual_os = FPwnOSLinux;
            break;
        default:
            app->manual_os = FPwnOSUnknown;
            break;
        }
        /* Rebuild so the item label reflects the new selection. */
        fpwn_rebuild_main_menu(app);
        break;

    case FPwnMainMenuWifi:
        fpwn_wifi_menu_setup(app);
        g_current_view = FPwnViewWifiMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiMenu);
        break;
    }
}

/* --------------------------------------------------------------------------
 * Category menu
 * -------------------------------------------------------------------------- */
static void fpwn_setup_category_menu(FPwnApp* app) {
    submenu_reset(app->category_menu);
    submenu_set_header(app->category_menu, "Category");
    submenu_add_item(
        app->category_menu, "Recon", FPwnCategoryRecon, fpwn_category_menu_callback, app);
    submenu_add_item(
        app->category_menu,
        "Credentials",
        FPwnCategoryCredential,
        fpwn_category_menu_callback,
        app);
    submenu_add_item(
        app->category_menu, "Exploit", FPwnCategoryExploit, fpwn_category_menu_callback, app);
    submenu_add_item(
        app->category_menu, "Post-Exploit", FPwnCategoryPost, fpwn_category_menu_callback, app);
}

static void fpwn_category_menu_callback(void* ctx, uint32_t index) {
    FPwnApp* app = (FPwnApp*)ctx;
    app->current_category = (FPwnCategory)index;
    fpwn_populate_module_list(app);
    g_current_view = FPwnViewModuleList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewModuleList);
}

/* --------------------------------------------------------------------------
 * Module list
 * -------------------------------------------------------------------------- */
static void fpwn_populate_module_list(FPwnApp* app) {
    submenu_reset(app->module_list);

    static const char* const cat_names[FPwnCategoryCount] = {
        [FPwnCategoryRecon] = "Recon",
        [FPwnCategoryCredential] = "Credentials",
        [FPwnCategoryExploit] = "Exploit",
        [FPwnCategoryPost] = "Post-Exploit",
    };
    const char* header = ((uint32_t)app->current_category < FPwnCategoryCount) ?
                             cat_names[app->current_category] :
                             "Modules";
    submenu_set_header(app->module_list, header);

    uint32_t shown = 0;
    for(uint32_t i = 0; i < app->module_count; i++) {
        if(app->modules[i].category != app->current_category) continue;
        /* Use the raw catalog index as item ID — allows O(1) lookup later. */
        submenu_add_item(
            app->module_list, app->modules[i].name, i, fpwn_module_list_callback, app);
        shown++;
    }

    if(shown == 0) {
        /* Placeholder — NULL callback and sentinel ID prevent crashes. */
        submenu_add_item(app->module_list, "(no modules found)", UINT32_MAX, NULL, NULL);
    }
}

static void fpwn_module_list_callback(void* ctx, uint32_t index) {
    FPwnApp* app = (FPwnApp*)ctx;
    if(index >= app->module_count) return;

    app->selected_module_index = (int32_t)index;

    if(!app->modules[index].options_loaded) {
        fpwn_module_load_full(app, index);
    }

    fpwn_populate_module_info(app);
    g_current_view = FPwnViewModuleInfo;
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewModuleInfo);
}

/* --------------------------------------------------------------------------
 * Module info widget
 * -------------------------------------------------------------------------- */

/* Build a slash-separated platform string into buf (must be >= 24 bytes). */
static void fpwn_format_platforms(uint8_t platforms, char* buf, size_t len) {
    if(!platforms) {
        snprintf(buf, len, "ALL");
        return;
    }
    size_t off = 0;
    if(platforms & FPwnPlatformWindows) {
        off += snprintf(buf + off, len - off, "WIN");
    }
    if(platforms & FPwnPlatformMac) {
        off += snprintf(buf + off, len - off, "%sMAC", off ? "/" : "");
    }
    if(platforms & FPwnPlatformLinux) {
        snprintf(buf + off, len - off, "%sLNX", off ? "/" : "");
    }
}

static void fpwn_populate_module_info(FPwnApp* app) {
    widget_reset(app->module_info);

    if(app->selected_module_index < 0 ||
       (uint32_t)app->selected_module_index >= app->module_count) {
        widget_add_string_element(
            app->module_info,
            64,
            32,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "No module selected");
        return;
    }

    const FPwnModule* mod = &app->modules[app->selected_module_index];

    /* Line 0 — module name (bold) */
    widget_add_string_element(app->module_info, 0, 0, AlignLeft, AlignTop, FontPrimary, mod->name);

    /* Line 1 — platform bitmask */
    char plat[24];
    fpwn_format_platforms(mod->platforms, plat, sizeof(plat));
    char plat_line[48];
    snprintf(plat_line, sizeof(plat_line), "Platforms: %s", plat);
    widget_add_string_element(
        app->module_info, 0, 14, AlignLeft, AlignTop, FontSecondary, plat_line);

    /* Lines 2-4 — scrollable description (y=24, height=28) */
    widget_add_text_scroll_element(app->module_info, 0, 24, 128, 28, mod->description);

    /* Line 5 — option count hint */
    char opt_hint[24];
    snprintf(opt_hint, sizeof(opt_hint), "Opts: %u", (unsigned)mod->option_count);
    widget_add_string_element(
        app->module_info, 0, 54, AlignLeft, AlignTop, FontSecondary, opt_hint);

    /* Buttons */
    widget_add_button_element(
        app->module_info, GuiButtonTypeLeft, "Back", fpwn_widget_back_callback, app);
    if(mod->option_count > 0) {
        widget_add_button_element(
            app->module_info, GuiButtonTypeCenter, "Options", fpwn_widget_options_callback, app);
    }
    widget_add_button_element(
        app->module_info, GuiButtonTypeRight, "Run", fpwn_widget_run_callback, app);
}

/* Widget button callbacks fire on both press and release; guard with type. */
static void fpwn_widget_run_callback(GuiButtonType btn, InputType type, void* ctx) {
    UNUSED(btn);
    if(type != InputTypeShort) return;
    FPwnApp* app = (FPwnApp*)ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, FPwnCustomEventRunModule);
}

static void fpwn_widget_back_callback(GuiButtonType btn, InputType type, void* ctx) {
    UNUSED(btn);
    if(type != InputTypeShort) return;
    FPwnApp* app = (FPwnApp*)ctx;
    g_current_view = FPwnViewModuleList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewModuleList);
}

static void fpwn_widget_options_callback(GuiButtonType btn, InputType type, void* ctx) {
    UNUSED(btn);
    if(type != InputTypeShort) return;
    FPwnApp* app = (FPwnApp*)ctx;
    fpwn_populate_options_list(app);
    g_current_view = FPwnViewOptions;
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewOptions);
}

/* --------------------------------------------------------------------------
 * Options list
 * -------------------------------------------------------------------------- */
static void fpwn_populate_options_list(FPwnApp* app) {
    variable_item_list_reset(app->options_list);

    if(app->selected_module_index < 0 ||
       (uint32_t)app->selected_module_index >= app->module_count) {
        return;
    }

    FPwnModule* mod = &app->modules[app->selected_module_index];

    for(uint8_t i = 0; i < mod->option_count; i++) {
        VariableItem* item = variable_item_list_add(
            app->options_list,
            mod->options[i].name,
            0, /* no cycling values */
            NULL,
            app);
        variable_item_set_current_value_text(item, mod->options[i].value);
    }

    variable_item_list_set_enter_callback(app->options_list, fpwn_options_enter_callback, app);
}

static void fpwn_options_enter_callback(void* ctx, uint32_t index) {
    FPwnApp* app = (FPwnApp*)ctx;

    if(app->selected_module_index < 0) return;
    FPwnModule* mod = &app->modules[app->selected_module_index];
    if(index >= mod->option_count) return;

    app->editing_option_index = (uint8_t)index;

    /* Copy current value into edit buffer */
    strncpy(app->option_edit_buf, mod->options[index].value, FPWN_OPT_VALUE_LEN - 1);
    app->option_edit_buf[FPWN_OPT_VALUE_LEN - 1] = '\0';

    text_input_reset(app->option_edit_input);
    text_input_set_header_text(app->option_edit_input, mod->options[index].name);
    text_input_set_result_callback(
        app->option_edit_input,
        fpwn_option_edit_done_callback,
        app,
        app->option_edit_buf,
        FPWN_OPT_VALUE_LEN,
        false);

    g_current_view = FPwnViewOptionEdit;
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewOptionEdit);
}

static void fpwn_option_edit_done_callback(void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    if(app->selected_module_index < 0) return;
    FPwnModule* mod = &app->modules[app->selected_module_index];

    if(app->editing_option_index < mod->option_count) {
        strncpy(
            mod->options[app->editing_option_index].value,
            app->option_edit_buf,
            FPWN_OPT_VALUE_LEN - 1);
        mod->options[app->editing_option_index].value[FPWN_OPT_VALUE_LEN - 1] = '\0';
    }

    /* Refresh the options list to show the new value */
    fpwn_populate_options_list(app);

    g_current_view = FPwnViewOptions;
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewOptions);
}

/* --------------------------------------------------------------------------
 * App alloc
 * -------------------------------------------------------------------------- */
static FPwnApp* flipperpwn_app_alloc(void) {
    FPwnApp* app = malloc(sizeof(FPwnApp));
    furi_assert(app);
    memset(app, 0, sizeof(FPwnApp));

    app->selected_module_index = -1;
    /* detected_os and manual_os default to FPwnOSUnknown (0) via memset. */

    /* ---- Service records ---- */
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* ---- Mutex for cross-thread abort flag ---- */
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(app->mutex);

    /* ---- Ensure SD card directories exist ---- */
    storage_simply_mkdir(app->storage, EXT_PATH("flipperpwn"));
    storage_simply_mkdir(app->storage, FPWN_MODULES_DIR);

    /* ---- Write sample modules on first launch (no-op if any exist) ---- */
    fpwn_modules_write_samples(app);

    /* ---- Scan for .fpwn files (metadata only) ---- */
    fpwn_modules_scan(app);

    /* ---- View dispatcher ---- */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, fpwn_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, fpwn_custom_event_callback);

    /* ---- Main menu ---- */
    app->main_menu = submenu_alloc();
    fpwn_rebuild_main_menu(app);
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewMainMenu, submenu_get_view(app->main_menu));

    /* ---- Category menu ---- */
    app->category_menu = submenu_alloc();
    fpwn_setup_category_menu(app);
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewCategoryMenu, submenu_get_view(app->category_menu));

    /* ---- Module list ---- */
    app->module_list = submenu_alloc();
    submenu_set_header(app->module_list, "Modules");
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewModuleList, submenu_get_view(app->module_list));

    /* ---- Module info widget ---- */
    app->module_info = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewModuleInfo, widget_get_view(app->module_info));

    /* ---- Options list ---- */
    app->options_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewOptions, variable_item_list_get_view(app->options_list));

    /* ---- Option edit text input ---- */
    app->option_edit_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewOptionEdit, text_input_get_view(app->option_edit_input));

    /* ---- Execute view ---- */
    app->execute_view = view_alloc();
    view_set_context(app->execute_view, app);
    view_set_draw_callback(app->execute_view, fpwn_execute_draw_callback);
    view_set_input_callback(app->execute_view, fpwn_execute_input_callback);
    view_allocate_model(app->execute_view, ViewModelTypeLocking, sizeof(FPwnExecModel));
    with_view_model(
        app->execute_view, FPwnExecModel * m, { memset(m, 0, sizeof(FPwnExecModel)); }, false);
    view_dispatcher_add_view(app->view_dispatcher, FPwnViewExecute, app->execute_view);

    /* ---- WiFi Dev Board views ---- */
    fpwn_wifi_views_alloc(app);

    /* Rebuild main menu now that wifi_uart is initialised so the label
     * accurately reflects whether an ESP32 is present. */
    fpwn_rebuild_main_menu(app);

    return app;
}

/* --------------------------------------------------------------------------
 * App free
 * -------------------------------------------------------------------------- */
static void flipperpwn_app_free(FPwnApp* app) {
    furi_assert(app);

    /* Abort any running execution thread gracefully. */
    if(app->exec_thread) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->abort_requested = true;
        furi_mutex_release(app->mutex);
        furi_thread_join(app->exec_thread);
        furi_thread_free(app->exec_thread);
        app->exec_thread = NULL;
    }

    /* Remove WiFi views first (they hold UART + marauder resources). */
    fpwn_wifi_views_free(app);

    /* Remove views from dispatcher before freeing underlying objects.
     * Order does not matter for correctness, but mirror alloc order. */
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewCategoryMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewModuleList);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewModuleInfo);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewOptions);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewOptionEdit);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewExecute);

    view_dispatcher_free(app->view_dispatcher);

    submenu_free(app->main_menu);
    submenu_free(app->category_menu);
    submenu_free(app->module_list);
    widget_free(app->module_info);
    variable_item_list_free(app->options_list);
    text_input_free(app->option_edit_input);
    view_free(app->execute_view);

    furi_mutex_free(app->mutex);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */
int32_t flipperpwn_app(void* p) {
    UNUSED(p);

    /* Save current USB config so we can restore it on exit. */
    FuriHalUsbInterface* prev_usb = furi_hal_usb_get_config();

    /* Switch to USB HID keyboard mode and wait for the host to enumerate. */
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_hid, NULL);
    {
        uint32_t t = furi_get_tick();
        while(!furi_hal_hid_is_connected() && (furi_get_tick() - t) < furi_ms_to_ticks(5000)) {
            furi_delay_ms(50);
        }
        if(furi_hal_hid_is_connected()) furi_delay_ms(1500);
    }

    /* Reset the view-stack tracker for a clean session. */
    g_current_view = FPwnViewMainMenu;

    FPwnApp* app = flipperpwn_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewMainMenu);

    /* Blocks until view_dispatcher_stop() is called (user exits root menu). */
    view_dispatcher_run(app->view_dispatcher);

    flipperpwn_app_free(app);

    /* Restore prior USB personality. */
    furi_hal_usb_set_config(prev_usb, NULL);

    return 0;
}
