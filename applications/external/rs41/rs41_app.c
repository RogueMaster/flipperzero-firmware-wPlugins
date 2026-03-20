/*
 * rs41_app.c — Flipper Zero RS41 radiosonde tracker
 *
 * Uses the built-in CC1101 (Sub-GHz radio) to receive and decode
 * Vaisala RS41 radiosondes on 400–406 MHz.
 *
 * Controls:
 *   UP / DOWN  : change receive frequency (±0.1 MHz)
 *   OK         : start / stop reception
 *   BACK       : exit
 *
 * CC1101 configuration:
 *   Modulation  : 2-FSK
 *   Data rate   : 4800 bps  (MDMCFG3/4: DRATE_M=131, DRATE_E=7)
 *   Deviation   : ≈ 2381 Hz  (DEVIATN: E=0, M=4)
 *   BW filter   : ≈ 58 kHz   (CHANBW_E=3, CHANBW_M=3)
 *   Sync word   : 0x086D     (first 2 bytes of the bit-reversed RS41 sync)
 *   Packet mode : infinite length (PKTCTRL0 = 0x02)
 *
 * After sync detection the CC1101 fills its RX FIFO with the remaining
 * 6 sync bytes + 312 payload bytes = 318 bytes total.  The radio thread
 * polls RXBYTES via furi_hal_subghz_rx_pipe_not_empty() / read_packet()
 * and accumulates them.  When 318 bytes are collected the 312-byte payload
 * (offset +6) is passed to rs41_decode().
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rs41_decoder.h"
#include "radio_hw.h"

/* Bit 7 of RXBYTES register: set if RX FIFO has overflowed */
#define CC1101_RXFIFO_OVERFLOW 0x80u

/* ── CC1101 preset ───────────────────────────────────────────────────────── */
/*
 * Register pairs {address, value}, terminated by {0x00, 0x00}.
 * Frequency is set separately via furi_hal_subghz_set_frequency_and_path().
 *
 * Sync word bytes (SYNC1:SYNC0 = 0x08:0x6D) are the CC1101-visible
 * (MSB-first) form of the RS41 on-air sync word:
 *   RS41 air (LSB-first) : 10 B6 CA 11 22 96 12 F8
 *   Bit-reversed         : 08 6D 53 88 44 69 48 1F
 *
 * The CC1101 consumes the matched 2-byte sync; the remaining 6 sync bytes
 * plus 312 payload bytes (318 total) appear in the RX FIFO.
 *
 * Format of CC1101_RS41_PRESET:
 *   {reg, val} pairs terminated by {0x00, 0x00}, followed by 8-byte PA table.
 *   furi_hal_subghz_load_custom_preset() reads reg/val pairs until reg==0x00,
 *   then loads the next 8 bytes as the PA table (confirmed from firmware).
 */
static const uint8_t CC1101_RS41_PRESET[] = {
    0x02, 0x0D,  /* IOCFG0   : GDO0 asserts when sync found, de-asserts at end  */
    0x03, 0x47,  /* FIFOTHR  : RX threshold = 32 bytes                           */
    0x04, 0x08,  /* SYNC1    : sync high byte (bit-reversed RS41)                */
    0x05, 0x6D,  /* SYNC0    : sync low  byte                                     */
    0x07, 0x04,  /* PKTCTRL1 : no address check                                   */
    0x08, 0x02,  /* PKTCTRL0 : infinite packet length, no CRC                     */
    0x0B, 0x06,  /* FSCTRL1  : IF ≈ 152 kHz                                       */
    0x0C, 0x00,  /* FSCTRL0  : no frequency offset                                */
    0x10, 0xF7,  /* MDMCFG4  : BW ≈ 58 kHz (CHANBW_E=3,M=3), DRATE_E=7          */
    0x11, 0x83,  /* MDMCFG3  : DRATE_M=131 → 4800.18 bps @ 26 MHz               */
    0x12, 0x02,  /* MDMCFG2  : 2-FSK, no Manchester, 16/16 sync                  */
    0x13, 0x22,  /* MDMCFG1  : no FEC, 4 preamble bytes                           */
    0x14, 0xF8,  /* MDMCFG0  : channel spacing mantissa                           */
    0x15, 0x04,  /* DEVIATN  : E=0, M=4 → ≈ 2381 Hz deviation                   */
    0x17, 0x30,  /* MCSM1    : stay in RX after packet                            */
    0x18, 0x18,  /* MCSM0    : auto-calibrate on IDLE→RX                          */
    0x19, 0x1D,  /* FOCCFG   : freq-offset compensation                           */
    0x1A, 0x1C,  /* BSCFG    : bit-sync compensation                              */
    0x1B, 0xC7,  /* AGCCTRL2 : max gain settings                                  */
    0x1C, 0x00,  /* AGCCTRL1                                                       */
    0x1D, 0xB2,  /* AGCCTRL0                                                       */
    0x21, 0xB6,  /* FREND1   : front-end RX config                                */
    0x22, 0x10,  /* FREND0   : front-end TX config (PA index 0)                   */
    0x00, 0x00,  /* Terminator (reg==0 stops the loader loop)                     */
    /* 8-byte PA table — required by furi_hal_subghz_load_custom_preset().
     * Only PA[0] matters for RX; 0xC0 = nominal output, rest unused.          */
    0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* ── Constants ───────────────────────────────────────────────────────────── */
#define FRAME_FIFO_LEN   318   /* 6 remaining sync bytes + 312 payload */
#define FREQ_STEP_MHZ    0.1f
#define FREQ_MIN_MHZ   400.1f
#define FREQ_MAX_MHZ   406.0f
#define FREQ_DEFAULT   403.0f
#define RX_TIMEOUT_MS  2500   /* restart if no new FIFO bytes for this long */

#define SCAN_CHANNELS   61    /* 400.0 – 406.0 MHz in 0.1 MHz steps        */
#define SCAN_DWELL_MS   150   /* dwell time per channel during scan          */

#define SIG_HIST_LEN    10   /* number of recent packet attempts to display */

/* ── App view enum ───────────────────────────────────────────────────────── */
typedef enum {
    AppViewDecoder = 0,
    AppViewScanner,
    AppViewHelp,
} AppView;

/* ── App state ───────────────────────────────────────────────────────────── */
typedef struct {
    /* GUI */
    Gui*              gui;
    ViewPort*         viewport;
    FuriMessageQueue* input_queue;
    NotificationApp*  notifications;

    /* Threading */
    FuriMutex*  mutex;
    FuriThread* radio_thread;
    volatile bool radio_running; /* set false to stop the radio thread */

    /* Radio config (written by main thread, read by radio thread) */
    volatile float freq_mhz;
    bool           rx_active;   /* true = RX thread is started */
    volatile bool  scan_mode;   /* true = radio thread runs scanner */
    RadioSource    radio_source; /* RadioInternal or RadioExternal */

    /* Latest decoded sonde data — access under mutex */
    Rs41Frame frame;
    bool      has_data;
    uint32_t  last_frame_tick;

    /* Scanner state — access under mutex */
    int8_t    scan_rssi[SCAN_CHANNELS]; /* last measured RSSI per channel    */
    int       scan_cursor;              /* highlighted channel index          */
    AppView   view;

    /* Signal quality history — access under mutex.
     * Ring buffer of last SIG_HIST_LEN frame decode attempts:
     *   1 = decoded OK,  0 = decode failed / CRC error
     * sig_head points to the next write position (oldest entry is at sig_head). */
    uint8_t sig_hist[SIG_HIST_LEN];
    uint8_t sig_head;
    bool    sig_any;   /* true once at least one entry has been recorded */
} AppCtx;

#define TAG "RS41"

/* ── Radio thread ────────────────────────────────────────────────────────── */

static void radio_start_rx(AppCtx* app) {
    uint32_t freq_hz = (uint32_t)(app->freq_mhz * 1.0e6f);
    radio_hw_init(app->radio_source, CC1101_RS41_PRESET);
    radio_hw_set_frequency(app->radio_source, freq_hz);
    FURI_LOG_I(TAG, "RX start [%s]: %.3f MHz",
               app->radio_source == RadioInternal ? "INT" : "EXT",
               (double)app->freq_mhz);
    radio_hw_rx(app->radio_source);
}

static int32_t radio_thread_fn(void* ctx) {
    AppCtx* app = (AppCtx*)ctx;
    FURI_LOG_I(TAG, "Radio thread started");

    while(app->radio_running) {
        if(app->scan_mode) {
            /* ── Scanner mode ──────────────────────────────────────────── */
            FURI_LOG_I(TAG, "Scanner started [%s]",
                       app->radio_source == RadioInternal ? "INT" : "EXT");
            radio_hw_init(app->radio_source, CC1101_RS41_PRESET);

            while(app->radio_running && app->scan_mode) {
                for(int ch = 0; ch < SCAN_CHANNELS; ch++) {
                    if(!app->radio_running || !app->scan_mode) break;

                    uint32_t freq_hz = (uint32_t)((400.0f + ch * 0.1f) * 1.0e6f);
                    /* Must go IDLE before tuning — firmware furi_check asserts
                     * state==IDLE in furi_hal_subghz_rx().                    */
                    radio_hw_idle(app->radio_source);
                    radio_hw_set_frequency(app->radio_source, freq_hz);
                    radio_hw_rx(app->radio_source);
                    furi_delay_ms(SCAN_DWELL_MS);

                    float rssi_f = radio_hw_get_rssi(app->radio_source);
                    int8_t rssi  = (int8_t)(rssi_f < -128.0f ? -128 :
                                            rssi_f >  127.0f ?  127 : rssi_f);

                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->scan_rssi[ch] = rssi;
                    furi_mutex_release(app->mutex);

                    view_port_update(app->viewport);
                }
            }
            radio_hw_idle(app->radio_source);
            FURI_LOG_I(TAG, "Scanner stopped");

        } else {
            /* ── Decoder mode ──────────────────────────────────────────── */
            uint8_t  fifo_buf[FRAME_FIFO_LEN];
            uint16_t buf_idx      = 0;
            uint32_t last_byte_ms = 0;
            bool     collecting   = false;
            float    last_freq    = 0.0f;
            uint32_t frames_rx    = 0;
            uint32_t frames_ok    = 0;

            radio_start_rx(app);
            last_freq = app->freq_mhz;

            while(app->radio_running && !app->scan_mode) {
                /* Retune if frequency changed */
                float cur_freq = app->freq_mhz;
                if(cur_freq != last_freq) {
                    FURI_LOG_I(TAG, "Freq change: %.3f -> %.3f MHz",
                               (double)last_freq, (double)cur_freq);
                    last_freq  = cur_freq;
                    collecting = false;
                    buf_idx    = 0;
                    radio_start_rx(app);
                }

                uint8_t rxbytes_reg = radio_hw_rxbytes(app->radio_source);

                if(rxbytes_reg & CC1101_RXFIFO_OVERFLOW) {
                    FURI_LOG_W(TAG, "FIFO overflow at buf=%u — restarting RX",
                               (unsigned)buf_idx);
                    collecting = false;
                    buf_idx    = 0;
                    radio_start_rx(app);
                    furi_delay_ms(2);
                    continue;
                }

                uint8_t available = rxbytes_reg & 0x7F;

                if(available == 0) {
                    if(collecting) {
                        if(furi_get_tick() - last_byte_ms > RX_TIMEOUT_MS) {
                            FURI_LOG_W(TAG, "Frame timeout after %u bytes — restarting RX",
                                       (unsigned)buf_idx);
                            collecting = false;
                            buf_idx    = 0;
                            radio_start_rx(app);
                        }
                    }
                    furi_delay_ms(2);
                    continue;
                }

                if(!collecting) {
                    FURI_LOG_I(TAG, "Sync found — collecting frame (%u bytes already in FIFO)",
                               (unsigned)available);
                    collecting = true;
                }
                last_byte_ms = furi_get_tick();

                uint16_t space   = FRAME_FIFO_LEN - buf_idx;
                uint8_t  to_read = available;
                if((uint16_t)to_read > space) to_read = (uint8_t)space;

                radio_hw_read_fifo(app->radio_source, fifo_buf + buf_idx, to_read);
                buf_idx += to_read;

                FURI_LOG_D(TAG, "Read %u FIFO bytes → buf %u/%u",
                           (unsigned)to_read, (unsigned)buf_idx, FRAME_FIFO_LEN);

                if(buf_idx >= FRAME_FIFO_LEN) {
                    frames_rx++;
                    int16_t rssi = (int16_t)radio_hw_get_rssi(app->radio_source);
                    FURI_LOG_I(TAG, "Frame #%lu captured (%u bytes), RSSI=%d dBm",
                               (unsigned long)frames_rx, (unsigned)buf_idx, (int)rssi);

                    FURI_LOG_I(TAG, "Sync bytes: %02X %02X %02X %02X %02X %02X",
                               fifo_buf[0], fifo_buf[1], fifo_buf[2],
                               fifo_buf[3], fifo_buf[4], fifo_buf[5]);

                    const uint8_t* p = fifo_buf + 6;
                    FURI_LOG_I(TAG, "Raw[0..15]: %02X %02X %02X %02X %02X %02X %02X %02X"
                                     " %02X %02X %02X %02X %02X %02X %02X %02X",
                               p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],
                               p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);

                    const uint8_t* q = fifo_buf + 6 + 44;
                    FURI_LOG_I(TAG, "Raw[44..57]: %02X %02X %02X %02X %02X %02X %02X %02X"
                                     " %02X %02X %02X %02X %02X %02X",
                               q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],
                               q[8],q[9],q[10],q[11],q[12],q[13]);

                    Rs41Frame decoded;
                    bool ok = rs41_decode(fifo_buf + 6, rssi, &decoded);
                    if(ok) {
                        frames_ok++;
                        FURI_LOG_I(TAG, "DECODE OK #%lu: id=%.8s lat=%.4f lon=%.4f"
                                        " alt=%.0f sats=%u T=%.1f P=%.1f",
                                   (unsigned long)frames_ok,
                                   decoded.id, (double)decoded.lat, (double)decoded.lon,
                                   (double)decoded.alt, (unsigned)decoded.sats,
                                   (double)decoded.temp, (double)decoded.pres);

                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        app->frame           = decoded;
                        app->has_data        = true;
                        app->last_frame_tick = furi_get_tick();
                        app->sig_hist[app->sig_head] = 1;
                        app->sig_head = (app->sig_head + 1) % SIG_HIST_LEN;
                        app->sig_any  = true;
                        furi_mutex_release(app->mutex);

                        view_port_update(app->viewport);
                        notification_message(app->notifications, &sequence_blink_green_10);
                    } else {
                        FURI_LOG_W(TAG, "Decode FAILED (frame #%lu) id='%.8s' has_gps=%d",
                                   (unsigned long)frames_rx, decoded.id, (int)decoded.has_gps);

                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        app->sig_hist[app->sig_head] = 0;
                        app->sig_head = (app->sig_head + 1) % SIG_HIST_LEN;
                        app->sig_any  = true;
                        furi_mutex_release(app->mutex);
                    }

                    collecting = false;
                    buf_idx    = 0;
                    radio_start_rx(app);
                }
            }

            radio_hw_idle(app->radio_source);
            FURI_LOG_I(TAG, "Decoder stopped. frames_rx=%lu ok=%lu",
                       (unsigned long)frames_rx, (unsigned long)frames_ok);
        }
    }

    radio_hw_idle(app->radio_source);
    radio_hw_sleep(app->radio_source);
    return 0;
}

/* ── Display helpers ─────────────────────────────────────────────────────── */

/* Format a coordinate: "50.0745N" or "014.4189E" */
static void fmt_coord(char* buf, size_t sz, float val, char pos, char neg);

/* ── Help view ───────────────────────────────────────────────────────────── */
/*
 * Shows external CC1101 module pin wiring.
 * Opened by long-pressing LEFT in the decoder view; any key dismisses.
 */
static void draw_help_cb(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "Ext CC1101 wiring");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    canvas_set_font(canvas, FontSecondary);
    /* Two-column table: signal name | Flipper pin */
    static const char* const rows[][2] = {
        {"VCC  ", "Pin 3  (3.3V)"},
        {"GND  ", "Pin 2  (GND) "},
        {"MOSI ", "Pin 12 (PA7) "},
        {"MISO ", "Pin 13 (PA6) "},
        {"SCK  ", "Pin 15 (PB3) "},
        {"CSN  ", "Pin 14 (PA4) "},
        {"GDO0 ", "Pin 7  (PC3) "},
    };
    for(int i = 0; i < 7; i++) {
        int y = 20 + i * 7;
        canvas_draw_str(canvas, 0,  y, rows[i][0]);
        canvas_draw_str(canvas, 30, y, rows[i][1]);
    }

    canvas_draw_line(canvas, 0, 61, 127, 61);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom,
                            "any key: back");
}

/* ── Scanner view ─────────────────────────────────────────────────────────── */
/*
 * Bar graph: 61 bars × 2px = 122px wide, centred in 128px (3px margins).
 * RSSI mapped from [−120, −40] dBm → [0, 36] pixels height.
 * The selected (cursor) channel is drawn inverted (white bar on black box).
 */
static void draw_scanner_cb(Canvas* canvas, AppCtx* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "Scan 400-406 MHz");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    /* Read scan data under mutex */
    int8_t rssi_copy[SCAN_CHANNELS];
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memcpy(rssi_copy, app->scan_rssi, sizeof(rssi_copy));
    int cursor = app->scan_cursor;
    furi_mutex_release(app->mutex);

    /* Bar graph: y=12 (top) to y=47 (bottom baseline), 36 px total height */
    for(int ch = 0; ch < SCAN_CHANNELS; ch++) {
        int val    = (int)rssi_copy[ch] + 120; /* 0 at −120 dBm */
        int height = val * 36 / 80;
        if(height < 0)  height = 0;
        if(height > 36) height = 36;

        int x     = 3 + ch * 2;
        int y_top = 47 - height;

        if(ch == cursor) {
            /* Inverted: fill full column, then draw signal bar in white */
            canvas_draw_box(canvas, x, 12, 2, 36);
            if(height > 0) {
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_box(canvas, x, y_top, 2, height);
                canvas_set_color(canvas, ColorBlack);
            }
        } else {
            if(height > 0)
                canvas_draw_box(canvas, x, y_top, 2, height);
        }
    }

    /* Separator below bar graph */
    canvas_draw_line(canvas, 0, 49, 127, 49);

    /* Info row: selected channel frequency + RSSI */
    canvas_set_font(canvas, FontSecondary);
    char info[32];
    float cursor_mhz = 400.0f + (float)cursor * 0.1f;
    snprintf(info, sizeof(info), "%.1f MHz  %d dBm",
             (double)cursor_mhz, (int)rssi_copy[cursor]);
    canvas_draw_str(canvas, 0, 58, info);

    /* Hint */
    canvas_draw_str(canvas, 0, 64, "LR:move  OK:tune+decode");
}

/* ── Decoder/scanner dispatcher ─────────────────────────────────────────── */
static void fmt_coord(char* buf, size_t sz, float val, char pos, char neg) {
    char sign     = (val >= 0.0f) ? pos : neg;
    float abs_val = fabsf(val);
    int   deg     = (int)abs_val;
    int   frac    = (int)((abs_val - (float)deg) * 10000.0f);
    if(frac > 9999) frac = 9999;
    if(frac < 0)    frac = 0;
    /* Clamp deg so gcc can prove the buffer is large enough */
    if(deg > 180)   deg  = 180;
    snprintf(buf, sz, "%d.%04d%c", deg, frac, sign);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    AppCtx* app = (AppCtx*)ctx;

    if(app->view == AppViewScanner) {
        draw_scanner_cb(canvas, app);
        return;
    }

    if(app->view == AppViewHelp) {
        draw_help_cb(canvas);
        return;
    }

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* ── Header ──────────────────────────────────────────────────────── */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "RS41");

    char freq_str[12];
    snprintf(freq_str, sizeof(freq_str), "%.1f MHz", (double)app->freq_mhz);
    canvas_draw_str(canvas, 38, 8, freq_str);

    if(app->rx_active)
        canvas_draw_str(canvas, 97, 8, "[RX]");

    /* Radio source indicator: small "IN"/"EX" at top right */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom,
                            app->radio_source == RadioInternal ? "IN" : "EX");
    canvas_set_font(canvas, FontPrimary);

    canvas_draw_line(canvas, 0, 10, 127, 10);

    /* ── Content ─────────────────────────────────────────────────────── */
    if(!app->rx_active && !app->has_data) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter,
                                "OK: start RX");
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter,
                                "UP/DN: tune freq");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter,
                                "RIGHT: freq scan");
        canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignCenter,
                                "LEFT: INT/EXT  L-LEFT:?");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter,
                                "BACK: exit");
        return;
    }

    if(app->rx_active && !app->has_data) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter,
                                "Searching...");
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter,
                                "RIGHT: freq scan");
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter,
                                "OK:stop  BACK:exit");
        return;
    }

    /* Has data — show sonde info */
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    Rs41Frame f = app->frame;
    furi_mutex_release(app->mutex);

    canvas_set_font(canvas, FontSecondary);

    /* Row 1: sonde ID + RSSI */
    char row1[32];
    snprintf(row1, sizeof(row1), "ID:%-8s %ddBm", f.id, f.rssi);
    canvas_draw_str(canvas, 0, 20, row1);

    if(f.has_gps) {
        /* Row 2: latitude / longitude */
        char lat_s[20], lon_s[20], row2[44];
        fmt_coord(lat_s, sizeof(lat_s), f.lat, 'N', 'S');
        fmt_coord(lon_s, sizeof(lon_s), f.lon, 'E', 'W');
        snprintf(row2, sizeof(row2), "%-9s %-9s", lat_s, lon_s);
        canvas_draw_str(canvas, 0, 29, row2);

        /* Row 3: altitude + temperature */
        char row3[32];
        if(f.temp > -998.0f)
            snprintf(row3, sizeof(row3), "Alt:%5.0fm T:%+.1fC",
                     (double)f.alt, (double)f.temp);
        else
            snprintf(row3, sizeof(row3), "Alt:%5.0fm  --.-C", (double)f.alt);
        canvas_draw_str(canvas, 0, 38, row3);

        /* Row 4: pressure + satellites */
        char row4[32];
        if(f.pres > 0.0f)
            snprintf(row4, sizeof(row4), "P:%6.1fhPa Sats:%u",
                     (double)f.pres, (unsigned)f.sats);
        else
            snprintf(row4, sizeof(row4), "P:   ---hPa Sats:%u",
                     (unsigned)f.sats);
        canvas_draw_str(canvas, 0, 47, row4);
    } else {
        /* GPS not yet decoded */
        canvas_draw_str(canvas, 0, 29, "No GPS fix yet");
        if(f.temp > -998.0f) {
            char tmp[24];
            snprintf(tmp, sizeof(tmp), "T: %+.1f C", (double)f.temp);
            canvas_draw_str(canvas, 0, 38, tmp);
        }
        if(f.pres > 0.0f) {
            char pstr[24];
            snprintf(pstr, sizeof(pstr), "P: %.1f hPa", (double)f.pres);
            canvas_draw_str(canvas, 0, 47, pstr);
        }
    }

    /* Signal quality history bar — 10 × 4px boxes below data rows */
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool     sig_any  = app->sig_any;
    uint8_t  sig_head = app->sig_head;
    uint8_t  hist_copy[SIG_HIST_LEN];
    memcpy(hist_copy, app->sig_hist, SIG_HIST_LEN);
    furi_mutex_release(app->mutex);

    if(sig_any) {
        canvas_draw_line(canvas, 0, 49, 127, 49);
        /* Left = oldest entry, right = newest entry */
        for(int i = 0; i < SIG_HIST_LEN; i++) {
            /* sig_head is next-write, so sig_head+i wraps to the oldest */
            int idx = (sig_head + i) % SIG_HIST_LEN;
            int x   = i * 5;   /* 4px box + 1px gap */
            if(hist_copy[idx])
                canvas_draw_box(canvas, x, 51, 4, 4);   /* filled = OK */
            else
                canvas_draw_frame(canvas, x, 51, 4, 4); /* outline = fail */
        }
    }

    /* Status hint at bottom */
    canvas_draw_str(canvas, 0, 63, "OK:stop RT:scan UP/DN:freq");
}

/* ── Input callback (called from GUI thread) ─────────────────────────────── */
static void input_cb(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, 0);
}

/* ── App entry point ─────────────────────────────────────────────────────── */
int32_t rs41_app(void* p) {
    UNUSED(p);

    AppCtx* app = malloc(sizeof(AppCtx));
    memset(app, 0, sizeof(AppCtx));

    app->freq_mhz         = FREQ_DEFAULT;
    app->rx_active        = false;
    app->has_data         = false;
    app->last_frame_tick  = 0;
    app->scan_mode        = false;
    app->scan_cursor      = 31; /* start at ~403 MHz */
    app->view             = AppViewDecoder;
    app->radio_source     = RadioInternal;
    app->sig_any          = false;
    app->sig_head         = 0;
    memset(app->sig_hist, 0, sizeof(app->sig_hist));
    for(int i = 0; i < SCAN_CHANNELS; i++) app->scan_rssi[i] = -120;

    /* Mutex for sonde state */
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    /* Input queue */
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    /* ViewPort */
    app->viewport = view_port_alloc();
    view_port_draw_callback_set(app->viewport, draw_cb, app);
    view_port_input_callback_set(app->viewport, input_cb, app->input_queue);

    /* GUI */
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);

    /* Notifications */
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* ── Main event loop ────────────────────────────────────────────── */
    bool running = true;
    while(running) {
        InputEvent event;
        FuriStatus status = furi_message_queue_get(app->input_queue, &event, 100);

        if(status != FuriStatusOk) continue;
        /* Accept short presses and long presses; ignore repeat / release */
        if(event.type != InputTypeShort && event.type != InputTypeLong) continue;

        /* ── Help view: any key dismisses ────────────────────────────── */
        if(app->view == AppViewHelp) {
            app->view = AppViewDecoder;
            view_port_update(app->viewport);
            continue;
        }

        if(app->view == AppViewScanner) {
            /* ── Scanner controls ─────────────────────────────────── */
            switch(event.key) {

            case InputKeyBack:
                running = false;
                break;

            case InputKeyOk:
                /* Tune to cursor frequency and switch to decoder */
                app->freq_mhz = 400.0f + (float)app->scan_cursor * 0.1f;
                app->scan_mode = false;
                app->view = AppViewDecoder;
                view_port_update(app->viewport);
                break;

            case InputKeyUp:
            case InputKeyRight:
                if(app->scan_cursor < SCAN_CHANNELS - 1) app->scan_cursor++;
                view_port_update(app->viewport);
                break;

            case InputKeyDown:
            case InputKeyLeft:
                if(app->scan_cursor > 0) app->scan_cursor--;
                view_port_update(app->viewport);
                break;

            default:
                break;
            }

        } else {
            /* ── Decoder controls ─────────────────────────────────── */
            switch(event.key) {

            case InputKeyBack:
                running = false;
                break;

            case InputKeyOk:
                if(!app->rx_active) {
                    app->scan_mode     = false;
                    app->radio_running = true;
                    app->rx_active     = true;
                    app->radio_thread  = furi_thread_alloc_ex(
                        "RS41Radio", 4096, radio_thread_fn, app);
                    furi_thread_start(app->radio_thread);
                } else {
                    app->radio_running = false;
                    furi_thread_join(app->radio_thread);
                    furi_thread_free(app->radio_thread);
                    app->radio_thread = NULL;
                    app->rx_active    = false;
                    app->scan_mode    = false;
                }
                view_port_update(app->viewport);
                break;

            case InputKeyUp:
                if(app->freq_mhz < FREQ_MAX_MHZ - 0.001f) {
                    app->freq_mhz += FREQ_STEP_MHZ;
                    if(app->freq_mhz > FREQ_MAX_MHZ) app->freq_mhz = FREQ_MAX_MHZ;
                }
                view_port_update(app->viewport);
                break;

            case InputKeyDown:
                if(app->freq_mhz > FREQ_MIN_MHZ + 0.001f) {
                    app->freq_mhz -= FREQ_STEP_MHZ;
                    if(app->freq_mhz < FREQ_MIN_MHZ) app->freq_mhz = FREQ_MIN_MHZ;
                }
                view_port_update(app->viewport);
                break;

            case InputKeyRight:
                /* Switch to frequency scanner; start radio thread if needed */
                for(int i = 0; i < SCAN_CHANNELS; i++) app->scan_rssi[i] = -120;
                app->scan_mode = true;
                app->view      = AppViewScanner;
                if(!app->rx_active) {
                    app->radio_running = true;
                    app->rx_active     = true;
                    app->radio_thread  = furi_thread_alloc_ex(
                        "RS41Radio", 4096, radio_thread_fn, app);
                    furi_thread_start(app->radio_thread);
                }
                view_port_update(app->viewport);
                break;

            case InputKeyLeft:
                if(event.type == InputTypeLong) {
                    /* Long LEFT: show pin wiring help */
                    app->view = AppViewHelp;
                } else if(!app->rx_active) {
                    /* Short LEFT: toggle radio source */
                    app->radio_source = (app->radio_source == RadioInternal)
                                        ? RadioExternal : RadioInternal;
                    FURI_LOG_I(TAG, "Radio source -> %s",
                               app->radio_source == RadioInternal ? "INT" : "EXT");
                }
                view_port_update(app->viewport);
                break;

            default:
                break;
            }
        }
    }

    /* ── Cleanup ─────────────────────────────────────────────────────── */
    if(app->rx_active) {
        app->radio_running = false;
        furi_thread_join(app->radio_thread);
        furi_thread_free(app->radio_thread);
    }

    gui_remove_view_port(app->gui, app->viewport);
    view_port_enabled_set(app->viewport, false);
    view_port_free(app->viewport);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
