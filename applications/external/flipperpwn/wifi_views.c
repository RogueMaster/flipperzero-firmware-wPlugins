/**
 * wifi_views.c — WiFi Dev Board GUI views for FlipperPwn.
 *
 * View roster
 * ~~~~~~~~~~~
 *   FPwnViewWifiMenu     Submenu           — WiFi tool picker
 *   FPwnViewWifiScan     View (custom)     — live AP list with RSSI bars
 *   FPwnViewWifiPassword TextInput         — password entry before join
 *   FPwnViewPingScan     View (custom)     — ping sweep host list
 *   FPwnViewPortScan     View (custom)     — port scan results
 *   FPwnViewWifiStatus   TextBox           — raw Marauder output log
 *
 * Navigation back-transitions are handled by fpwn_navigation_callback in
 * flipperpwn.c; this file only handles forward navigation and view-local
 * input events (scrolling, selection).
 */

#include "flipperpwn.h"
#include "wifi_uart.h"
#include <string.h>
#include <stdio.h>

#define TAG "FPwn"

/* Set once when the first UART line arrives; reset when wifi views are freed. */
static bool s_wifi_first_connect_notified = false;

/* =========================================================================
 * WiFi menu — item indices
 * ========================================================================= */

typedef enum {
    FPwnWifiMenuScanAP = 0,
    FPwnWifiMenuJoinNetwork,
    FPwnWifiMenuPingScan,
    FPwnWifiMenuPortScan,
    FPwnWifiMenuDeauth,
    FPwnWifiMenuSniffPmkid,
    FPwnWifiMenuStatus,
} FPwnWifiMenuItem;

/* =========================================================================
 * AP scan view model
 * ========================================================================= */

typedef struct {
    FPwnWifiAP aps[FPWN_MAX_APS];
    uint32_t ap_count;
    uint8_t scroll_offset;
    uint8_t selected_index;
    bool scanning;
} FPwnWifiScanModel;

/* =========================================================================
 * Ping scan view model
 * ========================================================================= */

typedef struct {
    FPwnNetHost hosts[FPWN_MAX_HOSTS];
    uint32_t host_count;
    uint8_t scroll_offset;
    uint8_t selected_index;
    bool scanning;
} FPwnPingScanModel;

/* =========================================================================
 * Port scan view model
 * ========================================================================= */

typedef struct {
    FPwnPortResult ports[FPWN_MAX_PORTS];
    uint32_t port_count;
    uint8_t scroll_offset;
} FPwnPortScanModel;

/* =========================================================================
 * Drawing helpers
 * ========================================================================= */

/* Draw a compact RSSI signal bar (5 segments, 10x6 px) at (x, y).
 * rssi ranges from roughly -100 (weakest) to -30 (strongest). */
static void fpwn_draw_rssi_bar(Canvas* canvas, int16_t x, int16_t y, int8_t rssi) {
    /* Map rssi to 0-5 filled segments. */
    int strength = 0;
    if(rssi > -40)
        strength = 5;
    else if(rssi > -55)
        strength = 4;
    else if(rssi > -67)
        strength = 3;
    else if(rssi > -75)
        strength = 2;
    else if(rssi > -85)
        strength = 1;

    for(int i = 0; i < 5; i++) {
        int bar_h = i + 2; /* bars grow taller left-to-right */
        int bx = x + i * 2;
        int by = y + (5 - bar_h);
        if(i < strength) {
            canvas_draw_box(canvas, bx, by, 1, bar_h);
        } else {
            /* Outline only for empty bars */
            canvas_draw_frame(canvas, bx, by, 1, bar_h);
        }
    }
}

/* Draw a lock glyph (5x7 px) at (x, y) for encrypted networks. */
static void fpwn_draw_lock(Canvas* canvas, int16_t x, int16_t y) {
    /* shackle arc: top 3 rows */
    canvas_draw_frame(canvas, x + 1, y, 3, 3);
    /* body: bottom 4 rows */
    canvas_draw_box(canvas, x, y + 3, 5, 4);
}

/* =========================================================================
 * AP scan view — draw callback
 *
 * Layout (128 x 64):
 *   Row 0-10:  header "WiFi Scan" + scanning indicator
 *   Row 11:    separator line
 *   Row 12-63: up to 5 AP rows (10 px each), each row shows:
 *              [rssi bar][lock?][ssid truncated][ch+rssi right-aligned]
 * ========================================================================= */
static void fpwn_wifi_scan_draw(Canvas* canvas, void* model_ptr) {
    FPwnWifiScanModel* m = (FPwnWifiScanModel*)model_ptr;

    canvas_clear(canvas);

    /* Header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "WiFi Scan");

    if(m->scanning) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 72, 10, "[scanning]");
    }

    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->ap_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 38, m->scanning ? "Searching..." : "No APs found");
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    /* Visible window: 5 rows of 10 px starting at y=14 */
    const uint8_t visible_rows = 5;
    const uint8_t row_h = 10;
    const uint8_t list_y = 14;

    for(uint8_t row = 0; row < visible_rows; row++) {
        uint8_t idx = m->scroll_offset + row;
        if(idx >= m->ap_count) break;

        int16_t ry = (int16_t)(list_y + row * row_h);
        bool is_selected = (idx == m->selected_index);

        /* Highlight selected row */
        if(is_selected) {
            canvas_draw_box(canvas, 0, ry - 1, 128, row_h);
            canvas_set_color(canvas, ColorWhite);
        }

        /* RSSI bar at x=1 */
        fpwn_draw_rssi_bar(canvas, 1, ry, m->aps[idx].rssi);

        /* Lock icon for encrypted APs at x=13 */
        if(m->aps[idx].encryption > 0) {
            fpwn_draw_lock(canvas, 13, ry);
        }

        /* SSID — truncated to 16 visible chars, starting x=20 */
        char ssid_buf[17];
        strncpy(ssid_buf, m->aps[idx].ssid, sizeof(ssid_buf) - 1);
        ssid_buf[sizeof(ssid_buf) - 1] = '\0';
        canvas_draw_str(canvas, 20, ry + 7, ssid_buf);

        /* Channel + RSSI right-aligned — "ch6 -72" */
        char meta[12];
        snprintf(
            meta, sizeof(meta), "c%u %d", (unsigned)m->aps[idx].channel, (int)m->aps[idx].rssi);
        /* Approximate right-align: each FontSecondary char ~6px wide */
        int16_t meta_x = (int16_t)(128 - (int16_t)(strlen(meta) * 5));
        canvas_draw_str(canvas, meta_x, ry + 7, meta);

        if(is_selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    /* Scroll indicator */
    if(m->ap_count > visible_rows) {
        uint8_t bar_h = (uint8_t)((visible_rows * (64 - list_y)) / m->ap_count);
        uint8_t bar_y = (uint8_t)(list_y + (m->scroll_offset * (64 - list_y)) / m->ap_count);
        canvas_draw_box(canvas, 126, bar_y, 2, bar_h);
    }
}

/* =========================================================================
 * AP scan view — input callback
 * ========================================================================= */
static bool fpwn_wifi_scan_input(InputEvent* event, void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;

    with_view_model(
        app->wifi_scan_view,
        FPwnWifiScanModel * m,
        {
            const uint8_t visible_rows = 5;

            if(event->key == InputKeyUp) {
                if(m->selected_index > 0) {
                    m->selected_index--;
                    /* Scroll up if cursor left the visible window */
                    if(m->selected_index < m->scroll_offset) {
                        m->scroll_offset = m->selected_index;
                    }
                }
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(m->ap_count > 0 && m->selected_index < m->ap_count - 1) {
                    m->selected_index++;
                    /* Scroll down if cursor left the visible window */
                    if(m->selected_index >= m->scroll_offset + visible_rows) {
                        m->scroll_offset = (uint8_t)(m->selected_index - visible_rows + 1);
                    }
                }
                consumed = true;
            } else if(event->key == InputKeyOk) {
                /* Store selected AP and navigate to password entry */
                if(m->ap_count > 0 && m->selected_index < m->ap_count) {
                    app->wifi_selected_ap = m->selected_index;
                    consumed = true;

                    /* Stop scanning before leaving the view */
                    if(m->scanning) {
                        m->scanning = false;
                        fpwn_marauder_stop_scan(app->marauder);
                    }

                    /* Set up password input then navigate */
                    memset(app->wifi_text_buf, 0, sizeof(app->wifi_text_buf));
                    text_input_reset(app->wifi_text_input);
                    text_input_set_header_text(app->wifi_text_input, "Password (empty=open)");
                    /* text_input result callback was set in alloc — buffer is
                     * already bound. Just switch to the password view. */
                    fpwn_set_current_view(FPwnViewWifiPassword);
                    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiPassword);
                }
            }
        },
        consumed);

    return consumed;
}

/* =========================================================================
 * Password entered callback — joins the selected AP
 * ========================================================================= */
static void fpwn_wifi_password_done(void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    /* Send join command — empty password string is fine for open networks */
    fpwn_marauder_join(app->marauder, app->wifi_selected_ap, app->wifi_text_buf);

    /* Navigate to the status log so the user can see the join output */
    fpwn_set_current_view(FPwnViewWifiStatus);
    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiStatus);
}

/* =========================================================================
 * Ping scan view — draw callback
 *
 * Layout (128 x 64):
 *   Row 0-10:  header "Ping Scan" + scanning indicator
 *   Row 11:    separator
 *   Row 12-63: up to 5 host rows (10 px each)
 *              [bullet alive/dead][ip address][right-side status label]
 * ========================================================================= */
static void fpwn_ping_scan_draw(Canvas* canvas, void* model_ptr) {
    FPwnPingScanModel* m = (FPwnPingScanModel*)model_ptr;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Ping Scan");

    if(m->scanning) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 68, 10, "[scanning]");
    }

    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->host_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 38, m->scanning ? "Scanning subnet..." : "No hosts found");
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    const uint8_t visible_rows = 5;
    const uint8_t row_h = 10;
    const uint8_t list_y = 14;

    for(uint8_t row = 0; row < visible_rows; row++) {
        uint8_t idx = m->scroll_offset + row;
        if(idx >= m->host_count) break;

        int16_t ry = (int16_t)(list_y + row * row_h);
        bool is_selected = (idx == m->selected_index);

        if(is_selected) {
            canvas_draw_box(canvas, 0, ry - 1, 128, row_h);
            canvas_set_color(canvas, ColorWhite);
        }

        /* Alive/dead bullet: filled circle for alive, outline for dead */
        if(m->hosts[idx].alive) {
            canvas_draw_box(canvas, 2, ry + 2, 5, 5);
        } else {
            canvas_draw_frame(canvas, 2, ry + 2, 5, 5);
        }

        /* IP address starting at x=10 */
        canvas_draw_str(canvas, 10, ry + 7, m->hosts[idx].ip);

        /* Status label right-aligned */
        const char* status = m->hosts[idx].alive ? "UP" : "down";
        int16_t sx = (int16_t)(128 - (int16_t)(strlen(status) * 5));
        canvas_draw_str(canvas, sx, ry + 7, status);

        if(is_selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    /* Scroll indicator */
    if(m->host_count > visible_rows) {
        uint8_t bar_h = (uint8_t)((visible_rows * (64 - list_y)) / m->host_count);
        uint8_t bar_y = (uint8_t)(list_y + (m->scroll_offset * (64 - list_y)) / m->host_count);
        canvas_draw_box(canvas, 126, bar_y, 2, bar_h);
    }
}

/* =========================================================================
 * Ping scan view — input callback
 * ========================================================================= */
static bool fpwn_ping_scan_input(InputEvent* event, void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;

    with_view_model(
        app->ping_scan_view,
        FPwnPingScanModel * m,
        {
            const uint8_t visible_rows = 5;

            if(event->key == InputKeyUp) {
                if(m->selected_index > 0) {
                    m->selected_index--;
                    if(m->selected_index < m->scroll_offset) {
                        m->scroll_offset = m->selected_index;
                    }
                }
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(m->host_count > 0 && m->selected_index < m->host_count - 1) {
                    m->selected_index++;
                    if(m->selected_index >= m->scroll_offset + visible_rows) {
                        m->scroll_offset = (uint8_t)(m->selected_index - visible_rows + 1);
                    }
                }
                consumed = true;
            } else if(event->key == InputKeyOk) {
                /* Store selected host index and kick off a port scan */
                if(m->host_count > 0 && m->selected_index < m->host_count) {
                    app->wifi_selected_host = m->selected_index;
                    /* Default scan (common ports only) */
                    fpwn_marauder_port_scan(app->marauder, app->wifi_selected_host, false);
                    fpwn_set_current_view(FPwnViewPortScan);
                    view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewPortScan);
                    consumed = true;
                }
            }
        },
        consumed);

    return consumed;
}

/* =========================================================================
 * Port scan view — draw callback
 *
 * Layout (128 x 64):
 *   Row 0-10:  header "Port Scan"
 *   Row 11:    separator
 *   Row 12-63: up to 5 port rows (10 px each)
 *              [bullet open/closed][port number][service name right-aligned]
 * ========================================================================= */
static void fpwn_port_scan_draw(Canvas* canvas, void* model_ptr) {
    FPwnPortScanModel* m = (FPwnPortScanModel*)model_ptr;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Port Scan");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->port_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 38, "No open ports");
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    const uint8_t visible_rows = 5;
    const uint8_t row_h = 10;
    const uint8_t list_y = 14;

    for(uint8_t row = 0; row < visible_rows; row++) {
        uint8_t idx = m->scroll_offset + row;
        if(idx >= m->port_count) break;

        int16_t ry = (int16_t)(list_y + row * row_h);

        /* Open/closed bullet */
        if(m->ports[idx].open) {
            canvas_draw_box(canvas, 2, ry + 2, 5, 5);
        } else {
            canvas_draw_frame(canvas, 2, ry + 2, 5, 5);
        }

        /* Port number */
        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%u", (unsigned)m->ports[idx].port);
        canvas_draw_str(canvas, 10, ry + 7, port_str);

        /* Service name right-aligned */
        if(m->ports[idx].service[0] != '\0') {
            int16_t sx = (int16_t)(128 - (int16_t)(strlen(m->ports[idx].service) * 5));
            canvas_draw_str(canvas, sx, ry + 7, m->ports[idx].service);
        }
    }

    /* Scroll indicator */
    if(m->port_count > visible_rows) {
        uint8_t bar_h = (uint8_t)((visible_rows * (64 - list_y)) / m->port_count);
        uint8_t bar_y = (uint8_t)(list_y + (m->scroll_offset * (64 - list_y)) / m->port_count);
        canvas_draw_box(canvas, 126, bar_y, 2, bar_h);
    }
}

/* =========================================================================
 * Port scan view — input callback
 * ========================================================================= */
static bool fpwn_port_scan_input(InputEvent* event, void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;

    with_view_model(
        app->port_scan_view,
        FPwnPortScanModel * m,
        {
            const uint8_t visible_rows = 5;
            uint8_t max_scroll =
                m->port_count > visible_rows ? (uint8_t)(m->port_count - visible_rows) : 0;

            if(event->key == InputKeyUp) {
                if(m->scroll_offset > 0) m->scroll_offset--;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(m->scroll_offset < max_scroll) m->scroll_offset++;
                consumed = true;
            }
        },
        consumed);

    return consumed;
}

/* =========================================================================
 * Scan timer callback — fires every 500 ms on the timer service thread.
 *
 * Polls the marauder for fresh results and pushes them into the appropriate
 * view model, triggering a canvas redraw.  Safe to call from timer thread
 * because with_view_model uses an internal mutex.
 * ========================================================================= */
static void fpwn_scan_timer_cb(void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;
    if(!app->marauder) return;

    FPwnMarauderState state = fpwn_marauder_get_state(app->marauder);

    if(state == FPwnMarauderStateScanning) {
        uint32_t count = 0;
        FPwnWifiAP* aps = fpwn_marauder_get_aps(app->marauder, &count);
        if(count > FPWN_MAX_APS) count = FPWN_MAX_APS;
        with_view_model(
            app->wifi_scan_view,
            FPwnWifiScanModel * m,
            {
                memcpy(m->aps, aps, count * sizeof(FPwnWifiAP));
                m->ap_count = count;
                m->scanning = true;
            },
            true);
    } else if(state == FPwnMarauderStatePingScan) {
        uint32_t count = 0;
        FPwnNetHost* hosts = fpwn_marauder_get_hosts(app->marauder, &count);
        if(count > FPWN_MAX_HOSTS) count = FPWN_MAX_HOSTS;
        with_view_model(
            app->ping_scan_view,
            FPwnPingScanModel * m,
            {
                memcpy(m->hosts, hosts, count * sizeof(FPwnNetHost));
                m->host_count = count;
                m->scanning = true;
            },
            true);
    } else if(state == FPwnMarauderStatePortScan) {
        uint32_t count = 0;
        FPwnPortResult* ports = fpwn_marauder_get_ports(app->marauder, &count);
        if(count > FPWN_MAX_PORTS) count = FPWN_MAX_PORTS;
        with_view_model(
            app->port_scan_view,
            FPwnPortScanModel * m,
            {
                memcpy(m->ports, ports, count * sizeof(FPwnPortResult));
                m->port_count = count;
            },
            true);
    }
}

/* =========================================================================
 * Marauder RX callback — appends received lines to the status TextBox and
 * notifies the main thread on first UART connection so the menu label
 * ("WiFi Tools (No ESP32)") can be updated.
 *
 * Fired on the UART worker thread.  We must not call view_dispatcher
 * functions that are NOT thread-safe.  send_custom_event IS safe from any
 * thread.
 * ========================================================================= */
static void fpwn_wifi_rx_callback(const char* line, void* ctx) {
    FPwnApp* app = (FPwnApp*)ctx;

    furi_string_cat_printf(app->wifi_status_text, "%s\n", line);
    text_box_set_text(app->wifi_status, furi_string_get_cstr(app->wifi_status_text));

    /* Rebuild the main menu once when ESP32 first responds. */
    if(!s_wifi_first_connect_notified && fpwn_wifi_uart_is_connected(app->wifi_uart)) {
        s_wifi_first_connect_notified = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, FPWN_CUSTOM_EVENT_WIFI_CONNECTED);
    }
}

/* =========================================================================
 * WiFi menu callback — dispatches to sub-views
 * ========================================================================= */
static void fpwn_wifi_menu_callback(void* ctx, uint32_t index) {
    FPwnApp* app = (FPwnApp*)ctx;

    switch((FPwnWifiMenuItem)index) {
    case FPwnWifiMenuScanAP: {
        /* Reset scan model and kick off a fresh scan */
        with_view_model(
            app->wifi_scan_view,
            FPwnWifiScanModel * m,
            {
                memset(m, 0, sizeof(FPwnWifiScanModel));
                m->scanning = true;
            },
            true);
        fpwn_marauder_scan_ap(app->marauder);
        fpwn_set_current_view(FPwnViewWifiScan);
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiScan);
        break;
    }

    case FPwnWifiMenuJoinNetwork:
        /* Go to scan first — user picks AP then enters password */
        with_view_model(
            app->wifi_scan_view,
            FPwnWifiScanModel * m,
            {
                memset(m, 0, sizeof(FPwnWifiScanModel));
                m->scanning = true;
            },
            true);
        fpwn_marauder_scan_ap(app->marauder);
        fpwn_set_current_view(FPwnViewWifiScan);
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiScan);
        break;

    case FPwnWifiMenuPingScan: {
        /* Reset ping model and start scan */
        with_view_model(
            app->ping_scan_view,
            FPwnPingScanModel * m,
            {
                memset(m, 0, sizeof(FPwnPingScanModel));
                m->scanning = true;
            },
            true);
        fpwn_marauder_ping_scan(app->marauder);
        fpwn_set_current_view(FPwnViewPingScan);
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewPingScan);
        break;
    }

    case FPwnWifiMenuPortScan:
        /* Navigate to ping scan first — user picks a host */
        with_view_model(
            app->ping_scan_view,
            FPwnPingScanModel * m,
            {
                memset(m, 0, sizeof(FPwnPingScanModel));
                m->scanning = true;
            },
            true);
        fpwn_marauder_ping_scan(app->marauder);
        fpwn_set_current_view(FPwnViewPingScan);
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewPingScan);
        break;

    case FPwnWifiMenuDeauth:
        fpwn_marauder_deauth(app->marauder);
        /* Show status log so the user can see deauth frames being sent */
        furi_string_reset(app->wifi_status_text);
        text_box_reset(app->wifi_status);
        fpwn_set_current_view(FPwnViewWifiStatus);
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiStatus);
        break;

    case FPwnWifiMenuSniffPmkid:
        fpwn_marauder_sniff_pmkid(app->marauder);
        furi_string_reset(app->wifi_status_text);
        text_box_reset(app->wifi_status);
        fpwn_set_current_view(FPwnViewWifiStatus);
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiStatus);
        break;

    case FPwnWifiMenuStatus:
        fpwn_set_current_view(FPwnViewWifiStatus);
        view_dispatcher_switch_to_view(app->view_dispatcher, FPwnViewWifiStatus);
        break;
    }
}

/* =========================================================================
 * WiFi menu setup — rebuilds the submenu items
 * ========================================================================= */
void fpwn_wifi_menu_setup(FPwnApp* app) {
    submenu_reset(app->wifi_menu);
    submenu_set_header(app->wifi_menu, "WiFi Tools");
    submenu_add_item(app->wifi_menu, "Scan APs", FPwnWifiMenuScanAP, fpwn_wifi_menu_callback, app);
    submenu_add_item(
        app->wifi_menu, "Join Network", FPwnWifiMenuJoinNetwork, fpwn_wifi_menu_callback, app);
    submenu_add_item(
        app->wifi_menu, "Ping Scan", FPwnWifiMenuPingScan, fpwn_wifi_menu_callback, app);
    submenu_add_item(
        app->wifi_menu, "Port Scan", FPwnWifiMenuPortScan, fpwn_wifi_menu_callback, app);
    submenu_add_item(
        app->wifi_menu, "Deauth Attack", FPwnWifiMenuDeauth, fpwn_wifi_menu_callback, app);
    submenu_add_item(
        app->wifi_menu, "Sniff PMKID", FPwnWifiMenuSniffPmkid, fpwn_wifi_menu_callback, app);
    submenu_add_item(
        app->wifi_menu, "Status Log", FPwnWifiMenuStatus, fpwn_wifi_menu_callback, app);
}

/* =========================================================================
 * fpwn_wifi_views_alloc — allocate and register all WiFi views
 * ========================================================================= */
void fpwn_wifi_views_alloc(FPwnApp* app) {
    /* Reset first-connect flag for a fresh session. */
    s_wifi_first_connect_notified = false;

    /* ---- UART + Marauder layer ---- */
    app->wifi_uart = fpwn_wifi_uart_alloc();
    app->marauder = fpwn_marauder_alloc(app->wifi_uart);

    /* Register the status log as a secondary callback on the marauder layer.
     * This fires for every line AFTER the marauder parser has processed it,
     * so both parsing and the status TextBox work simultaneously. */
    fpwn_marauder_set_log_callback(app->marauder, fpwn_wifi_rx_callback, app);

    /* ---- Status string ---- */
    app->wifi_status_text = furi_string_alloc();

    /* ---- WiFi menu submenu ---- */
    app->wifi_menu = submenu_alloc();
    fpwn_wifi_menu_setup(app);
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewWifiMenu, submenu_get_view(app->wifi_menu));

    /* ---- AP scan view ---- */
    app->wifi_scan_view = view_alloc();
    view_set_context(app->wifi_scan_view, app);
    view_set_draw_callback(app->wifi_scan_view, fpwn_wifi_scan_draw);
    view_set_input_callback(app->wifi_scan_view, fpwn_wifi_scan_input);
    view_allocate_model(app->wifi_scan_view, ViewModelTypeLocking, sizeof(FPwnWifiScanModel));
    with_view_model(
        app->wifi_scan_view,
        FPwnWifiScanModel * m,
        { memset(m, 0, sizeof(FPwnWifiScanModel)); },
        false);
    view_dispatcher_add_view(app->view_dispatcher, FPwnViewWifiScan, app->wifi_scan_view);

    /* ---- Password text input ---- */
    app->wifi_text_input = text_input_alloc();
    text_input_set_header_text(app->wifi_text_input, "Password (empty=open)");
    text_input_set_result_callback(
        app->wifi_text_input,
        fpwn_wifi_password_done,
        app,
        app->wifi_text_buf,
        sizeof(app->wifi_text_buf),
        false);
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewWifiPassword, text_input_get_view(app->wifi_text_input));

    /* ---- Status TextBox ---- */
    app->wifi_status = text_box_alloc();
    text_box_set_font(app->wifi_status, TextBoxFontText);
    /* TextBoxFocusStart lets the user scroll up through history.
     * New lines accumulate but the view position is not force-jumped
     * to the end on every update. */
    text_box_set_focus(app->wifi_status, TextBoxFocusStart);
    view_dispatcher_add_view(
        app->view_dispatcher, FPwnViewWifiStatus, text_box_get_view(app->wifi_status));

    /* ---- Ping scan view ---- */
    app->ping_scan_view = view_alloc();
    view_set_context(app->ping_scan_view, app);
    view_set_draw_callback(app->ping_scan_view, fpwn_ping_scan_draw);
    view_set_input_callback(app->ping_scan_view, fpwn_ping_scan_input);
    view_allocate_model(app->ping_scan_view, ViewModelTypeLocking, sizeof(FPwnPingScanModel));
    with_view_model(
        app->ping_scan_view,
        FPwnPingScanModel * m,
        { memset(m, 0, sizeof(FPwnPingScanModel)); },
        false);
    view_dispatcher_add_view(app->view_dispatcher, FPwnViewPingScan, app->ping_scan_view);

    /* ---- Port scan view ---- */
    app->port_scan_view = view_alloc();
    view_set_context(app->port_scan_view, app);
    view_set_draw_callback(app->port_scan_view, fpwn_port_scan_draw);
    view_set_input_callback(app->port_scan_view, fpwn_port_scan_input);
    view_allocate_model(app->port_scan_view, ViewModelTypeLocking, sizeof(FPwnPortScanModel));
    with_view_model(
        app->port_scan_view,
        FPwnPortScanModel * m,
        { memset(m, 0, sizeof(FPwnPortScanModel)); },
        false);
    view_dispatcher_add_view(app->view_dispatcher, FPwnViewPortScan, app->port_scan_view);

    /* ---- Scan refresh timer (500 ms) ---- */
    app->wifi_scan_timer = furi_timer_alloc(fpwn_scan_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->wifi_scan_timer, 500);
}

/* =========================================================================
 * fpwn_wifi_views_free — remove views and release all WiFi resources
 * ========================================================================= */
void fpwn_wifi_views_free(FPwnApp* app) {
    /* Stop and free the scan refresh timer first. */
    if(app->wifi_scan_timer) {
        furi_timer_stop(app->wifi_scan_timer);
        furi_timer_free(app->wifi_scan_timer);
        app->wifi_scan_timer = NULL;
    }

    /* Stop any active Marauder operation before tearing down */
    if(fpwn_marauder_get_state(app->marauder) != FPwnMarauderStateIdle) {
        fpwn_marauder_stop(app->marauder);
    }

    /* Remove views from dispatcher first, then free underlying objects */
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewWifiMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewWifiScan);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewWifiPassword);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewWifiStatus);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewPingScan);
    view_dispatcher_remove_view(app->view_dispatcher, FPwnViewPortScan);

    submenu_free(app->wifi_menu);
    view_free(app->wifi_scan_view);
    text_input_free(app->wifi_text_input);
    text_box_free(app->wifi_status);
    view_free(app->ping_scan_view);
    view_free(app->port_scan_view);

    furi_string_free(app->wifi_status_text);

    /* Free Marauder before UART (marauder holds a reference to uart) */
    fpwn_marauder_free(app->marauder);
    fpwn_wifi_uart_free(app->wifi_uart);

    app->marauder = NULL;
    app->wifi_uart = NULL;
}
