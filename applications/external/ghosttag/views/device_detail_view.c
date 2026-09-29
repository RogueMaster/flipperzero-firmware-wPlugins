#include "device_detail_view.h"
#include "ghosttag_icons.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

struct DeviceDetailView {
    View* view;
    DeviceDetailViewCallback prev_cb;
    DeviceDetailViewCallback next_cb;
    void* step_ctx;
};

typedef struct {
    TrackerRecord rec;
    bool valid;
    bool demo;
    size_t index; /* 1-based; 0 = this device is no longer in the list */
    size_t total;
} DeviceDetailModel;

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

static void device_detail_view_draw(Canvas* canvas, void* model) {
    DeviceDetailModel* m = model;
    char buf[40];

    if(!m->valid) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No device selected");
        return;
    }
    TrackerRecord* r = &m->rec;

    /* ---- header (0..11, divider on 12) ---- */
    canvas_draw_icon(canvas, 2, 1, type_icon(r->type));
    canvas_set_font(canvas, FontPrimary);
    /* The short name, not the long one: "SmartTag" is 48px where "Samsung
     * SmartTag" is 100px, and the icon already carries the vendor. The room
     * that buys is what makes the DEMO stamp and the position counter fit. */
    canvas_draw_str(canvas, 16, 10, tracker_type_short(r->type));
    canvas_set_font(canvas, FontSecondary);
    if(m->demo) canvas_draw_str(canvas, 70, 10, "DEMO");
    if(m->total > 1 && m->index > 0) {
        /* The chevrons are the only thing telling the user Left/Right do
         * anything at all, so they are drawn whenever stepping is possible. */
        snprintf(buf, sizeof(buf), "<%u/%u>", (unsigned)m->index, (unsigned)m->total);
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, buf);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    /* ---- address (baseline 22) ---- */
    snprintf(
        buf,
        sizeof(buf),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        r->mac[0],
        r->mac[1],
        r->mac[2],
        r->mac[3],
        r->mac[4],
        r->mac[5]);
    canvas_draw_str(canvas, 2, 22, buf);

    /* ---- signal (baseline 33): live, peak, and bars ---- */
    snprintf(buf, sizeof(buf), "%d dBm", (int)r->rssi);
    canvas_draw_str(canvas, 2, 33, buf);
    /* rssi_max used to be recorded and never shown. The peak is the useful
     * number: it is how close the thing has ever actually been to you. */
    snprintf(buf, sizeof(buf), "peak %d", (int)r->rssi_max);
    canvas_draw_str(canvas, 46, 33, buf);

    int bars = rssi_bars(r->rssi);
    for(int i = 0; i < 4; i++) {
        int h = 2 + i * 2;
        int bx = 100 + i * 5;
        int by = 33 - h;
        if(i < bars) {
            canvas_draw_box(canvas, bx, by, 4, h);
        } else {
            canvas_draw_frame(canvas, bx, by, 4, h);
        }
    }

    /* ---- dwell + sightings (baseline 44) ---- */
    uint32_t dur_s = (r->last_seen - r->first_seen) / 1000UL;
    uint32_t mins = dur_s / 60UL;
    if(mins > 99UL) {
        snprintf(buf, sizeof(buf), "In range 99m+");
    } else {
        snprintf(buf, sizeof(buf), "In range %u:%02u", (unsigned)mins, (unsigned)(dur_s % 60UL));
    }
    canvas_draw_str(canvas, 2, 44, buf);
    snprintf(buf, sizeof(buf), "x%u", (unsigned)(r->count > 9999 ? 9999 : r->count));
    canvas_draw_str_aligned(canvas, 126, 44, AlignRight, AlignBottom, buf);

    /* ---- verdict band (rows 48..63) ----
     * "TRAVELLING WITH YOU", not "FOLLOWING YOU". Time in range is the only
     * thing that was measured: there is no GPS here and no route matching, so
     * the screen says what it saw rather than what it suspects. */
    if(r->following) {
        canvas_draw_box(canvas, 0, 48, 128, 16);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 3, 50, &I_warning_10px);
        canvas_draw_str(canvas, 16, 60, "TRAVELLING WITH YOU");
        canvas_set_color(canvas, ColorBlack);
    } else if(tracker_type_is_threat(r->type)) {
        canvas_draw_line(canvas, 0, 48, 127, 48);
        canvas_draw_str(canvas, 2, 60, "Tracker - not yet a follower");
    } else {
        canvas_draw_line(canvas, 0, 48, 127, 48);
        canvas_draw_str(canvas, 2, 60, "Not a stalking risk");
    }
}

static bool device_detail_view_input(InputEvent* event, void* context) {
    DeviceDetailView* ddv = context;
    /* Short OR Long - the firmware sends Long, never Short, past the
     * long-press threshold. Back is deliberately left to the scene manager. */
    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;

    if(event->key == InputKeyLeft) {
        if(ddv->prev_cb) ddv->prev_cb(ddv->step_ctx);
        return true;
    }
    if(event->key == InputKeyRight) {
        if(ddv->next_cb) ddv->next_cb(ddv->step_ctx);
        return true;
    }
    return false;
}

DeviceDetailView* device_detail_view_alloc(void) {
    DeviceDetailView* ddv = malloc(sizeof(DeviceDetailView));
    memset(ddv, 0, sizeof(DeviceDetailView));
    ddv->view = view_alloc();
    view_set_context(ddv->view, ddv);
    view_set_draw_callback(ddv->view, device_detail_view_draw);
    view_set_input_callback(ddv->view, device_detail_view_input);
    view_allocate_model(ddv->view, ViewModelTypeLocking, sizeof(DeviceDetailModel));
    return ddv;
}

void device_detail_view_free(DeviceDetailView* ddv) {
    furi_assert(ddv);
    view_free(ddv->view);
    free(ddv);
}

View* device_detail_view_get_view(DeviceDetailView* ddv) {
    furi_assert(ddv);
    return ddv->view;
}

void device_detail_view_set_record(DeviceDetailView* ddv, const TrackerRecord* rec) {
    furi_assert(ddv);
    with_view_model(
        ddv->view,
        DeviceDetailModel * m,
        {
            m->rec = *rec;
            m->valid = true;
        },
        true);
}

void device_detail_view_set_demo(DeviceDetailView* ddv, bool demo) {
    furi_assert(ddv);
    with_view_model(ddv->view, DeviceDetailModel * m, { m->demo = demo; }, true);
}

void device_detail_view_set_position(DeviceDetailView* ddv, size_t index, size_t total) {
    furi_assert(ddv);
    with_view_model(
        ddv->view,
        DeviceDetailModel * m,
        {
            m->index = index;
            m->total = total;
        },
        true);
}

void device_detail_view_set_step_callbacks(
    DeviceDetailView* ddv,
    DeviceDetailViewCallback prev,
    DeviceDetailViewCallback next,
    void* context) {
    furi_assert(ddv);
    ddv->prev_cb = prev;
    ddv->next_cb = next;
    ddv->step_ctx = context;
}
