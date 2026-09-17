#include "../marauder_gui_app_i.h"
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

/* Filterable, scrollable table over an already-indexed .pcap file (see
   marauder_gui_pcap_index.c). Selected/scroll_offset are always indices of currently-MATCHING
   entries (an invariant kept by the navigation helpers below) - filtering never touches the
   index itself, it's re-applied on every redraw/navigation, so toggling filters in
   pcap_filter.c and coming back here is effectively free (no re-parsing the file). */

#define MARAUDER_PCAP_INDEX_CAPACITY     2000
#define MARAUDER_PCAP_INDEX_MIN_CAPACITY 20
#define PCAP_VIEW_ROW_HEIGHT             12
#define PCAP_VIEW_HEADER_HEIGHT          11
#define PCAP_VIEW_VISIBLE_ROWS           4
#define PCAP_VIEW_MARQUEE_TICKS          3
#define PCAP_VIEW_MARQUEE_DELAY_TICKS    30

static bool marauder_pcap_view_matches(MarauderGuiApp* app, size_t idx) {
    return marauder_pcap_entry_matches_filter(
        &app->pcap_index[idx],
        app->pcap_filter_type_mask,
        app->pcap_filter_ssids,
        app->pcap_filter_ssid_count);
}

/* Returns app->pcap_index_count (an always-invalid index) as the "no such entry" sentinel, so
   callers never need a separate found/not-found flag. */
static size_t marauder_pcap_view_next_match(MarauderGuiApp* app, size_t from) {
    for(size_t i = from + 1; i < app->pcap_index_count; i++) {
        if(marauder_pcap_view_matches(app, i)) return i;
    }
    return app->pcap_index_count;
}

static size_t marauder_pcap_view_prev_match(MarauderGuiApp* app, size_t from) {
    for(size_t i = from; i-- > 0;) {
        if(marauder_pcap_view_matches(app, i)) return i;
    }
    return app->pcap_index_count;
}

static size_t marauder_pcap_view_first_match(MarauderGuiApp* app) {
    for(size_t i = 0; i < app->pcap_index_count; i++) {
        if(marauder_pcap_view_matches(app, i)) return i;
    }
    return app->pcap_index_count;
}

static size_t marauder_pcap_view_count_matches(MarauderGuiApp* app, size_t from, size_t to_incl) {
    size_t c = 0;
    for(size_t i = from; i <= to_incl && i < app->pcap_index_count; i++) {
        if(marauder_pcap_view_matches(app, i)) c++;
    }
    return c;
}

/* Keeps scroll_offset such that selected lands within the PCAP_VIEW_VISIBLE_ROWS matching rows
   drawn starting at scroll_offset - same "scroll window follows selection" idea as every other
   list view in this app, just walking only matching entries instead of every row. */
static void marauder_pcap_view_ensure_visible(MarauderGuiApp* app) {
    if(app->pcap_view_selected < app->pcap_view_scroll_offset) {
        app->pcap_view_scroll_offset = app->pcap_view_selected;
        return;
    }
    while(app->pcap_index_count > 0 &&
          marauder_pcap_view_count_matches(
              app, app->pcap_view_scroll_offset, app->pcap_view_selected) >
              PCAP_VIEW_VISIBLE_ROWS) {
        size_t next = marauder_pcap_view_next_match(app, app->pcap_view_scroll_offset);
        if(next >= app->pcap_index_count) break;
        app->pcap_view_scroll_offset = next;
    }
}

static void
    marauder_pcap_view_format_row(MarauderGuiApp* app, size_t idx, char* out, size_t out_size) {
    const MarauderPcapIndexEntry* e = &app->pcap_index[idx];
    const char* type_label = marauder_pcap_frame_type_label(
        (MarauderPcapFrameType)e->frame_type, app->language == MarauderLanguageEnglish);
    if(e->ssid[0]) {
        snprintf(out, out_size, "%u %s %s", (unsigned)idx, type_label, e->ssid);
    } else {
        snprintf(out, out_size, "%u %s", (unsigned)idx, type_label);
    }
}

static void marauder_pcap_table_draw_callback(Canvas* canvas, void* model) {
    MarauderGuiApp* app = *(MarauderGuiApp**)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(app->pcap_index_count == 0) {
        canvas_draw_str(
            canvas, 2, 9, marauder_gui_text(app, "Bos/gecersiz .pcap", "Empty/invalid .pcap"));
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas,
            2,
            22,
            marauder_gui_text(app, "Sag: Yeniden Adlandir/Sil", "Right: Rename/Delete"));
        return;
    }

    size_t total_matches = marauder_pcap_view_count_matches(app, 0, app->pcap_index_count - 1);

    char header[48];
    snprintf(
        header,
        sizeof(header),
        "%s: %u/%u%s",
        marauder_gui_text(app, "Eslesen", "Matches"),
        (unsigned)total_matches,
        (unsigned)app->pcap_index_count,
        app->pcap_index_truncated ? "+" : "");
    FuriString* h = furi_string_alloc_set_str(header);
    elements_scrollable_text_line(canvas, 2, 9, canvas_width(canvas) - 4, h, 0, false);
    furi_string_free(h);
    canvas_draw_line(
        canvas, 0, PCAP_VIEW_HEADER_HEIGHT, canvas_width(canvas), PCAP_VIEW_HEADER_HEIGHT);

    canvas_set_font(canvas, FontSecondary);

    if(total_matches == 0) {
        canvas_draw_str(
            canvas,
            2,
            PCAP_VIEW_HEADER_HEIGHT + 12,
            marauder_gui_text(app, "Filtreye uyan yok", "No matches"));
        return;
    }

    size_t idx = app->pcap_view_scroll_offset;
    for(size_t row = 0; row < PCAP_VIEW_VISIBLE_ROWS && idx < app->pcap_index_count; row++) {
        while(idx < app->pcap_index_count && !marauder_pcap_view_matches(app, idx))
            idx++;
        if(idx >= app->pcap_index_count) break;

        int32_t y = PCAP_VIEW_HEADER_HEIGHT + (int32_t)((row + 1) * PCAP_VIEW_ROW_HEIGHT) - 2;
        bool selected = (idx == app->pcap_view_selected);

        char line[48];
        marauder_pcap_view_format_row(app, idx, line, sizeof(line));

        if(selected) {
            canvas_draw_box(
                canvas,
                0,
                PCAP_VIEW_HEADER_HEIGHT + (int32_t)(row * PCAP_VIEW_ROW_HEIGHT) + 1,
                canvas_width(canvas),
                PCAP_VIEW_ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);
            FuriString* t = furi_string_alloc_set_str(line);
            elements_scrollable_text_line(
                canvas, 2, y, canvas_width(canvas) - 4, t, app->pcap_view_marquee_tick, false);
            furi_string_free(t);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 2, y, line);
        }
        idx++;
    }
}

static bool marauder_pcap_table_input_callback(InputEvent* event, void* context) {
    MarauderGuiApp* app = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    /* An empty/invalid file has no rows to navigate or open, but Right must still reach
       pcap_filter.c - that's the only place Rename/Delete live, and a bad capture is exactly the
       case someone most needs to rename or delete (see pcap_filter.c for the empty-file mode
       that hides the now-meaningless type/SSID rows and shows only those two). */
    if(app->pcap_index_count == 0) {
        if(event->key == InputKeyRight) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MARAUDER_WIFI_LIST_PROCEED_CUSTOM_EVENT);
            return true;
        }
        return false;
    }

    if(event->key == InputKeyUp) {
        size_t prev = marauder_pcap_view_prev_match(app, app->pcap_view_selected);
        if(prev >= app->pcap_index_count) return true;
        app->pcap_view_selected = prev;
        marauder_pcap_view_ensure_visible(app);
        app->pcap_view_marquee_tick = 0;
        app->pcap_view_marquee_hold = 0;
        app->pcap_view_marquee_delay = PCAP_VIEW_MARQUEE_DELAY_TICKS;
    } else if(event->key == InputKeyDown) {
        size_t next = marauder_pcap_view_next_match(app, app->pcap_view_selected);
        if(next >= app->pcap_index_count) return true;
        app->pcap_view_selected = next;
        marauder_pcap_view_ensure_visible(app);
        app->pcap_view_marquee_tick = 0;
        app->pcap_view_marquee_hold = 0;
        app->pcap_view_marquee_delay = PCAP_VIEW_MARQUEE_DELAY_TICKS;
    } else if(event->key == InputKeyRight) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MARAUDER_WIFI_LIST_PROCEED_CUSTOM_EVENT);
        return true;
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        /* Row index itself as the event value - same convention as the WifiList/Menu views. */
        view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)app->pcap_view_selected);
        return true;
    } else {
        return false;
    }

    marauder_gui_pcap_table_redraw(app);
    return true;
}

void marauder_gui_pcap_table_redraw(MarauderGuiApp* app) {
    with_view_model(app->pcap_table_view, MarauderGuiApp * *model, { UNUSED(model); }, true);
}

View* marauder_gui_pcap_table_view_alloc(MarauderGuiApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(MarauderGuiApp*));
    with_view_model(view, MarauderGuiApp * *model, { *model = app; }, false);
    view_set_draw_callback(view, marauder_pcap_table_draw_callback);
    view_set_input_callback(view, marauder_pcap_table_input_callback);
    view_set_context(view, app);
    return view;
}

static void marauder_gui_scene_pcap_view_tick(MarauderGuiApp* app) {
    if(app->pcap_view_marquee_delay > 0) {
        app->pcap_view_marquee_delay--;
    } else {
        app->pcap_view_marquee_hold++;
        if(app->pcap_view_marquee_hold >= PCAP_VIEW_MARQUEE_TICKS) {
            app->pcap_view_marquee_hold = 0;
            app->pcap_view_marquee_tick++;
            marauder_gui_pcap_table_redraw(app);
        }
    }
}

void marauder_gui_scene_pcap_view_on_enter(void* context) {
    MarauderGuiApp* app = context;

    bool same_file = app->pcap_index &&
                     strcmp(app->pcap_index_loaded_filename, app->pcap_view_filename) == 0;

    if(!same_file) {
        if(!app->pcap_index) {
            /* Flipper's allocator crashes the whole device with an "out of memory" screen when a
               malloc can't be satisfied, instead of returning NULL for us to handle - so the old
               fixed MARAUDER_PCAP_INDEX_CAPACITY (2000 entries, ~48KB in one shot) was a gamble:
               it only worked as long as whatever else this session had already allocated left
               enough headroom, and a heavier session (e.g. the live capture dashboard tracking
               more APs, more scenes visited) could push it over. Size the request off actual free
               heap instead: never ask for more than a third of what's currently free, so there's
               always headroom left for the rest of the app - the file is still fully readable
               either way, just capped at fewer indexed packets (pcap_index_truncated already
               shows a "+" for that in the header). Below MIN_CAPACITY there's so little heap left
               that even a small index isn't worth risking - skip the allocation entirely and let
               pcap_index stay NULL, which marauder_pcap_index_build() already treats as "return
               0 entries" (shown as the existing empty/invalid message). */
            size_t free_heap = memmgr_get_free_heap();
            size_t budget_entries = (free_heap / 3) / sizeof(MarauderPcapIndexEntry);
            size_t capacity = (budget_entries < MARAUDER_PCAP_INDEX_CAPACITY) ?
                                  budget_entries :
                                  MARAUDER_PCAP_INDEX_CAPACITY;
            if(capacity >= MARAUDER_PCAP_INDEX_MIN_CAPACITY) {
                app->pcap_index_capacity = capacity;
                app->pcap_index =
                    malloc(sizeof(MarauderPcapIndexEntry) * app->pcap_index_capacity);
            }
        }

        char path[128];
        snprintf(path, sizeof(path), "%s/%s", MARAUDER_GUI_PCAP_DIR, app->pcap_view_filename);
        app->pcap_index_count = marauder_pcap_index_build(
            app->storage,
            path,
            app->pcap_index,
            app->pcap_index_capacity,
            &app->pcap_index_truncated);
        strncpy(
            app->pcap_index_loaded_filename,
            app->pcap_view_filename,
            sizeof(app->pcap_index_loaded_filename) - 1);
        app->pcap_index_loaded_filename[sizeof(app->pcap_index_loaded_filename) - 1] = '\0';

        /* Fresh file: show everything until the user narrows it down. */
        app->pcap_filter_type_mask = 0xFFFF;
        app->pcap_filter_ssid_count = 0;

        size_t first = marauder_pcap_view_first_match(app);
        app->pcap_view_selected = (first < app->pcap_index_count) ? first : 0;
        app->pcap_view_scroll_offset = app->pcap_view_selected;
    }

    app->pcap_view_marquee_tick = 0;
    app->pcap_view_marquee_hold = 0;
    app->pcap_view_marquee_delay = PCAP_VIEW_MARQUEE_DELAY_TICKS;
    app->tick_handler = marauder_gui_scene_pcap_view_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewPcapTable);
    marauder_gui_pcap_table_redraw(app);
}

bool marauder_gui_scene_pcap_view_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MARAUDER_WIFI_LIST_PROCEED_CUSTOM_EVENT) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapFilter);
        } else if(event.event < app->pcap_index_count) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapPacketDetail);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_view_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
