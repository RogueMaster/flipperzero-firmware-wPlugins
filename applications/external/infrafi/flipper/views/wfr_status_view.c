#include "wfr_status_view.h"
#include "wfr_draw.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <furi.h>
#include "infrafi_icons.h"

#define FRAME_MS 100

typedef enum {
    WfrStatusModeNone,
    WfrStatusModeTransmit,
    WfrStatusModeWaiting,
    WfrStatusModeConnected,
    WfrStatusModeResult,
    WfrStatusModeNfcScan,
    WfrStatusModeAbout,
} WfrStatusMode;

struct WfrStatusView {
    View* view;
    FuriTimer* timer;
    WfrStatusViewCallback callback;
    void* context;
};

typedef struct {
    WfrStatusMode mode;
    uint32_t frame;

    /* transmit */
    uint16_t sent;
    uint16_t total;
    char proto[8];

    /* waiting */
    uint8_t timeout_sec;

    /* connected / result / about */
    const Icon* subject;
    WfrStatusBadge badge;
    char title[24];
    char line1[32];
    char line2[32];
    const char* btn_left;
    const char* btn_right;
} WfrStatusViewModel;

/* Device positions shared by the transmit and waiting scenes */
#define FLIPPER_X 2
#define DEVICE_Y  16
#define SERVER_X  106

static void draw_badge(Canvas* canvas, int32_t cx, int32_t cy, WfrStatusBadge badge) {
    switch(badge) {
    case WfrStatusBadgeCheck:
        wfr_draw_badge(canvas, cx, cy, &I_check_16x12);
        break;
    case WfrStatusBadgeCross:
        wfr_draw_badge(canvas, cx, cy, &I_cross_10x10);
        break;
    case WfrStatusBadgeQuestion:
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, cx, cy, 11);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, cx, cy, 10);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, cx + 1, cy + 4, AlignCenter, AlignBottom, "?");
        canvas_set_color(canvas, ColorBlack);
        break;
    }
}

static void draw_transmit(Canvas* canvas, WfrStatusViewModel* m) {
    static const char bits[] = "1011001110100101";
    const int32_t n_bits = sizeof(bits) - 1;

    wfr_draw_title(canvas, "Beaming credentials");
    canvas_draw_icon(canvas, FLIPPER_X, DEVICE_Y, &I_flipper_32x18);
    canvas_draw_icon(canvas, SERVER_X, DEVICE_Y, &I_server_20x18);

    /* Bits scrolling from the Flipper's IR window to the server */
    canvas_set_font(canvas, FontSecondary);
    int32_t off = (m->frame * 2) % 8;
    char ch[2] = {0, 0};
    int32_t i = 0;
    for(int32_t x = 38 + off; x < SERVER_X - 6; x += 8, i++) {
        int32_t idx = ((i - (int32_t)(m->frame / 4)) % n_bits + n_bits) % n_bits;
        ch[0] = bits[idx];
        canvas_draw_str(canvas, x, 29, ch);
    }

    /* Progress */
    canvas_draw_rframe(canvas, 4, 42, 120, 8, 2);
    if(m->total) {
        int32_t w = 116 * m->sent / m->total;
        if(w > 0) canvas_draw_box(canvas, 6, 44, w, 4);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%u/%u B", m->sent, m->total);
    canvas_draw_str(canvas, 4, 61, m->proto);
    canvas_draw_str_aligned(canvas, 124, 61, AlignRight, AlignBottom, buf);
}

static void draw_waiting(Canvas* canvas, WfrStatusViewModel* m) {
    wfr_draw_title(canvas, "Waiting for server");
    canvas_draw_icon(canvas, FLIPPER_X, DEVICE_Y, &I_flipper_32x18);
    canvas_draw_icon(canvas, SERVER_X, DEVICE_Y, &I_server_20x18);

    /* "((" pulses travelling from the server back to the Flipper */
    const int32_t span = 64;
    for(int32_t k = 0; k < 3; k++) {
        int32_t x = 100 - (int32_t)((m->frame * 3 + k * span / 3) % span);
        canvas_draw_icon(canvas, x, 22, &I_pulse_5x7);
    }

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    static const char* dots[] = {"", ".", "..", "..."};
    snprintf(buf, sizeof(buf), "Listening for ACK%s", dots[(m->frame / 4) % 4]);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, buf);

    uint32_t elapsed = m->frame * FRAME_MS / 1000;
    uint32_t left = (elapsed < m->timeout_sec) ? m->timeout_sec - elapsed : 0;
    snprintf(buf, sizeof(buf), "%lus left", (unsigned long)left);
    canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, buf);
}

static void draw_connected(Canvas* canvas, WfrStatusViewModel* m) {
    const int32_t sx = 6, sy = 26;
    canvas_draw_icon(canvas, sx, sy, &I_server_20x18);
    canvas_draw_icon(canvas, sx + 1, sy - 14, &I_wifi_up_19x14);
    draw_badge(canvas, sx + 24, sy + 20, WfrStatusBadgeCheck);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 46, 20, "Connected!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 46, 32, "Server is online");
    canvas_draw_rframe(canvas, 44, 38, 82, 14, 3);
    canvas_draw_str_aligned(canvas, 85, 48, AlignCenter, AlignBottom, m->line1);
}

static void draw_result(Canvas* canvas, WfrStatusViewModel* m) {
    bool buttons = m->btn_left || m->btn_right;
    const int32_t sx = 6;
    const int32_t sy = buttons ? 16 : 22;
    const Icon* subject = m->subject ? m->subject : &I_server_20x18;

    canvas_draw_icon(canvas, sx, sy, subject);
    draw_badge(canvas, sx + 24, sy + 20, m->badge);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 46, sy, m->title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 46, sy + 12, m->line1);
    canvas_draw_str(canvas, 46, sy + 23, m->line2);

    if(m->btn_left) elements_button_left(canvas, m->btn_left);
    if(m->btn_right) elements_button_right(canvas, m->btn_right);
}

static void draw_nfc_scan(Canvas* canvas, WfrStatusViewModel* m) {
    wfr_draw_title(canvas, "Scan NFC Tag");

    /* Back of the Flipper with its NFC coil */
    canvas_draw_rframe(canvas, 4, 15, 44, 34, 6);
    canvas_draw_rframe(canvas, 5, 16, 42, 32, 5);
    canvas_draw_rframe(canvas, 14, 22, 24, 20, 3);
    canvas_draw_rframe(canvas, 17, 25, 18, 14, 2);
    canvas_draw_rframe(canvas, 20, 28, 12, 8, 1);

    /* Tag bobbing next to it, pulses flowing into the coil */
    static const int8_t bob[] = {0, 1, 2, 1};
    canvas_draw_icon(canvas, 60 + bob[(m->frame / 3) % 4], 24, &I_menu_nfc_20x16);
    canvas_draw_icon(canvas, 56 - (int32_t)((m->frame / 2) % 4) * 2, 29, &I_pulse_5x7);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 84, 26, "Hold tag");
    canvas_draw_str(canvas, 84, 37, "to back of");
    canvas_draw_str(canvas, 84, 48, "Flipper");
    elements_button_left(canvas, "Back");
}

static void draw_about(Canvas* canvas, WfrStatusViewModel* m) {
    canvas_draw_icon(canvas, 4, 4, &I_flipper_32x18);
    canvas_draw_icon(canvas, 38, 7, &I_beam_9x11);
    canvas_draw_icon(canvas, 54, 4, &I_server_20x18);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 125, 12, AlignRight, AlignBottom, m->title);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 36, "InfraFi");
    canvas_draw_line(canvas, 4, 39, 43, 39);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 49, "Wi-Fi over infrared.");
    canvas_draw_str(canvas, 4, 60, "github.com/amd989");
}

static void wfr_status_view_draw(Canvas* canvas, void* _model) {
    WfrStatusViewModel* m = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(m->mode) {
    case WfrStatusModeTransmit:
        draw_transmit(canvas, m);
        break;
    case WfrStatusModeWaiting:
        draw_waiting(canvas, m);
        break;
    case WfrStatusModeConnected:
        draw_connected(canvas, m);
        break;
    case WfrStatusModeResult:
        draw_result(canvas, m);
        break;
    case WfrStatusModeNfcScan:
        draw_nfc_scan(canvas, m);
        break;
    case WfrStatusModeAbout:
        draw_about(canvas, m);
        break;
    default:
        break;
    }
}

static bool wfr_status_view_input(InputEvent* event, void* context) {
    WfrStatusView* status = context;
    if(event->type != InputTypeShort) return false;
    if(event->key != InputKeyLeft && event->key != InputKeyRight && event->key != InputKeyOk) {
        return false;
    }

    WfrStatusMode mode = WfrStatusModeNone;
    with_view_model(status->view, WfrStatusViewModel * m, { mode = m->mode; }, false);
    /* The NFC scan screen only has a Left "Back" hint */
    bool nfc_back = (mode == WfrStatusModeNfcScan && event->key == InputKeyLeft);
    if(mode != WfrStatusModeResult && mode != WfrStatusModeConnected && !nfc_back) return false;

    if(status->callback) status->callback(status->context, event->key);
    return true;
}

static void wfr_status_view_tick(void* context) {
    WfrStatusView* status = context;
    with_view_model(status->view, WfrStatusViewModel * m, { m->frame++; }, true);
}

static void wfr_status_view_enter(void* context) {
    WfrStatusView* status = context;
    furi_timer_start(status->timer, furi_ms_to_ticks(FRAME_MS));
}

static void wfr_status_view_exit(void* context) {
    WfrStatusView* status = context;
    furi_timer_stop(status->timer);
}

WfrStatusView* wfr_status_view_alloc(void) {
    WfrStatusView* status = malloc(sizeof(WfrStatusView));
    status->callback = NULL;
    status->context = NULL;
    status->view = view_alloc();
    view_allocate_model(status->view, ViewModelTypeLocking, sizeof(WfrStatusViewModel));
    view_set_context(status->view, status);
    view_set_draw_callback(status->view, wfr_status_view_draw);
    view_set_input_callback(status->view, wfr_status_view_input);
    view_set_enter_callback(status->view, wfr_status_view_enter);
    view_set_exit_callback(status->view, wfr_status_view_exit);
    status->timer = furi_timer_alloc(wfr_status_view_tick, FuriTimerTypePeriodic, status);
    return status;
}

void wfr_status_view_free(WfrStatusView* status) {
    furi_timer_stop(status->timer);
    furi_timer_free(status->timer);
    view_free(status->view);
    free(status);
}

View* wfr_status_view_get_view(WfrStatusView* status) {
    return status->view;
}

void wfr_status_view_set_callback(
    WfrStatusView* status,
    WfrStatusViewCallback callback,
    void* context) {
    status->callback = callback;
    status->context = context;
}

static void set_mode(WfrStatusViewModel* m, WfrStatusMode mode) {
    if(m->mode != mode) {
        m->mode = mode;
        m->frame = 0;
    }
}

void wfr_status_view_set_transmit(
    WfrStatusView* status,
    uint16_t sent,
    uint16_t total,
    const char* proto) {
    with_view_model(
        status->view,
        WfrStatusViewModel * m,
        {
            set_mode(m, WfrStatusModeTransmit);
            m->sent = sent;
            m->total = total;
            strlcpy(m->proto, proto ? proto : "", sizeof(m->proto));
        },
        true);
}

void wfr_status_view_set_waiting(WfrStatusView* status, uint8_t timeout_sec) {
    with_view_model(
        status->view,
        WfrStatusViewModel * m,
        {
            m->mode = WfrStatusModeWaiting;
            m->frame = 0;
            m->timeout_sec = timeout_sec;
        },
        true);
}

void wfr_status_view_set_connected(WfrStatusView* status, const char* ip) {
    with_view_model(
        status->view,
        WfrStatusViewModel * m,
        {
            set_mode(m, WfrStatusModeConnected);
            strlcpy(m->line1, ip ? ip : "", sizeof(m->line1));
        },
        true);
}

void wfr_status_view_set_result(
    WfrStatusView* status,
    const Icon* subject,
    WfrStatusBadge badge,
    const char* title,
    const char* line1,
    const char* line2,
    const char* btn_left,
    const char* btn_right) {
    with_view_model(
        status->view,
        WfrStatusViewModel * m,
        {
            m->mode = WfrStatusModeResult;
            m->frame = 0;
            m->subject = subject;
            m->badge = badge;
            strlcpy(m->title, title ? title : "", sizeof(m->title));
            strlcpy(m->line1, line1 ? line1 : "", sizeof(m->line1));
            strlcpy(m->line2, line2 ? line2 : "", sizeof(m->line2));
            m->btn_left = btn_left;
            m->btn_right = btn_right;
        },
        true);
}

void wfr_status_view_set_nfc_scan(WfrStatusView* status) {
    with_view_model(
        status->view, WfrStatusViewModel * m, { set_mode(m, WfrStatusModeNfcScan); }, true);
}

void wfr_status_view_set_about(WfrStatusView* status, const char* version) {
    with_view_model(
        status->view,
        WfrStatusViewModel * m,
        {
            set_mode(m, WfrStatusModeAbout);
            snprintf(m->title, sizeof(m->title), "v%s", version);
        },
        true);
}
