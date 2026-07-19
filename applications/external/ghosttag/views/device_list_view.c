#include "device_list_view.h"
#include "ghosttag_icons.h"
#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>

#define VISIBLE_ROWS 4
#define ROW_H 13
#define LIST_TOP 13

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

static void draw_rssi(Canvas* canvas, int x, int baseline, int8_t rssi, bool inverted) {
    int bars = rssi_bars(rssi);
    for(int i = 0; i < 4; i++) {
        int h = 2 + i * 2;
        int bx = x + i * 4;
        int by = baseline - h;
        if(i < bars) {
            canvas_draw_box(canvas, bx, by, 3, h);
        } else {
            canvas_set_color(canvas, inverted ? ColorWhite : ColorBlack);
            canvas_draw_frame(canvas, bx, by, 3, h);
        }
    }
}

static void device_list_view_draw(Canvas* canvas, void* model) {
    DeviceListModel* m = model;
    char buf[28];

    // header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Detections");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->count);
    canvas_draw_str_aligned(canvas, 125, 10, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No trackers yet");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "Run a hunt to scan");
        return;
    }

    for(size_t row = 0; row < VISIBLE_ROWS; row++) {
        size_t idx = m->top + row;
        if(idx >= m->count) break;
        TrackerRecord* r = &m->records[idx];
        int y = LIST_TOP + (int)row * ROW_H;
        int baseline = y + 10;
        bool selected = (idx == m->selected);

        if(selected) {
            canvas_draw_box(canvas, 0, y, 122, ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_icon(canvas, 2, y + 1, type_icon(r->type));

        canvas_set_font(canvas, FontSecondary);
        snprintf(
            buf, sizeof(buf), "%s %02X%02X", tracker_type_short(r->type), r->mac[4], r->mac[5]);
        canvas_draw_str(canvas, 15, baseline, buf);

        draw_rssi(canvas, 90, baseline, r->rssi, selected);

        if(r->following) {
            canvas_draw_icon(canvas, 110, y + 1, &I_warning_10px);
        }

        if(selected) canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar(canvas, m->selected, m->count);
}

static bool device_list_view_input(InputEvent* event, void* context) {
    DeviceListView* dlv = context;
    bool consumed = false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyUp) {
        with_view_model(
            dlv->view,
            DeviceListModel * m,
            {
                if(m->count && m->selected > 0) {
                    m->selected--;
                    if(m->selected < m->top) m->top = m->selected;
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyDown) {
        with_view_model(
            dlv->view,
            DeviceListModel * m,
            {
                if(m->count && m->selected + 1 < m->count) {
                    m->selected++;
                    if(m->selected >= m->top + VISIBLE_ROWS) m->top = m->selected - VISIBLE_ROWS + 1;
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        bool has = false;
        with_view_model(dlv->view, DeviceListModel * m, { has = m->count > 0; }, false);
        if(has && dlv->ok_cb) dlv->ok_cb(dlv->ok_ctx);
        consumed = true;
    }
    return consumed;
}

DeviceListView* device_list_view_alloc(void) {
    DeviceListView* dlv = malloc(sizeof(DeviceListView));
    dlv->ok_cb = NULL;
    dlv->ok_ctx = NULL;
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
            for(size_t i = 0; i < n; i++) m->records[i] = recs[i];
            m->count = n;
            if(m->selected >= n) m->selected = n ? n - 1 : 0;
            if(m->top > m->selected) m->top = m->selected;
            if(m->selected >= m->top + VISIBLE_ROWS) m->top = m->selected - VISIBLE_ROWS + 1;
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
