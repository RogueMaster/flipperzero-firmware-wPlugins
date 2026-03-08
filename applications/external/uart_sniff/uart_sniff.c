/**
 * uart_sniff.c — Main application for UART Sniff FAP.
 *
 * Flow:
 *   Main Menu  →  Start Sniff  →  Sniff View (TextBox, refreshed at 100 ms)
 *              →  Settings     →  VariableItemList
 *              →  Clear        →  clears ring buffer, stays in menu
 *
 * All large buffers are heap-allocated; the FAP stack is only ~4 KB.
 */

#include "uart_sniff.h"

#include <furi_hal.h>

#define TAG "UartSniff"

/* -----------------------------------------------------------------------
 * Baud rate table
 * ----------------------------------------------------------------------- */
static const uint32_t BAUD_VALUES[UartSniffBaudCount] = {
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
};

static const char* const BAUD_LABELS[UartSniffBaudCount] = {
    "9600",
    "19200",
    "38400",
    "57600",
    "115200",
    "230400",
};

static const char* const SHOW_LABELS[UartSniffShowCount] = {
    "Hex",
    "ASCII",
    "Both",
};

static const char* const CHANNEL_LABELS[2] = {
    "USART (13/14)",
    "LPUART",
};

/* -----------------------------------------------------------------------
 * Display formatting
 * ----------------------------------------------------------------------- */

/**
 * Render the last N bytes from the ring into app->display_buf.
 * Format depends on app->show_mode.
 *
 * "Both" mode: "XXXX: HH HH HH HH HH HH HH HH  ABCDEFGH\n"
 * "Hex" mode:  "XXXX: HH HH HH HH HH HH HH HH\n"
 * "ASCII" mode: printable chars + '.' for non-printable, 8 per line.
 */
static void uart_sniff_format_display(UartSniffApp* app) {
    /* Pull last DISPLAY_BYTES from ring */
    size_t got = uart_sniff_worker_read(app->worker, app->capture_buf, UART_SNIFF_DISPLAY_BYTES);

    char* dst = app->display_buf;
    size_t remaining = app->display_buf_size;
    size_t written = 0;

    size_t lines = (got + UART_SNIFF_BYTES_PER_LINE - 1) / UART_SNIFF_BYTES_PER_LINE;

    /* Compute address of the first displayed byte so addresses increment
     * monotonically even as the window slides. */
    uint32_t base_addr = uart_sniff_worker_total(app->worker);
    /* base_addr is total bytes received; the first displayed byte was
     * (total - got) bytes ago. */
    uint32_t first_addr = (base_addr >= (uint32_t)got) ? (base_addr - (uint32_t)got) : 0;

    for(size_t line = 0; line < lines; line++) {
        size_t offset = line * UART_SNIFF_BYTES_PER_LINE;
        size_t count = got - offset;
        if(count > UART_SNIFF_BYTES_PER_LINE) count = UART_SNIFF_BYTES_PER_LINE;

        uint32_t addr = first_addr + (uint32_t)offset;
        int n = 0;

        if(app->show_mode == UartSniffShowBoth) {
            /* Address */
            n = snprintf(dst + written, remaining, "%04lX: ", (unsigned long)addr);
            if(n <= 0 || (size_t)n >= remaining) break;
            written += (size_t)n;
            remaining -= (size_t)n;

            /* Hex bytes */
            for(size_t b = 0; b < UART_SNIFF_BYTES_PER_LINE; b++) {
                if(b < count) {
                    n = snprintf(dst + written, remaining, "%02X ", app->capture_buf[offset + b]);
                } else {
                    n = snprintf(dst + written, remaining, "   ");
                }
                if(n <= 0 || (size_t)n >= remaining) goto done;
                written += (size_t)n;
                remaining -= (size_t)n;
            }

            /* Separator */
            if(remaining < 2) goto done;
            dst[written++] = ' ';
            remaining--;

            /* ASCII sidebar */
            for(size_t b = 0; b < count; b++) {
                uint8_t byte = app->capture_buf[offset + b];
                if(remaining < 2) goto done;
                dst[written++] = (byte >= 0x20 && byte < 0x7F) ? (char)byte : '.';
                remaining--;
            }

        } else if(app->show_mode == UartSniffShowHex) {
            n = snprintf(dst + written, remaining, "%04lX: ", (unsigned long)addr);
            if(n <= 0 || (size_t)n >= remaining) break;
            written += (size_t)n;
            remaining -= (size_t)n;

            for(size_t b = 0; b < count; b++) {
                n = snprintf(dst + written, remaining, "%02X ", app->capture_buf[offset + b]);
                if(n <= 0 || (size_t)n >= remaining) goto done;
                written += (size_t)n;
                remaining -= (size_t)n;
            }

        } else { /* UartSniffShowAscii */
            for(size_t b = 0; b < count; b++) {
                uint8_t byte = app->capture_buf[offset + b];
                if(remaining < 2) goto done;
                dst[written++] = (byte >= 0x20 && byte < 0x7F) ? (char)byte : '.';
                remaining--;
            }
        }

        /* Newline */
        if(remaining < 2) goto done;
        dst[written++] = '\n';
        remaining--;
    }

done:
    dst[written] = '\0';
}

/* -----------------------------------------------------------------------
 * Refresh timer callback — fires every 100 ms on the timer service thread.
 * Must not call GUI APIs directly; posts an event to the GUI thread instead.
 * ----------------------------------------------------------------------- */
static void uart_sniff_refresh_cb(void* context) {
    UartSniffApp* app = (UartSniffApp*)context;
    if(!app->sniffing || !app->worker) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, UartSniffEventRefresh);
}

/* -----------------------------------------------------------------------
 * Custom event callback — runs on the GUI thread (view_dispatcher_run loop).
 * Safe to call TextBox and other GUI APIs here.
 * ----------------------------------------------------------------------- */
static bool uart_sniff_custom_event_cb(void* context, uint32_t event) {
    UartSniffApp* app = (UartSniffApp*)context;
    switch((UartSniffEvent)event) {
    case UartSniffEventRefresh:
        uart_sniff_format_display(app);
        text_box_set_text(app->text_box, app->display_buf);
        text_box_set_focus(app->text_box, TextBoxFocusEnd);
        return true;
    }
    return false;
}

/* -----------------------------------------------------------------------
 * Sniff view — back navigation
 * ----------------------------------------------------------------------- */
static uint32_t sniff_view_back_cb(void* context) {
    UNUSED(context);
    return UartSniffViewMenu;
}

/* -----------------------------------------------------------------------
 * Main menu callbacks
 * ----------------------------------------------------------------------- */
static void menu_cb(void* context, uint32_t index) {
    UartSniffApp* app = (UartSniffApp*)context;

    switch((UartSniffMenuItem)index) {
    case UartSniffMenuStart:
        if(!app->sniffing) {
            uint32_t baud = BAUD_VALUES[app->baud_idx];
            app->worker = uart_sniff_worker_alloc(baud, app->serial_id);
            if(app->worker) {
                app->sniffing = true;
                /* Clear display buffer before showing anything */
                app->display_buf[0] = '\0';
                text_box_set_text(app->text_box, "Waiting for data...");
                text_box_set_focus(app->text_box, TextBoxFocusEnd);
            } else {
                /* Serial port busy — show a status message in text box */
                text_box_set_text(app->text_box, "Serial port busy!\nCheck USB/expansion.");
                text_box_set_focus(app->text_box, TextBoxFocusStart);
            }
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, UartSniffViewSniff);
        break;

    case UartSniffMenuSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, UartSniffViewSettings);
        break;

    case UartSniffMenuClear:
        if(app->worker) {
            uart_sniff_worker_clear(app->worker);
        }
        app->display_buf[0] = '\0';
        text_box_set_text(app->text_box, "Cleared.");
        break;
    }
}

/* -----------------------------------------------------------------------
 * Navigation callback — Back from top-level menu exits the app.
 * ----------------------------------------------------------------------- */
static bool app_navigation_cb(void* context) {
    UartSniffApp* app = (UartSniffApp*)context;

    /* If we are in the sniff view, stop sniffing when navigating back. */
    if(app->sniffing) {
        app->sniffing = false;
        if(app->worker) {
            uart_sniff_worker_free(app->worker);
            app->worker = NULL;
        }
        /* The view_dispatcher will handle the actual view switch via
         * sniff_view_back_cb returning UartSniffViewMenu. */
        return false; /* allow default back — triggers previous_callback */
    }

    /* Back pressed at the main menu: exit app. */
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

/* -----------------------------------------------------------------------
 * Settings callbacks
 * ----------------------------------------------------------------------- */
static void settings_baud_cb(VariableItem* item) {
    UartSniffApp* app = (UartSniffApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= UartSniffBaudCount) idx = 0;
    app->baud_idx = (UartSniffBaud)idx;
    variable_item_set_current_value_text(item, BAUD_LABELS[idx]);
}

static void settings_channel_cb(VariableItem* item) {
    UartSniffApp* app = (UartSniffApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->serial_id = (idx == 0) ? FuriHalSerialIdUsart : FuriHalSerialIdLpuart;
    variable_item_set_current_value_text(item, CHANNEL_LABELS[idx]);
}

static void settings_show_cb(VariableItem* item) {
    UartSniffApp* app = (UartSniffApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= UartSniffShowCount) idx = 0;
    app->show_mode = (UartSniffShow)idx;
    variable_item_set_current_value_text(item, SHOW_LABELS[idx]);
}

static uint32_t settings_back_cb(void* context) {
    UNUSED(context);
    return UartSniffViewMenu;
}

static void settings_list_setup(UartSniffApp* app) {
    VariableItem* item;

    item = variable_item_list_add(
        app->settings_list, "Baud Rate", UartSniffBaudCount, settings_baud_cb, app);
    variable_item_set_current_value_index(item, (uint8_t)app->baud_idx);
    variable_item_set_current_value_text(item, BAUD_LABELS[app->baud_idx]);

    item = variable_item_list_add(app->settings_list, "Channel", 2, settings_channel_cb, app);
    uint8_t ch_idx = (app->serial_id == FuriHalSerialIdUsart) ? 0u : 1u;
    variable_item_set_current_value_index(item, ch_idx);
    variable_item_set_current_value_text(item, CHANNEL_LABELS[ch_idx]);

    item = variable_item_list_add(
        app->settings_list, "Show", UartSniffShowCount, settings_show_cb, app);
    variable_item_set_current_value_index(item, (uint8_t)app->show_mode);
    variable_item_set_current_value_text(item, SHOW_LABELS[app->show_mode]);
}

/* -----------------------------------------------------------------------
 * App alloc / free
 * ----------------------------------------------------------------------- */
static UartSniffApp* uart_sniff_app_alloc(void) {
    UartSniffApp* app = malloc(sizeof(UartSniffApp));
    furi_assert(app);
    memset(app, 0, sizeof(UartSniffApp));

    /* Defaults */
    app->baud_idx = UartSniffBaud115200;
    app->serial_id = FuriHalSerialIdUsart;
    app->show_mode = UartSniffShowBoth;

    /* Heap buffers */
    app->display_buf_size = UART_SNIFF_DISPLAY_BUF_SIZE;
    app->display_buf = malloc(app->display_buf_size);
    furi_assert(app->display_buf);
    app->display_buf[0] = '\0';

    app->capture_buf = malloc(UART_SNIFF_DISPLAY_BYTES);
    furi_assert(app->capture_buf);

    /* Records */
    app->gui = furi_record_open(RECORD_GUI);

    /* ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_navigation_cb);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, uart_sniff_custom_event_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* ---- Main Menu ---- */
    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "UART Sniff");
    submenu_add_item(app->menu, "Start Sniff", UartSniffMenuStart, menu_cb, app);
    submenu_add_item(app->menu, "Settings", UartSniffMenuSettings, menu_cb, app);
    submenu_add_item(app->menu, "Clear", UartSniffMenuClear, menu_cb, app);
    view_dispatcher_add_view(app->view_dispatcher, UartSniffViewMenu, submenu_get_view(app->menu));

    /* ---- Sniff View (TextBox) ---- */
    app->text_box = text_box_alloc();
    text_box_set_font(app->text_box, TextBoxFontHex);
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    text_box_set_text(app->text_box, "");
    view_set_previous_callback(text_box_get_view(app->text_box), sniff_view_back_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, UartSniffViewSniff, text_box_get_view(app->text_box));

    /* ---- Settings (VariableItemList) ---- */
    app->settings_list = variable_item_list_alloc();
    variable_item_list_set_header(app->settings_list, "Settings");
    settings_list_setup(app);
    view_set_previous_callback(variable_item_list_get_view(app->settings_list), settings_back_cb);
    view_dispatcher_add_view(
        app->view_dispatcher,
        UartSniffViewSettings,
        variable_item_list_get_view(app->settings_list));

    return app;
}

static void uart_sniff_app_free(UartSniffApp* app) {
    /* Stop sniffing if active */
    if(app->sniffing && app->worker) {
        app->sniffing = false;
        uart_sniff_worker_free(app->worker);
        app->worker = NULL;
    }

    /* Remove views before freeing them — ViewDispatcher requirement */
    view_dispatcher_remove_view(app->view_dispatcher, UartSniffViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UartSniffViewSniff);
    view_dispatcher_remove_view(app->view_dispatcher, UartSniffViewSettings);

    submenu_free(app->menu);
    text_box_free(app->text_box);
    variable_item_list_free(app->settings_list);

    view_dispatcher_free(app->view_dispatcher);

    free(app->display_buf);
    free(app->capture_buf);

    furi_record_close(RECORD_GUI);

    free(app);
}

/* -----------------------------------------------------------------------
 * Entry point
 * ----------------------------------------------------------------------- */
int32_t uart_sniff_app(void* p) {
    UNUSED(p);

    UartSniffApp* app = uart_sniff_app_alloc();

    /* 100 ms refresh timer — updates the TextBox from ring buffer data */
    app->refresh_timer = furi_timer_alloc(uart_sniff_refresh_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->refresh_timer, furi_ms_to_ticks(100));

    view_dispatcher_switch_to_view(app->view_dispatcher, UartSniffViewMenu);
    view_dispatcher_run(app->view_dispatcher);

    furi_timer_stop(app->refresh_timer);
    furi_timer_free(app->refresh_timer);

    uart_sniff_app_free(app);

    return 0;
}
