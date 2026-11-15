#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_cdc.h>
#include <gui/gui.h>
#include <input/input.h>
#include <drmosfet_can_icons.h>
#include <stdio.h>
#include <string.h>
#include "mcp_can_2515.h"
#include "gvret_protocol.h"

#define STOP      1U
#define TX_DONE   2U
#define WAKE      4U
#define USB_RESET 8U
#define RING_SIZE 2048U
typedef struct {
    uint32_t tx, rx, errors, bitrate;
    CANFRAME last;
    bool ready, usb, have_frame, last_tx, splash;
} Snapshot;
typedef struct {
    FuriThread* thread;
    FuriMutex* mutex;
    FuriMessageQueue* input;
    FuriMessageQueue* frames;
    Snapshot display, stats;
    MCP2515* can;
    GvretProtocol protocol;
    volatile bool connected, dtr;
    bool inflight;
    uint8_t ring[RING_SIZE];
    size_t head, used;
} Bridge;

static void tx_done(void* ctx) {
    Bridge* b = ctx;
    furi_thread_flags_set(furi_thread_get_id(b->thread), TX_DONE);
}
static void rx_ready(void* ctx) {
    Bridge* b = ctx;
    furi_thread_flags_set(furi_thread_get_id(b->thread), WAKE);
}
static void usb_state(void* ctx, CdcState state) {
    Bridge* b = ctx;
    b->connected = state == CdcStateConnected;
    furi_thread_flags_set(furi_thread_get_id(b->thread), USB_RESET);
}
static void usb_control(void* ctx, CdcCtrlLine line) {
    Bridge* b = ctx;
    bool dtr = (line & CdcCtrlLineDTR) != 0;
    if(dtr != b->dtr) {
        b->dtr = dtr;
        furi_thread_flags_set(furi_thread_get_id(b->thread), USB_RESET);
    }
}
static CdcCallbacks callbacks = {
    .tx_ep_callback = tx_done,
    .rx_ep_callback = rx_ready,
    .state_callback = usb_state,
    .ctrl_line_callback = usb_control};
static void output(void* ctx, const uint8_t* data, size_t size) {
    Bridge* b = ctx;
    if(!b->connected || !b->dtr) return;
    if(size > RING_SIZE - b->used) {
        ++b->stats.errors;
        return;
    }
    for(size_t i = 0; i < size; ++i)
        b->ring[(b->head + b->used + i) % RING_SIZE] = data[i];
    b->used += size;
}
static uint32_t timestamp(void* ctx) {
    UNUSED(ctx);
    return furi_get_tick() * 1000U;
}
static void frame(void* ctx, uint32_t id, bool extended, uint8_t dlc, const uint8_t data[8]) {
    Bridge* b = ctx;
    CANFRAME f = {.canId = id, .ext = extended, .data_lenght = dlc};
    memcpy(f.buffer, data, dlc);
    if(!b->stats.ready || furi_message_queue_put(b->frames, &f, 0) != FuriStatusOk)
        ++b->stats.errors;
}
static bool bitrate(void* ctx, uint32_t speed) {
    Bridge* b = ctx;
    MCP_BITRATE rate;
    switch(speed) {
    case 50000:
        rate = MCP_50KBPS;
        break;
    case 125000:
        rate = MCP_125KBPS;
        break;
    case 250000:
        rate = MCP_250KBPS;
        break;
    case 500000:
        rate = MCP_500KBPS;
        break;
    case 1000000:
        rate = MCP_1000KBPS;
        break;
    default:
        return false;
    }
    furi_message_queue_reset(b->frames);
    deinit_mcp2515(b->can);
    b->can->bitRate = rate;
    b->stats.ready = mcp2515_init(b->can) == ERROR_OK;
    b->stats.bitrate = speed;
    return true;
}
static void publish(Bridge* b) {
    b->stats.usb = b->connected && b->dtr;
    furi_mutex_acquire(b->mutex, FuriWaitForever);
    b->display = b->stats;
    furi_mutex_release(b->mutex);
}
static int32_t worker(void* ctx) {
    Bridge* b = ctx;
    b->can = mcp_alloc(MCP_NORMAL, MCP_16MHZ, MCP_500KBPS);
    b->stats.bitrate = 500000;
    b->stats.ready = mcp2515_init(b->can) == ERROR_OK;
    GvretIo io = {.reply = output, .frame = frame, .bitrate = bitrate, .timestamp = timestamp};
    gvret_init(&b->protocol, &io, b, 500000);
    furi_hal_cdc_set_callbacks(1, &callbacks, b);
    uint32_t retry = furi_get_tick(), refresh = retry;
    while(true) {
        uint32_t flags =
            furi_thread_flags_wait(STOP | TX_DONE | WAKE | USB_RESET, FuriFlagWaitAny, 1);
        if(!(flags & FuriFlagError)) {
            if(flags & STOP) break;
            if(flags & TX_DONE) b->inflight = false;
            if(flags & USB_RESET) {
                b->used = 0;
                b->head = 0;
                b->inflight = false;
                furi_message_queue_reset(b->frames);
                gvret_init(&b->protocol, &io, b, b->stats.bitrate);
            }
        }
        if(!b->connected || !b->dtr) {
            b->used = 0;
            b->head = 0;
            b->inflight = false;
        }
        uint8_t bytes[64];
        // Bound each USB burst so a busy host cannot starve CAN receive.
        for(unsigned burst = 0; burst < 4; ++burst) {
            int32_t n = furi_hal_cdc_receive(1, bytes, sizeof(bytes));
            if(n <= 0) break;
            if(b->connected && b->dtr) {
                uint32_t invalid = b->protocol.invalid_frames;
                for(int32_t i = 0; i < n; ++i)
                    gvret_feed(&b->protocol, bytes[i]);
                b->stats.errors += b->protocol.invalid_frames - invalid;
            }
        }
        uint32_t now = furi_get_tick();
        if(!b->stats.ready && now - retry >= 1000) {
            deinit_mcp2515(b->can);
            b->stats.ready = mcp2515_init(b->can) == ERROR_OK;
            retry = now;
        }
        if(b->stats.ready) {
            CANFRAME f;
            for(unsigned i = 0; i < 16 && read_can_message(b->can, &f) == ERROR_OK; ++i) {
                if(f.req) {
                    ++b->stats.errors;
                    continue;
                }
                ++b->stats.rx;
                b->stats.last = f;
                b->stats.last_tx = false;
                b->stats.have_frame = true;
                uint8_t packet[20];
                size_t n =
                    gvret_encode(packet, timestamp(b), f.canId, f.ext, f.data_lenght, f.buffer);
                if(n)
                    output(b, packet, n);
                else
                    ++b->stats.errors;
            }
            if(furi_message_queue_get(b->frames, &f, 0) == FuriStatusOk) {
                if(send_can_frame(b->can, &f) == ERROR_OK) {
                    ++b->stats.tx;
                    b->stats.last = f;
                    b->stats.last_tx = true;
                    b->stats.have_frame = true;
                } else
                    ++b->stats.errors;
            }
        }
        if(b->connected && b->dtr && !b->inflight && b->used) {
            size_t n = b->used < sizeof(bytes) ? b->used : sizeof(bytes);
            for(size_t i = 0; i < n; ++i)
                bytes[i] = b->ring[(b->head + i) % RING_SIZE];
            b->inflight = true;
            furi_hal_cdc_send(1, bytes, n);
            b->head = (b->head + n) % RING_SIZE;
            b->used -= n;
        }
        if(now - refresh >= 100) {
            publish(b);
            refresh = now;
        }
    }
    furi_hal_cdc_set_callbacks(1, NULL, NULL);
    deinit_mcp2515(b->can);
    free_mcp2515(b->can);
    return 0;
}
static void draw(Canvas* c, void* ctx) {
    Bridge* b = ctx;
    furi_mutex_acquire(b->mutex, FuriWaitForever);
    Snapshot s = b->display;
    furi_mutex_release(b->mutex);
    canvas_clear(c);
    if(s.splash) {
        canvas_draw_icon(c, 3, 4, &I_DrMosfet44x44);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 51, 19, "DR. MOSFET");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 51, 32, "DrMosfetCan");
        canvas_draw_str(c, 51, 44, "USB > CAN");
        canvas_draw_line(c, 0, 52, 127, 52);
        canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "By Dr Mosfet");
        return;
    }
    char text[48];
    canvas_draw_box(c, 0, 0, 128, 12);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 3, 9, "DrMosfetCan");
    snprintf(text, sizeof(text), "%luk", (unsigned long)(s.bitrate / 1000));
    canvas_draw_str_aligned(c, 125, 9, AlignRight, AlignBottom, text);
    canvas_set_color(c, ColorBlack);
    for(unsigned i = 0; i < 2; ++i) {
        unsigned x = i ? 66 : 1;
        canvas_draw_rframe(c, x, 15, 61, 21, 3);
        canvas_draw_str(c, x + 4, 23, i ? "RX" : "TX");
        canvas_set_font(c, FontPrimary);
        snprintf(text, sizeof(text), "%lu", (unsigned long)(i ? s.rx : s.tx));
        canvas_draw_str_aligned(c, x + 56, 33, AlignRight, AlignBottom, text);
        canvas_set_font(c, FontSecondary);
    }
    if(s.have_frame) {
        snprintf(
            text,
            sizeof(text),
            "%s %08lX  %s [%u]",
            s.last_tx ? ">" : "<",
            (unsigned long)s.last.canId,
            s.last.ext ? "EXT" : "STD",
            s.last.data_lenght);
        canvas_draw_str(c, 2, 45, text);
        char* p = text;
        for(unsigned i = 0; i < s.last.data_lenght && i < 8; ++i)
            p += snprintf(p, 4, "%02X ", s.last.buffer[i]);
        if(!s.last.data_lenght) strcpy(text, "(no data)");
        canvas_draw_str(c, 2, 55, text);
    } else
        canvas_draw_str_aligned(c, 64, 50, AlignCenter, AlignBottom, "Waiting for frames...");
    // Leave rows 56-57 clear above the footer's seven-pixel font.
    snprintf(
        text,
        sizeof(text),
        "%s | %s | E:%lu",
        s.ready ? "CAN OK" : "NO MCP",
        s.usb ? "USB OK" : "USB --",
        (unsigned long)(s.errors));
    canvas_draw_str(c, 2, 63, text);
}
static void input(InputEvent* event, void* ctx) {
    Bridge* b = ctx;
    if(event->key == InputKeyBack && event->type == InputTypeShort)
        furi_message_queue_put(b->input, event, 0);
}
int32_t drmosfet_can_app(void* ctx) {
    UNUSED(ctx);
    Bridge* b = malloc(sizeof(Bridge));
    memset(b, 0, sizeof(*b));
    b->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    b->input = furi_message_queue_alloc(2, sizeof(InputEvent));
    b->frames = furi_message_queue_alloc(32, sizeof(CANFRAME));
    b->display.splash = true;
    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw, b);
    view_port_input_callback_set(viewport, input, b);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);
    furi_delay_ms(1500);
    FuriHalUsbInterface* previous = furi_hal_usb_get_config();
    bool switched = previous != &usb_cdc_dual;
    if(switched && !furi_hal_usb_set_config(&usb_cdc_dual, NULL)) {
        gui_remove_view_port(gui, viewport);
        view_port_free(viewport);
        furi_record_close(RECORD_GUI);
        furi_message_queue_free(b->frames);
        furi_message_queue_free(b->input);
        furi_mutex_free(b->mutex);
        free(b);
        return -1;
    }
    b->thread = furi_thread_alloc_ex("DrMosfetCan", 3072, worker, b);
    furi_thread_start(b->thread);
    InputEvent event;
    while(furi_message_queue_get(b->input, &event, 100) != FuriStatusOk)
        view_port_update(viewport);
    furi_thread_flags_set(furi_thread_get_id(b->thread), STOP);
    furi_thread_join(b->thread);
    if(switched) furi_hal_usb_set_config(previous, NULL);
    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    furi_thread_free(b->thread);
    furi_message_queue_free(b->frames);
    furi_message_queue_free(b->input);
    furi_mutex_free(b->mutex);
    free(b);
    return 0;
}
