#include "wmbus_worker.h"
#include "wmbus_radio.h"
#include "../protocol/wmbus_3of6.h"
#include "../protocol/wmbus_manchester.h"
#include "../protocol/wmbus_crc.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <furi_hal_light.h>
#include <furi_hal_resources.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/preset.h>

/* CC1101 register presets.
 *
 * Two configs are sufficient: one for the 100 kbps band where T1, C1
 * and combined T+C all share the chip-level sync word 0x543D, and one
 * for the 32.768 kbps Manchester S-mode at 868.3 MHz. Values derived
 * from TI SWRA522 cross-checked against rtl-wmbus / wmbusmeters.
 *
 * Infinite-length RX is used so the slicer can run in software: our
 * 3-of-6 decoder handles T1, while C1 frames carry 0xCD (Format A) or
 * 0x3D (Format B) right after sync, telling us which CRC verifier
 * to apply. */
static const uint8_t k_preset_ct[] = {
    0x06, 0x00,                 /* PKTLEN  = 0    (infinite, slicer handles)   */
    0x07, 0x00,                 /* PKTCTRL1: no addr check                     */
    0x08, 0x02,                 /* PKTCTRL0: infinite, no whitening            */
    0x0B, 0x06, 0x0C, 0x00,     /* FSCTRL                                       */
    0x10, 0x5B, 0x11, 0xF8,     /* MDMCFG4/3 — 99.97 kBaud                     */
    0x12, 0x06,                 /* MDMCFG2: 2-FSK, 16/16 sync, no Manchester   */
    0x13, 0x22, 0x14, 0xF8,     /* MDMCFG1/0                                    */
    0x15, 0x44,                 /* DEVIATN: ±50 kHz                            */
    0x17, 0x00,                 /* MCSM1: stay in RX after packet              */
    0x18, 0x18,                 /* MCSM0: PO_TIMEOUT, auto-cal idle->rx        */
    0x19, 0x16, 0x1A, 0x6C,     /* FOCCFG / BSCFG                              */
    0x1B, 0x43, 0x1C, 0x40, 0x1D, 0x91,
    0x21, 0x56, 0x22, 0x10,
    0x23, 0xE9, 0x24, 0x2A, 0x25, 0x00, 0x26, 0x1F,
    0x2C, 0x81, 0x2D, 0x35, 0x2E, 0x09,
    0x04, 0x54, 0x05, 0x3D,     /* SYNC1:SYNC0 = 0x543D                        */
    0x00, 0x00,
    0xC0, 0,0,0,0,0,0,0,
};

/* S1 — Manchester encoded at 32.768 kbit data (~65.536 kchip/s). */
static const uint8_t k_preset_s1[] = {
    0x06, 0x00, 0x07, 0x00, 0x08, 0x02,
    0x0B, 0x06, 0x0C, 0x00,
    0x10, 0x68, 0x11, 0x4A,
    0x12, 0x06, 0x13, 0x22, 0x14, 0xF8,
    0x15, 0x50,
    0x17, 0x00, 0x18, 0x18,
    0x19, 0x16, 0x1A, 0x6C,
    0x1B, 0x43, 0x1C, 0x40, 0x1D, 0x91,
    0x21, 0x56, 0x22, 0x10,
    0x23, 0xE9, 0x24, 0x2A, 0x25, 0x00, 0x26, 0x1F,
    0x2C, 0x81, 0x2D, 0x35, 0x2E, 0x09,
    0x04, 0x76, 0x05, 0x96,
    0x00, 0x00,
    0xC0, 0,0,0,0,0,0,0,
};

/* RX-FIFO drain hook. Weak default for host tests; wmbus_hal_rx.c
 * overrides on-device. `external` picks the SPI handle. */
__attribute__((weak)) size_t wmbus_hal_rx_drain(uint8_t* dst, size_t cap, bool external) {
    (void)dst; (void)cap; (void)external;
    return 0;
}

struct WmbusWorker {
    WmbusApp*       app;
    FuriThread*     thread;
    volatile bool   running;
    WmbusMode       mode;

    uint8_t         chip_buf[1024];   /* generous: max frame ~290 B + 3of6 expansion */
    uint8_t         data_buf[WMBUS_MAX_FRAME];
    uint8_t         payload_buf[WMBUS_MAX_FRAME];

    WmbusWorkerStats stats;
};

void wmbus_worker_get_stats(const WmbusWorker* w, WmbusWorkerStats* out) {
    if(!w || !out) return;
    *out = w->stats;
}

static int32_t worker_thread(void* ctx);

static const uint8_t* preset_for(WmbusMode m) {
    return (m == WmbusModeS1) ? k_preset_s1 : k_preset_ct;
}

static uint32_t freq_for_mode(WmbusMode m) {
    return (m == WmbusModeS1) ? 868300000 : 868950000;
}

WmbusWorker* wmbus_worker_alloc(WmbusApp* app) {
    WmbusWorker* w = malloc(sizeof(WmbusWorker));
    memset(w, 0, sizeof(*w));
    w->app = app;
    w->thread = furi_thread_alloc_ex("WmbusWorker", 3072, worker_thread, w);
    furi_thread_set_priority(w->thread, FuriThreadPriorityLow);
    return w;
}

void wmbus_worker_free(WmbusWorker* w) {
    if(!w) return;
    wmbus_worker_stop(w);
    furi_thread_free(w->thread);
    free(w);
}

void wmbus_worker_start(WmbusWorker* w, WmbusMode mode) {
    if(w->running) return;
    w->mode = mode;
    w->running = true;
    furi_thread_start(w->thread);
}

void wmbus_worker_stop(WmbusWorker* w) {
    if(!w->running) return;
    w->running = false;
    furi_thread_join(w->thread);
}

bool wmbus_worker_is_running(const WmbusWorker* w) { return w->running; }

/* Frame slicer.
 *
 * T1 / C1 / CT share the same CC1101 config; only the post-sync handling
 * differs:
 *   T1: chip stream is 3-of-6 encoded, decode and slice on L.
 *   C1: byte[0] = 0xCD (Format A) or 0x3D (Format B); drop it, then
 *       the rest is plain NRZ starting at the L byte.
 *   CT: peek byte[0]; route to C1 path on 0xCD/0x3D, else T1 path.
 * S1 is independent: Manchester-decoded NRZ then Format A. */
typedef struct {
    size_t  chip_bits;
    size_t  needed_bytes;
    bool    have_L;
    bool    routed;                 /* mode decision finalised for this frame */
    uint8_t route;                  /* 0 = T1/3of6, 1 = Format A NRZ, 2 = Format B NRZ */
    int8_t  rssi;
} Slicer;

static void emit_format_a(WmbusWorker* w, const uint8_t* decoded, size_t len, int8_t rssi) {
    size_t out_len;
    if(!wmbus_crc_verify_format_a(decoded, len, w->payload_buf, sizeof(w->payload_buf), &out_len)) {
        w->stats.crc_fails++;
        return;
    }
    w->stats.decoded_a++;
    wmbus_app_on_telegram(w->app, w->payload_buf, out_len, rssi);
}

static void emit_format_b(WmbusWorker* w, const uint8_t* frame, size_t len, int8_t rssi) {
    if(!wmbus_crc_verify_format_b(frame, len)) {
        w->stats.crc_fails++;
        return;
    }
    w->stats.decoded_b++;
    wmbus_app_on_telegram(w->app, frame, len - 2, rssi);
}

static void slicer_route(WmbusWorker* w, Slicer* s) {
    if(s->chip_bits < 8 || s->routed) return;
    uint8_t b0 = w->chip_buf[0];
    if(w->mode == WmbusModeT1) {
        s->route = 0;
    } else if(w->mode == WmbusModeC1) {
        if(b0 == 0xCD)      s->route = 1;
        else if(b0 == 0x3D) s->route = 2;
        else { s->chip_bits = 0; return; }
    } else {
        if(b0 == 0xCD)      s->route = 1;
        else if(b0 == 0x3D) s->route = 2;
        else                s->route = 0;
    }
    s->routed = true;
}

static void slicer_run_nrz(WmbusWorker* w, Slicer* s, bool format_b) {
    /* byte[0] is the C-mode postamble (0xCD/0x3D), byte[1] is L. */
    size_t n = s->chip_bits / 8;
    if(n < 2) return;
    const uint8_t* buf = w->chip_buf + 1;
    size_t avail = n - 1;

    if(!s->have_L) {
        size_t L = (size_t)buf[0];
        s->needed_bytes = L + 1;
        if(!format_b) {
            /* Format A has per-block CRCs; B's CRCs are already in L. */
            size_t ov = 2;
            if(L > 9) ov += ((L - 9 + 15) / 16) * 2;
            s->needed_bytes += ov;
        }
        s->have_L = true;
    }

    if(avail >= s->needed_bytes) {
        if(format_b) emit_format_b(w, buf, s->needed_bytes, s->rssi);
        else         emit_format_a(w, buf, s->needed_bytes, s->rssi);
        s->chip_bits = 0; s->have_L = false; s->routed = false;
    }
}

static void slicer_run_3of6(WmbusWorker* w, Slicer* s) {
    bool err = false;
    size_t n = wmbus_3of6_decode(
        w->chip_buf, s->chip_bits, w->data_buf, sizeof(w->data_buf), &err);
    if(err) w->stats.three_of_six_err++;
    if(n < 1) return;
    if(!s->have_L) {
        size_t L = (size_t)w->data_buf[0];
        s->needed_bytes = L + 1;
        size_t ov = 2;
        if(L > 9) { size_t rest = L - 9; ov += ((rest + 15) / 16) * 2; }
        s->needed_bytes += ov;
        s->have_L = true;
    }
    if(n >= s->needed_bytes) {
        emit_format_a(w, w->data_buf, s->needed_bytes, s->rssi);
        s->chip_bits = 0; s->have_L = false; s->routed = false;
    }
}

static void slicer_run(WmbusWorker* w, Slicer* s) {
    if(w->mode == WmbusModeS1) {
        size_t bits = wmbus_manchester_decode(
            w->chip_buf, s->chip_bits, WmbusManchesterIeee802,
            w->data_buf, sizeof(w->data_buf) * 8, NULL);
        size_t n = bits / 8;
        if(n < 1) return;
        if(!s->have_L) { s->needed_bytes = (size_t)w->data_buf[0] + 1; s->have_L = true; }
        if(n >= s->needed_bytes) {
            emit_format_a(w, w->data_buf, s->needed_bytes, s->rssi);
            s->chip_bits = 0; s->have_L = false;
        }
        return;
    }

    bool was_routed = s->routed;
    slicer_route(w, s);
    if(!was_routed && s->routed) w->stats.sync_locks++;
    if(!s->routed) return;
    switch(s->route) {
        case 0: slicer_run_3of6(w, s);            break;
        case 1: slicer_run_nrz(w, s, false);      break;
        case 2: slicer_run_nrz(w, s, true);       break;
    }
}

static int32_t worker_thread(void* ctx) {
    WmbusWorker* w = (WmbusWorker*)ctx;
    Slicer s; memset(&s, 0, sizeof(s));

    /* Internal radio uses furi_hal_subghz_* directly; the plugin
     * abstraction (subghz_devices_*) is only used for the external
     * module, which owns its own SPI / GDO0 / power. */
    const SubGhzDevice* dev = NULL;
    bool is_external = false;

    if(w->app->settings.module == WmbusModuleExternal_) {
        dev = wmbus_radio_select(NULL, WmbusModuleExternal);
        is_external = dev && wmbus_radio_is_external(dev);
    }

    uint32_t f = w->app->settings.freq_hz;
    if(f < 868000000 || f > 869500000) f = freq_for_mode(w->mode);

    if(is_external) {
        subghz_devices_reset(dev);
        subghz_devices_idle(dev);
        subghz_devices_load_preset(
            dev, FuriHalSubGhzPresetCustom, (uint8_t*)preset_for(w->mode));
        subghz_devices_set_frequency(dev, f);
        subghz_devices_flush_rx(dev);
        subghz_devices_set_rx(dev);
    } else {
        if(dev) { wmbus_radio_release(dev); dev = NULL; }
        furi_hal_subghz_reset();
        furi_hal_subghz_load_custom_preset((uint8_t*)preset_for(w->mode));
        furi_hal_subghz_set_frequency_and_path(f);
        furi_hal_subghz_flush_rx();
        furi_hal_subghz_rx();
    }

    uint32_t idle_ticks = 0;
    while(w->running) {
        furi_delay_tick(1);

        s.rssi = (int8_t)(is_external ? subghz_devices_get_rssi(dev)
                                       : furi_hal_subghz_get_rssi());

        uint8_t buf[64];
        size_t got = wmbus_hal_rx_drain(buf, sizeof(buf), is_external);

        if(got > 0) {
            idle_ticks = 0;
            size_t free_bits = (sizeof(w->chip_buf) * 8) - s.chip_bits;
            size_t fits = got * 8 <= free_bits ? got : free_bits / 8;
            if(fits) {
                memcpy(&w->chip_buf[s.chip_bits / 8], buf, fits);
                s.chip_bits += fits * 8;
            } else {
                s.chip_bits = 0; s.have_L = false; s.needed_bytes = 0; s.routed = false;
            }
            slicer_run(w, &s);
        } else {
            furi_delay_ms(2);
            if(s.chip_bits > 0) {
                idle_ticks++;
                if(idle_ticks > 20) {
                    s.chip_bits = 0; s.have_L = false; s.needed_bytes = 0; s.routed = false;
                    idle_ticks = 0;
                }
            }
        }
    }

    if(is_external) {
        subghz_devices_idle(dev);
        subghz_devices_sleep(dev);
        wmbus_radio_release(dev);
    } else {
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    }
    return 0;
}

void wmbus_worker_inject(WmbusWorker* w, const uint8_t* raw, size_t len, int8_t rssi) {
    if(len > sizeof(w->payload_buf)) return;
    size_t out_len;
    if(wmbus_crc_verify_format_a(raw, len, w->payload_buf, sizeof(w->payload_buf), &out_len))
        wmbus_app_on_telegram(w->app, w->payload_buf, out_len, rssi);
}
