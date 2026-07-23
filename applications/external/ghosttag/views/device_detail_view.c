#include "device_detail_view.h"
#include "ghosttag_icons.h"
#include <furi.h>
#include <stdio.h>

struct DeviceDetailView {
    View* view;
};

typedef struct {
    TrackerRecord rec;
    bool valid;
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
    if(!m->valid) return;
    TrackerRecord* r = &m->rec;

    // header
    canvas_draw_icon(canvas, 2, 1, type_icon(r->type));
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 16, 10, tracker_type_name(r->type));
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    // MAC
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
    canvas_draw_str(canvas, 2, 23, buf);

    // RSSI + bars
    snprintf(buf, sizeof(buf), "RSSI %d dBm", r->rssi);
    canvas_draw_str(canvas, 2, 34, buf);
    int bars = rssi_bars(r->rssi);
    for(int i = 0; i < 4; i++) {
        int h = 2 + i * 2;
        int bx = 92 + i * 5;
        int by = 33 - h;
        if(i < bars)
            canvas_draw_box(canvas, bx, by, 4, h);
        else
            canvas_draw_frame(canvas, bx, by, 4, h);
    }

    // tracked duration + count
    uint32_t dur_s = (r->last_seen - r->first_seen) / 1000;
    snprintf(
        buf,
        sizeof(buf),
        "Tracked %02lu:%02lu  x%u",
        (unsigned long)(dur_s / 60),
        (unsigned long)(dur_s % 60),
        (unsigned)r->count);
    canvas_draw_str(canvas, 2, 45, buf);

    // status footer
    if(r->following) {
        canvas_draw_box(canvas, 0, 52, 128, 12);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 3, 53, &I_warning_10px);
        canvas_draw_str(canvas, 16, 62, "FOLLOWING YOU");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 2, 62, "Monitoring - keep moving");
    }
}

static bool device_detail_view_input(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    return false; // let the scene manager handle Back
}

DeviceDetailView* device_detail_view_alloc(void) {
    DeviceDetailView* ddv = malloc(sizeof(DeviceDetailView));
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
