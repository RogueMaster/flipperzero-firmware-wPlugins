#include "z5r.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_cdc.h>

/*
 * IronLogic Z-2 "Z2 PROTO" (binary, 38400 8N1), recovered from BaseZ5R 1.1.0 CAdapterThread:
 *   PC -> adapter : 00                       sync, adapter answers AA
 *                   cmd [data] cs            cs: sum(cmd..cs) + 0x5A == 0; every frame is ACKed with AA
 *     11 / 12 / 1A                           init / reader-mode select
 *     19 lo hi                               read 32 bytes of controller memory at addr
 *     18 lo hi d[32]                         write 32 bytes of controller memory
 *     13, 15 lo hi, 14 lo hi d[32]           DS1996L touch / read / write (not emulated)
 *   adapter -> PC: CC 40 d[32] cs            page data
 *                  CC 00                     write ok
 *                  F0                        controller not found
 *                  CC F1                     error (BaseZ5R retries)
 */
/* Channel 0 stays with the CLI, like the stock USB-UART bridge. A single non-composite CDC was tried (2026-11-19): ZReader
 * then re-detects the adapter after replug, but BaseZ5R's adapter thread still sticks to the dead handle, so no gain. */
#define CDC_IF 1
#define FLAG_RX (1 << 0)
#define FLAG_STOP (1 << 1)

/*
 * Windows BaseZ5R (ZReader.dll) only lists USB devices from its table:
 * FTDI 0403:1234/1237/6001 (D2XX driver) or IronLogic CDC 3168:0100/0102/0106,
 * and picks the adapter type by the USB product string. We present a plain CDC ACM
 * port (no driver needed on Win10+) with IronLogic IDs and the Z-2 EHR name.
 */
#define Z2_VID 0x3168
#define Z2_PID 0x0106 /* shortcut: PID->model mapping not decoded, all three share one table */
static const char z2_product[] = "USB IronLogic Adapter Z-2 EHR";
static struct usb_device_descriptor z2_dev_desc;
static FuriHalUsbInterface z2_usb;

static void z2_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    usb_cdc_dual.init(dev, intf, ctx);
    free(intf->str_prod_descr); /* cdc_deinit frees whatever is here, so keep it heap-allocated */
    size_t len = strlen(z2_product);
    struct usb_string_descriptor* d = malloc(len * 2 + 2);
    d->bLength = len * 2 + 2;
    d->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < len; i++) d->wString[i] = z2_product[i];
    intf->str_prod_descr = d;
}

/* BaseZ5R does not survive the adapter going away (dead handle, "Read/Write error 22" flood, then ~300 ms per command
 * even after the port is re-selected) and it gives up polling for good after one unanswered "00". So "stopping" the
 * emulation only switches the data off (reads answer F0 "no controller", writes fail) while the Z-2 USB device stays
 * attached and keeps ACKing. The stock USB config and the thread go away when the app exits (z2emu_usb_release). */
static FuriHalUsbInterface* usb_saved;
static bool usb_held;
static Z2Emu* inst;

struct Z2Emu {
    FuriThread* thread;
    FuriSemaphore* tx_done;
    Z2Backend backend;
    Z5rDb* db;
    volatile uint32_t pages;
    volatile bool dirty;
    volatile bool active;
};

static void cdc_tx(void* ctx) {
    furi_semaphore_release(((Z2Emu*)ctx)->tx_done);
}
static void cdc_rx(void* ctx) {
    furi_thread_flags_set(furi_thread_get_id(((Z2Emu*)ctx)->thread), FLAG_RX);
}
static CdcCallbacks cdc_cb = {cdc_tx, cdc_rx, NULL, NULL, NULL};

static void send(Z2Emu* e, uint8_t* d, size_t n) {
    furi_semaphore_acquire(e->tx_done, 100);
    furi_hal_cdc_send(CDC_IF, d, n);
}

static size_t frame_len(uint8_t cmd) {
    switch(cmd) {
    case 0x00:
    case 0x57: /* 'W': stop Dallas emulation */
        return 1;
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x1A:
        return 2;
    case 0x15:
    case 0x19:
        return 4;
    case 0x14:
    case 0x18:
        return 36;
    case 0x44: /* 'D' + 8-byte ROM: emulate key to a controller, ignored */
        return 9;
    default:
        return 0;
    }
}

static bool page_read(Z2Emu* e, uint16_t addr, uint8_t* out) {
    if(e->backend == Z2BackendController) return z5r_link_read(addr, out, Z5R_PAGE);
    for(int i = 0; i < Z5R_PAGE; i++) out[i] = e->db->mem[(addr + i) % e->db->mem_size]; /* EEPROM-like aliasing */
    return true;
}

static bool page_write(Z2Emu* e, uint16_t addr, const uint8_t* d) {
    if(e->backend == Z2BackendController) return z5r_link_write_page(addr, d);
    for(int i = 0; i < Z5R_PAGE; i++) e->db->mem[(addr + i) % e->db->mem_size] = d[i];
    e->dirty = true;
    return true;
}

static void handle(Z2Emu* e, const uint8_t* f) {
    uint8_t out[40];
    size_t n = 0;
    out[n++] = 0xAA;
    uint16_t addr = f[1] | (f[2] << 8);
    switch(f[0]) {
    case 0x19:
        out[n++] = 0xCC;
        out[n++] = 0x40;
        if(e->active && page_read(e, addr, out + n)) {
            n += Z5R_PAGE;
            out[n] = z2_cs(out + 1, n - 1);
            n++;
            e->pages++;
        } else {
            n = 1;
            out[n++] = 0xF0;
        }
        break;
    case 0x18:
        out[n++] = 0xCC;
        out[n++] = e->active && page_write(e, addr, f + 3) ? 0x00 : 0xF1;
        e->pages++;
        break;
    default: /* 11/12/1A ack only; 13/14/15 (DS1996L) ack and never answer -> BaseZ5R times out */
        break;
    }
    send(e, out, n);
}

static int32_t emu_thread(void* ctx) {
    Z2Emu* e = ctx;
    uint8_t buf[128];
    size_t len = 0;
    uint32_t last_rx = furi_get_tick();
    while(true) {
        uint32_t fl = furi_thread_flags_wait(FLAG_RX | FLAG_STOP, FuriFlagWaitAny, 100);
        if(!(fl & FuriFlagError) && (fl & FLAG_STOP)) break;
        if(!(fl & FuriFlagError) && (fl & FLAG_RX)) {
            int32_t r;
            while(len < sizeof(buf) - CDC_DATA_SZ &&
                  (r = furi_hal_cdc_receive(CDC_IF, buf + len, CDC_DATA_SZ)) > 0) {
                len += r;
            }
            last_rx = furi_get_tick();
        } else if(len && furi_get_tick() - last_rx > 500) {
            len = 0; /* drop a stale partial frame */
        }
        while(len) {
            if(buf[0] == 'i') { /* ZReader.dll identifies an adapter it has no table entry for with "i\r\n" (9600/115200) */
                if(len < 3) break;
                if(buf[1] == '\r' && buf[2] == '\n') {
                    /* parsed by FUN_180017650: line 1 needs "S/N: <dec> [<hex model>]" (0x302 = Z-2 EHR), a later chunk "Software version:" */
                    static uint8_t id1[] = "S/N: 4242 [302]\r\n";
                    static uint8_t id2[] = "Software version: 2.15.0.0\r\nBuild jan 1 2024 0:0:0\r\n";
                    send(e, id1, sizeof(id1) - 1);
                    send(e, id2, sizeof(id2) - 1);
                    memmove(buf, buf + 3, len -= 3);
                    continue;
                }
            }
            size_t need = frame_len(buf[0]);
            if(!need) {
                memmove(buf, buf + 1, --len);
                continue;
            }
            if(len < need) break;
            if(buf[0] == 0x00 || buf[0] == 0x57 || buf[0] == 0x44) { /* sync / Dallas emulation stop / start: ACK only */
                uint8_t aa = 0xAA;
                send(e, &aa, 1);
            } else if(need > 1 && buf[0] != 0x44 && z2_cs(buf, need - 1) == buf[need - 1]) {
                handle(e, buf);
            }
            memmove(buf, buf + need, len -= need);
        }
    }
    return 0;
}

Z2Emu* z2emu_start(Z2Backend backend, Z5rDb* db) {
    Z2Emu* e = inst;
    if(e) { /* the thread and the USB device are still up from the previous run */
        e->backend = backend;
        e->db = db;
        e->pages = 0;
        e->dirty = false;
        e->active = true;
        return e;
    }
    e = malloc(sizeof(Z2Emu));
    memset(e, 0, sizeof(*e));
    e->backend = backend;
    e->db = db;
    e->active = true;
    e->tx_done = furi_semaphore_alloc(1, 1);
    e->thread = furi_thread_alloc_ex("Z2Emu", 3072, emu_thread, e);
    z2_dev_desc = *usb_cdc_dual.dev_descr;
    z2_dev_desc.idVendor = Z2_VID;
    z2_dev_desc.idProduct = Z2_PID;
    z2_usb = usb_cdc_dual;
    z2_usb.init = z2_usb_init;
    z2_usb.dev_descr = &z2_dev_desc;
    usb_saved = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&z2_usb, NULL));
    usb_held = true;
    furi_thread_start(e->thread);
    furi_hal_cdc_set_callbacks(CDC_IF, &cdc_cb, e);
    inst = e;
    return e;
}

void z2emu_stop(Z2Emu* e) {
    e->active = false;
    furi_delay_ms(150); /* shortcut: lets a frame in flight finish before the app touches the image, no lock */
}

void z2emu_usb_release(void) {
    Z2Emu* e = inst;
    if(e) {
        furi_hal_cdc_set_callbacks(CDC_IF, NULL, NULL);
        furi_thread_flags_set(furi_thread_get_id(e->thread), FLAG_STOP);
        furi_thread_join(e->thread);
        furi_thread_free(e->thread);
        furi_semaphore_free(e->tx_done);
        free(e);
        inst = NULL;
    }
    if(usb_held) furi_hal_usb_set_config(usb_saved, NULL);
    usb_held = false;
}

uint32_t z2emu_pages(Z2Emu* e) {
    return e->pages;
}
bool z2emu_dirty(Z2Emu* e) {
    return e->dirty;
}
