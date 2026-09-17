#include "../marauder_gui_app_i.h"
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Marauder's "list -a" only reads its internal AP list, it doesn't touch scan state, so we can
   poll it periodically while "scanall" keeps running to build the list live. */
#define WIFI_SCAN_REFRESH_TICKS 15 /* ~1.5s at the app's 100ms tick period */
#define WIFI_LIST_MARQUEE_TICKS 3 /* advance the marquee once every N ticks - "slowly" */
#define WIFI_LIST_MARQUEE_DELAY_TICKS \
    30 /* ~3s pause on a newly-highlighted row before it scrolls */
#define WIFI_LIST_ROW_HEIGHT    12
#define WIFI_LIST_HEADER_HEIGHT 11
#define WIFI_LIST_VISIBLE_ROWS  4

/* ---- Custom list View: Submenu doesn't scroll long labels, and SSIDs routinely don't fit the
   128px screen, so the highlighted row needs its own marquee (via elements_scrollable_text_line)
   to ever become fully readable. The view's "model" is just a pointer back to the app, since
   the actual list data (ap_list/ap_count) already lives there and everything in this app runs
   on the single ViewDispatcher thread - no real locking is needed. ---- */

void marauder_gui_wifi_list_redraw(MarauderGuiApp* app) {
    with_view_model(app->wifi_list_view, MarauderGuiApp * *model, { UNUSED(model); }, true);
}

/* Small ascending-bars "signal" glyph plus a count, drawn top-right of the header - opt-in via
   wifi_list_show_selected_count since this view is shared by several scenes that have no such
   concept (see marauder_gui_app_i.h). Counts "[x] " markers wifi_select_aps.c writes into
   ap_list itself, so no separate counter needs to be kept in sync. */
static void marauder_gui_wifi_list_draw_selected_badge(Canvas* canvas, MarauderGuiApp* app) {
    size_t selected = 0;
    for(size_t i = 0; i < app->ap_count; i++) {
        if(app->ap_list[i][1] == 'x') selected++;
    }

    char count_str[8];
    snprintf(count_str, sizeof(count_str), "%u", (unsigned)selected);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 113, 9, AlignRight, AlignBottom, count_str);

    /* 3 bars of increasing height, bottom-aligned to the header baseline. */
    canvas_draw_box(canvas, 115, 6, 2, 3);
    canvas_draw_box(canvas, 118, 4, 2, 5);
    canvas_draw_box(canvas, 121, 2, 2, 7);
}

static void marauder_gui_wifi_list_draw_callback(Canvas* canvas, void* model) {
    MarauderGuiApp* app = *(MarauderGuiApp**)model;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(
        canvas,
        2,
        9,
        app->wifi_scan_frozen ?
            (app->wifi_list_frozen_label ? app->wifi_list_frozen_label :
                                           marauder_gui_text(app, "Durduruldu", "Stopped")) :
            app->wifi_list_scanning_label);

    if(app->wifi_list_show_selected_count) {
        marauder_gui_wifi_list_draw_selected_badge(canvas, app);
    }

    canvas_set_font(canvas, FontSecondary);

    if(app->ap_count == 0) {
        canvas_draw_str(canvas, 2, WIFI_LIST_HEADER_HEIGHT + 12, app->wifi_list_empty_label);
        return;
    }

    for(size_t row = 0; row < WIFI_LIST_VISIBLE_ROWS; row++) {
        size_t idx = app->wifi_list_scroll_offset + row;
        if(idx >= app->ap_count) break;

        int32_t y = WIFI_LIST_HEADER_HEIGHT + (int32_t)((row + 1) * WIFI_LIST_ROW_HEIGHT) - 2;
        bool selected = (idx == app->wifi_list_selected);

        if(selected) {
            canvas_draw_box(
                canvas,
                0,
                WIFI_LIST_HEADER_HEIGHT + (int32_t)(row * WIFI_LIST_ROW_HEIGHT) + 1,
                canvas_width(canvas),
                WIFI_LIST_ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);

            FuriString* text = furi_string_alloc_set_str(app->ap_list[idx]);
            elements_scrollable_text_line(
                canvas, 2, y, canvas_width(canvas) - 4, text, app->wifi_list_marquee_tick, false);
            furi_string_free(text);

            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 2, y, app->ap_list[idx]);
        }
    }

    if(app->ap_count > WIFI_LIST_VISIBLE_ROWS) {
        elements_scrollbar_pos(
            canvas,
            canvas_width(canvas) - 3,
            WIFI_LIST_HEADER_HEIGHT + 1,
            canvas_height(canvas) - WIFI_LIST_HEADER_HEIGHT - 1,
            app->wifi_list_selected,
            app->ap_count);
    }
}

static bool marauder_gui_wifi_list_input_callback(InputEvent* event, void* context) {
    MarauderGuiApp* app = context;

    if(app->ap_count == 0) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyUp) {
        app->wifi_list_selected = (app->wifi_list_selected == 0) ? app->ap_count - 1 :
                                                                   app->wifi_list_selected - 1;
        app->wifi_list_marquee_tick = 0;
        app->wifi_list_marquee_hold = 0;
        app->wifi_list_marquee_delay = WIFI_LIST_MARQUEE_DELAY_TICKS;
    } else if(event->key == InputKeyDown) {
        app->wifi_list_selected =
            (app->wifi_list_selected + 1 >= app->ap_count) ? 0 : app->wifi_list_selected + 1;
        app->wifi_list_marquee_tick = 0;
        app->wifi_list_marquee_hold = 0;
        app->wifi_list_marquee_delay = WIFI_LIST_MARQUEE_DELAY_TICKS;
    } else if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, app->wifi_list_selected);
        return true;
    } else if(event->key == InputKeyRight) {
        if(!app->wifi_list_show_selected_count) return false;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MARAUDER_WIFI_LIST_PROCEED_CUSTOM_EVENT);
        return true;
    } else {
        return false;
    }

    if(app->wifi_list_selected < app->wifi_list_scroll_offset) {
        app->wifi_list_scroll_offset = app->wifi_list_selected;
    } else if(app->wifi_list_selected >= app->wifi_list_scroll_offset + WIFI_LIST_VISIBLE_ROWS) {
        app->wifi_list_scroll_offset = app->wifi_list_selected - WIFI_LIST_VISIBLE_ROWS + 1;
    }

    marauder_gui_wifi_list_redraw(app);
    return true;
}

View* marauder_gui_wifi_list_view_alloc(MarauderGuiApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(MarauderGuiApp*));
    with_view_model(view, MarauderGuiApp * *model, { *model = app; }, false);
    view_set_draw_callback(view, marauder_gui_wifi_list_draw_callback);
    view_set_input_callback(view, marauder_gui_wifi_list_input_callback);
    view_set_context(view, app);
    return view;
}

/* ---- Scene ---- */

/* Marauder prints AP entries as "[<index>][CH:<n>] <ssid> <rssi>", one per line, in the
   same order as its internal access_points list (the index "select -a" expects). Any other
   line (echoed command, "> " prompt, ...) is ignored.

   Every periodic "list -a" re-sends the FULL list from index 0, not just what's new - so
   rebuilding the list from scratch each time was what reset the user's scroll position.
   Instead, only append an entry the first time its index is seen (idx == ap_count, i.e. the
   next one we're expecting); re-transmitted lines for indices we already have are skipped. */
static void marauder_gui_scene_wifi_scanning_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] != '[') return;

    long idx = strtol(line + 1, NULL, 10);
    if(idx < 0 || (size_t)idx != app->ap_count) return;
    if(app->ap_count >= MARAUDER_AP_LIST_MAX) return;

    strncpy(app->ap_list[app->ap_count], line, MARAUDER_LINE_MAX - 1);
    app->ap_list[app->ap_count][MARAUDER_LINE_MAX - 1] = '\0';
    app->ap_count++;

    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_scene_wifi_scanning_tick(MarauderGuiApp* app) {
    app->wifi_scan_refresh_tick++;
    if(app->wifi_scan_refresh_tick >= WIFI_SCAN_REFRESH_TICKS) {
        app->wifi_scan_refresh_tick = 0;
        if(!app->wifi_scan_frozen) {
            marauder_uart_send_line(app->uart, "list -a");
        }
    }

    /* elements_scrollable_text_line derives scroll speed straight from how fast this counter
       grows, not from how often we redraw - so the counter itself must be throttled to slow
       the marquee down, rather than just skipping redraws while still bumping it every tick. */
    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= WIFI_LIST_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

void marauder_gui_scene_wifi_scanning_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    app->wifi_scan_refresh_tick = 0;
    app->wifi_scan_frozen = false;
    app->wifi_scan_keep_alive = false;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = WIFI_LIST_MARQUEE_DELAY_TICKS;
    app->wifi_list_scanning_label = marauder_gui_text(app, "Taraniyor...", "Scanning...");
    app->wifi_list_empty_label = marauder_gui_text(app, "AP araniyor...", "Searching for AP...");

    app->uart_line_handler = marauder_gui_scene_wifi_scanning_uart_line;
    app->tick_handler = marauder_gui_scene_wifi_scanning_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);

    marauder_uart_send_line(app->uart, "scanall");
}

bool marauder_gui_scene_wifi_scanning_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->ap_count) {
            app->selected_ap_index = (int)event.event;
            /* Targeted deauth needs a station picked under this AP first; joining WiFi needs a
               password next; every other attack type goes straight to the select-and-attack
               screen. */
            if(app->wifi_ap_attack_type == 3 || app->wifi_ap_attack_type == 11 ||
               app->wifi_ap_attack_type == 15) {
                app->wifi_scan_keep_alive = true;
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiStationScan);
            } else if(app->wifi_ap_attack_type == 9) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiJoinPassword);
            } else if(app->wifi_ap_attack_type == 10) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiApInfo);
            } else if(app->wifi_ap_attack_type == 12) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiCloneApMac);
            } else if(app->wifi_ap_attack_type == 13) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiPacketCount);
            } else if(app->wifi_ap_attack_type == 14) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiFoxHunt);
            } else if(app->wifi_ap_attack_type == 20) {
                scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapSniff);
            } else {
                scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiAttack);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(!app->wifi_scan_frozen) {
            /* First Back: stop scanning and freeze the list in place (no more periodic
               rebuilds), so the user can scroll it without losing their position */
            app->wifi_scan_frozen = true;
            marauder_uart_send_line(app->uart, "stopscan");
            marauder_gui_wifi_list_redraw(app);
        } else {
            /* Second Back: actually leave the scene. (Holding Back, at any point, is handled
               by the OS itself and force-closes the app - no extra code needed for that.) */
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_scanning_on_exit(void* context) {
    MarauderGuiApp* app = context;

    /* Targeted Deauth, Clone STA MAC, and Station Fox Hunt all need to keep discovering stations
       under the picked AP on the next screen, and "scanall" wipes the whole AP list on every
       call (RunAPScan does `delete access_points; access_points = new LinkedList<...>()`) -
       restarting it there would destroy the very AP we just selected. So only when we're
       actually headed into one of those flows (not just because wifi_ap_attack_type happens to
       still be 3/11/15 from backing out of it entirely) do we leave the scan running
       uninterrupted for wifi_station_scan.c to inherit and eventually stop. Every other exit
       path stops it here as usual - which Clone AP MAC's "cloneapmac", AP Bilgisi's "info -a",
       Packet Count's "packetcount", and AP Fox Hunt's "foxhunt -w" all rely on, since the first
       three are silently ignored while a scan is still running. */
    if(!app->wifi_scan_keep_alive) {
        marauder_uart_send_line(app->uart, "stopscan");
    }

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
}
