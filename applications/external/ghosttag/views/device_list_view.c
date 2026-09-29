#include "device_list_view.h"
#include "ghosttag_icons.h"
#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>
#include <string.h>

/* Rows 13..60 - four rows of twelve. The old layout used four rows of
 * thirteen from y=13, which ends at 65 on a screen whose last row is 63: the
 * fourth row's highlight and its text descenders were sliced off. */
#define VISIBLE_ROWS 4
#define ROW_H        12
#define LIST_TOP     13

#define SEL_W   123 /* leave 124..127 for the scrollbar */
#define DWELL_R 78
#define BARS_X  84
#define WARN_X  106

struct DeviceListView {
    View* view;
    DeviceListViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    TrackerRecord records[TRACKER_DB_MAX];
    size_t count;
    size_t selected;
    size_t top;
    uint8_t anchor[6]; /* address of the selected device */
    bool anchored;
    DeviceListState state;
} DeviceListModel;

static const Icon* type_icon(TrackerType t) {
    switch(t) {
    case TrackerTypeAppleFindMy:
    case TrackerTypeAirTagPaired:
    case TrackerTypeChipolo:
        return &I_apple_10px;
    case TrackerTypeTile:
        return &I_tile_10px;
    case TrackerTypeSamsungSmartTag:
        return &I_samsung_10px;
    default:
        return &I_ghost_10px;
    }
}

static int rssi_bars(int8_t rssi) {
    if(rssi >= -54) return 4;
    if(rssi >= -66) return 3;
    if(rssi >= -78) return 2;
    if(rssi >= -90) return 1;
    return 0;
}

static void draw_rssi(Canvas* canvas, int x, int baseline, int8_t rssi) {
    int bars = rssi_bars(rssi);
    for(int i = 0; i < 4; i++) {
        int h = 2 + i * 2;
        int bx = x + i * 4;
        int by = baseline - h;
        if(i < bars) {
            canvas_draw_box(canvas, bx, by, 3, h);
        } else {
            /* An outline, in whatever colour the row is already drawing in -
             * the caller has already inverted for a selected row. */
            canvas_draw_frame(canvas, bx, by, 3, h);
        }
    }
}

static void format_dwell(char* buf, size_t len, const TrackerRecord* r) {
    uint32_t s = (r->last_seen - r->first_seen) / 1000UL;
    uint32_t mins = s / 60UL;
    /* Split the format by decade rather than trusting a runtime width:
     * -Werror=format-truncation will not accept a width it cannot prove. */
    if(mins > 99UL) {
        snprintf(buf, len, "99m+");
    } else {
        snprintf(buf, len, "%u:%02u", (unsigned)mins, (unsigned)(s % 60UL));
    }
}

static void device_list_view_draw(Canvas* canvas, void* model) {
    DeviceListModel* m = model;
    char buf[16];

    /* header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Detections");
    canvas_set_font(canvas, FontSecondary);
    /* Say whether this list is live. A frozen list and a live one looked
     * identical, so a stopped session read as "nothing out there". */
    switch(m->state) {
    case DeviceListStateDemo:
        canvas_draw_str(canvas, 66, 10, "DEMO");
        break;
    case DeviceListStateLive:
        canvas_draw_str(canvas, 66, 10, "LIVE");
        break;
    case DeviceListStateWaiting:
        canvas_draw_str(canvas, 60, 10, "NO BOARD");
        break;
    default:
        break;
    }
    snprintf(buf, sizeof(buf), "%u", (unsigned)(m->count > 999 ? 999 : m->count));
    canvas_draw_str_aligned(canvas, 125, 10, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->count == 0) {
        canvas_set_font(canvas, FontSecondary);
        const char* l1;
        const char* l2;
        const char* l3;
        switch(m->state) {
        case DeviceListStateLive:
            l1 = "Listening";
            l2 = "No trackers in range yet.";
            l3 = "Keep walking.";
            break;
        case DeviceListStateWaiting:
            l1 = "No ESP32 board";
            l2 = "GhostTag cannot hear";
            l3 = "anything without one.";
            break;
        case DeviceListStateDemo:
            l1 = "Demo starting";
            l2 = "The scripted scenario";
            l3 = "takes a moment to fill.";
            break;
        default:
            l1 = "Nothing detected yet";
            l2 = "Start a Hunt, or try";
            l3 = "Demo from the menu.";
            break;
        }
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, l1);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, l2);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, l3);
        return;
    }

    for(size_t row = 0; row < VISIBLE_ROWS; row++) {
        size_t idx = m->top + row;
        if(idx >= m->count) break;
        TrackerRecord* r = &m->records[idx];
        int y = LIST_TOP + (int)row * ROW_H;
        int baseline = y + 9;
        bool selected = (idx == m->selected);

        if(selected) {
            canvas_draw_box(canvas, 0, y, SEL_W, ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_icon(canvas, 2, y + 1, type_icon(r->type));

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 14, baseline, tracker_type_short(r->type));

        /* Right-aligned so a long type name can never run into it. */
        format_dwell(buf, sizeof(buf), r);
        canvas_draw_str_aligned(canvas, DWELL_R, baseline, AlignRight, AlignBottom, buf);

        draw_rssi(canvas, BARS_X, baseline, r->rssi);

        if(r->following) canvas_draw_icon(canvas, WARN_X, y + 1, &I_warning_10px);

        if(selected) canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar(canvas, m->selected, m->count);
}

/* Keep `top` showing `selected` without moving the window more than it must. */
static void reframe(DeviceListModel* m) {
    if(m->count == 0) {
        m->selected = 0;
        m->top = 0;
        return;
    }
    if(m->selected >= m->count) m->selected = m->count - 1;
    if(m->selected < m->top) m->top = m->selected;
    if(m->selected >= m->top + VISIBLE_ROWS) m->top = m->selected - VISIBLE_ROWS + 1;
    if(m->top + VISIBLE_ROWS > m->count) {
        m->top = (m->count > VISIBLE_ROWS) ? m->count - VISIBLE_ROWS : 0;
    }
}

static void anchor_to_selected(DeviceListModel* m) {
    if(m->count == 0 || m->selected >= m->count) {
        m->anchored = false;
        return;
    }
    memcpy(m->anchor, m->records[m->selected].mac, 6);
    m->anchored = true;
}

static bool device_list_view_input(InputEvent* event, void* context) {
    DeviceListView* dlv = context;

    bool nav =
        (event->type == InputTypeShort || event->type == InputTypeLong ||
         event->type == InputTypeRepeat);
    /* Repeat is welcome for scrolling - holding Down should walk the list - but
     * an action key must fire exactly once per physical press. */
    bool act = (event->type == InputTypeShort || event->type == InputTypeLong);

    if(event->key == InputKeyUp && nav) {
        with_view_model(
            dlv->view,
            DeviceListModel * m,
            {
                if(m->count && m->selected > 0) {
                    m->selected--;
                    reframe(m);
                    anchor_to_selected(m);
                }
            },
            true);
        return true;
    }
    if(event->key == InputKeyDown && nav) {
        with_view_model(
            dlv->view,
            DeviceListModel * m,
            {
                if(m->count && m->selected + 1 < m->count) {
                    m->selected++;
                    reframe(m);
                    anchor_to_selected(m);
                }
            },
            true);
        return true;
    }
    if(event->key == InputKeyOk && act) {
        bool has = false;
        with_view_model(dlv->view, DeviceListModel * m, { has = m->count > 0; }, false);
        /* An empty list must not silently swallow OK: the scene shows a
         * message instead, because a press that changes nothing on screen is
         * indistinguishable from a dead button. */
        if(dlv->ok_cb) dlv->ok_cb(dlv->ok_ctx);
        UNUSED(has);
        return true;
    }
    return false;
}

DeviceListView* device_list_view_alloc(void) {
    DeviceListView* dlv = malloc(sizeof(DeviceListView));
    memset(dlv, 0, sizeof(DeviceListView));
    dlv->view = view_alloc();
    view_set_context(dlv->view, dlv);
    view_set_draw_callback(dlv->view, device_list_view_draw);
    view_set_input_callback(dlv->view, device_list_view_input);
    view_allocate_model(dlv->view, ViewModelTypeLocking, sizeof(DeviceListModel));
    return dlv;
}

void device_list_view_free(DeviceListView* dlv) {
    furi_assert(dlv);
    view_free(dlv->view);
    free(dlv);
}

View* device_list_view_get_view(DeviceListView* dlv) {
    furi_assert(dlv);
    return dlv->view;
}

void device_list_view_set_records(DeviceListView* dlv, const TrackerRecord* recs, size_t count) {
    furi_assert(dlv);
    with_view_model(
        dlv->view,
        DeviceListModel * m,
        {
            size_t n = count < TRACKER_DB_MAX ? count : TRACKER_DB_MAX;
            for(size_t i = 0; i < n; i++)
                m->records[i] = recs[i];
            m->count = n;

            /* Follow the anchored device to wherever the re-sort put it. */
            if(m->anchored) {
                bool found = false;
                for(size_t i = 0; i < n; i++) {
                    if(memcmp(m->records[i].mac, m->anchor, 6) == 0) {
                        m->selected = i;
                        found = true;
                        break;
                    }
                }
                /* It was evicted or the session was cleared. Stay where we are
                 * rather than jumping to the top, and re-anchor. */
                if(!found) m->anchored = false;
            }
            reframe(m);
            if(!m->anchored) anchor_to_selected(m);
        },
        true);
}

bool device_list_view_get_selected(DeviceListView* dlv, TrackerRecord* out) {
    furi_assert(dlv);
    bool ok = false;
    with_view_model(
        dlv->view,
        DeviceListModel * m,
        {
            if(m->count > 0 && m->selected < m->count) {
                *out = m->records[m->selected];
                ok = true;
            }
        },
        false);
    return ok;
}

void device_list_view_set_ok_callback(
    DeviceListView* dlv,
    DeviceListViewCallback cb,
    void* context) {
    furi_assert(dlv);
    dlv->ok_cb = cb;
    dlv->ok_ctx = context;
}

void device_list_view_set_state(DeviceListView* dlv, DeviceListState state) {
    furi_assert(dlv);
    with_view_model(dlv->view, DeviceListModel * m, { m->state = state; }, true);
}
