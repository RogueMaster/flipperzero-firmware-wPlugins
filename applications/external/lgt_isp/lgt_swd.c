/* ============================================================
 * lgt_swd.c  —  LGT8F328P SWD (GPIO-Bitbang) fuer Flipper Zero
 * Faithful port aus ft2232_lgtisp.c. Die Framing-Sequenzen (Unlock, EEE,
 * Chip-Erase, Page-Program) sind BYTE-fuer-BYTE identisch zum hardware-
 * bestaetigten FTDI-Tool; nur die Pin-Ebene ist furi_hal_gpio.
 *
 * Vorteil ggü. FTDI: der Read liest den SWD-Pin direkt im Sample-Moment
 * (SWC low, vor der steigenden Flanke) — kein Buffering, kein USB-Read,
 * keine RD_DELAY-Phasensuche noetig.
 * ============================================================ */
#include "lgt_swd.h"
#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

/* ---- Pin-Belegung (Flipper GPIO-Header) ----
 *   SWC  = PA7  (Header-Pin 2)
 *   SWD  = PA6  (Header-Pin 3)  bidirektional
 *   RSTN = PA4  (Header-Pin 4)
 *   +3V3 = Pin 9,  GND = Pin 8/11/18
 */
#define PIN_SWC  (&gpio_ext_pa7)
#define PIN_SWD  (&gpio_ext_pa6)
#define PIN_RSTN (&gpio_ext_pa4)

static uint32_t g_delay_us = 2;
void lgt_set_delay_us(uint32_t us) {
    g_delay_us = us;
}
static inline void swd_delay(void) {
    furi_delay_us(g_delay_us);
}

/* ---- Pin-Primitive ---- */
static inline void swc_lo(void) {
    furi_hal_gpio_write(PIN_SWC, false);
}
static inline void swc_hi(void) {
    furi_hal_gpio_write(PIN_SWC, true);
}
static inline void swd_lo(void) {
    furi_hal_gpio_write(PIN_SWD, false);
}
static inline void swd_hi(void) {
    furi_hal_gpio_write(PIN_SWD, true);
}
static inline void rstn_lo(void) {
    furi_hal_gpio_write(PIN_RSTN, false);
}
static inline void rstn_hi(void) {
    furi_hal_gpio_write(PIN_RSTN, true);
}
static inline void swd_out(void) {
    furi_hal_gpio_init(PIN_SWD, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
}
static inline void swd_in(void) {
    furi_hal_gpio_init(PIN_SWD, GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);
}

void lgt_gpio_init(void) {
    furi_hal_gpio_init(PIN_SWC, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(PIN_SWD, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(PIN_RSTN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    swc_hi();
    swd_hi();
    rstn_hi();
}
void lgt_gpio_deinit(void) {
    /* alle auf Input (analog) zuruecksetzen -> Target laeuft, Pins hochohmig */
    furi_hal_gpio_init(PIN_SWC, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(PIN_SWD, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(PIN_RSTN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

/* ---- SWD-Byte schreiben: [start] + 8 Datenbits (LSB-first) + stop ---- */
static void SWD_WriteByte(uint8_t start, uint8_t data, uint8_t stop) {
    int i;
    swd_out();
    if(start) {
        swc_lo();
        swd_delay();
        swd_lo();
        swd_delay();
        swc_hi();
        swd_delay();
    }
    for(i = 0; i < 8; i++) {
        swc_lo();
        if(data & 1)
            swd_hi();
        else
            swd_lo();
        swd_delay();
        data >>= 1;
        swc_hi();
        swd_delay();
    }
    swc_lo();
    if(stop)
        swd_hi();
    else
        swd_lo();
    swd_delay();
    swc_hi();
    swd_delay();
}

/* ---- N Bytes lesen: Start + 8*N Bits + Stop. SWD wird direkt im
 *      Sample-Moment (SWC low) gelesen, LSB-first. ---- */
static void swd_read(uint8_t* out, int n) {
    int b, i;
    for(b = 0; b < n; b++) {
        uint8_t start = (b == 0) ? 1 : 0;
        uint8_t stop = (b == n - 1) ? 1 : 0;
        uint8_t r = 0;
        swd_out();
        if(start) {
            swc_lo();
            swd_delay();
            swd_lo();
            swd_delay();
            swc_hi();
            swd_delay();
        }
        swd_in();
        swd_delay();
        swd_delay();
        for(i = 0; i < 8; i++) {
            swc_lo();
            swd_delay();
            if(furi_hal_gpio_read(PIN_SWD)) r |= (uint8_t)(1u << i); /* Sample bei SWC low */
            swc_hi();
            swd_delay();
        }
        swd_out();
        swc_lo();
        if(stop)
            swd_hi();
        else
            swd_lo();
        swd_delay();
        swc_hi();
        swd_delay();
        out[b] = r;
    }
}

static void SWD_Idle(uint8_t cnt) {
    int i;
    swd_out();
    swd_hi();
    for(i = 0; i < cnt; i++) {
        swc_lo();
        swd_delay();
        swc_hi();
        swd_delay();
    }
}

static void SWD_init(void) {
    swd_out();
    swc_hi();
    swd_hi();
}

static void SWD_ReadSWDID(uint8_t id[4]) {
    SWD_WriteByte(1, 0xAE, 1);
    SWD_Idle(4);
    swd_read(id, 4);
    SWD_Idle(4);
}
/* GUID (4-Byte-Chip-Seriennummer) -- lock-unabhaengig lesbar, kein Unlock noetig. */
static void SWD_ReadGUID(uint8_t g[4]) {
    SWD_Idle(10);
    SWD_WriteByte(1, 0xA8, 1);
    SWD_Idle(4);
    swd_read(g, 4);
    SWD_Idle(4);
}
static bool guid_plausible(const uint8_t g[4]) {
    int z = 1, f = 1;
    for(int i = 0; i < 4; i++) {
        if(g[i] != 0x00) z = 0;
        if(g[i] != 0xFF) f = 0;
    }
    return !(z || f);
}

/* ---- Framing: SWDEN / Unlock / EEE (exakt wie ft2232_lgtisp) ---- */
static void SWD_SWDEN(void) {
    SWD_WriteByte(1, 0xD0, 0);
    SWD_WriteByte(0, 0xAA, 0);
    SWD_WriteByte(0, 0x55, 0);
    SWD_WriteByte(0, 0xAA, 0);
    SWD_WriteByte(0, 0x55, 1);
    SWD_Idle(4);
}
static void SWD_UnLock0(void) {
    SWD_WriteByte(1, 0xF0, 0);
    SWD_WriteByte(0, 0x54, 0);
    SWD_WriteByte(0, 0x51, 0);
    SWD_WriteByte(0, 0x4A, 0);
    SWD_WriteByte(0, 0x4C, 1);
    SWD_Idle(4);
}
static void SWD_UnLock1(void) {
    SWD_WriteByte(1, 0xF0, 0);
    SWD_WriteByte(0, 0x00, 0);
    SWD_WriteByte(0, 0x00, 0);
    SWD_WriteByte(0, 0x00, 0);
    SWD_WriteByte(0, 0x00, 1);
    SWD_Idle(4);
}
static void SWD_UnLock2(void) {
    SWD_WriteByte(1, 0xF0, 0);
    SWD_WriteByte(0, 0x43, 0);
    SWD_WriteByte(0, 0x40, 0);
    SWD_WriteByte(0, 0x59, 0);
    SWD_WriteByte(0, 0x5D, 1);
    SWD_Idle(4);
}

static void SWD_EEE_CSEQ(uint8_t ctrl, uint16_t addr) {
    SWD_WriteByte(1, 0xB2, 0);
    SWD_WriteByte(0, (uint8_t)(addr & 0xFF), 0);
    SWD_WriteByte(0, (uint8_t)(((ctrl & 0x3) << 6) | ((addr >> 8) & 0x3F)), 0);
    SWD_WriteByte(0, (uint8_t)(0xC0 | (ctrl >> 2)), 1);
    SWD_Idle(4);
}
static void SWD_EEE_DSEQ(uint32_t data) {
    SWD_WriteByte(1, 0xB2, 0);
    SWD_WriteByte(0, (uint8_t)(data), 0);
    SWD_WriteByte(0, (uint8_t)(data >> 8), 0);
    SWD_WriteByte(0, (uint8_t)(data >> 16), 0);
    SWD_WriteByte(0, (uint8_t)(data >> 24), 1);
    SWD_Idle(4);
}
/* CRACK: loescht nur die 1. Flash-Seite (1 KB, 0x000-0x3FF), bricht den Leseschutz,
 * der Rest bleibt lesbar. Port aus LGTISP crack() / ftdude. Semi-destruktiv --
 * nur fuer Lese-/Dump-Zugriff (unterscheidet sich von ChipErase durch 0x92/0x9e statt 0x9a). */
static void SWD_Crack(void) {
    SWD_EEE_CSEQ(0x00, 1);
    SWD_EEE_CSEQ(0x98, 1);
    SWD_EEE_CSEQ(0x92, 1);
    furi_delay_ms(200);
    SWD_EEE_CSEQ(0x9e, 1);
    furi_delay_ms(200);
    SWD_EEE_CSEQ(0x8a, 1);
    furi_delay_ms(20);
    SWD_EEE_CSEQ(0x88, 1);
    SWD_EEE_CSEQ(0x00, 1);
}
static void SWD_ChipErase(void) {
    SWD_EEE_CSEQ(0x00, 1);
    SWD_EEE_CSEQ(0x98, 1);
    SWD_EEE_CSEQ(0x9a, 1);
    furi_delay_ms(200);
    SWD_EEE_CSEQ(0x8a, 1);
    furi_delay_ms(20);
    SWD_EEE_CSEQ(0x88, 1);
    SWD_EEE_CSEQ(0x00, 1);
}
static void SWD_EEE_Write(uint32_t data, uint16_t addr) {
    SWD_EEE_DSEQ(data);
    SWD_EEE_CSEQ(0x86, addr);
    SWD_EEE_CSEQ(0xc6, addr);
    SWD_EEE_CSEQ(0x86, addr);
}
static uint32_t SWD_EEE_Read(uint16_t addr) {
    uint8_t d[4];
    SWD_EEE_CSEQ(0xc0, addr);
    SWD_EEE_CSEQ(0xe0, addr);
    SWD_WriteByte(1, 0xaa, 1);
    swd_read(d, 4);
    SWD_Idle(4);
    return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) |
           ((uint32_t)d[3] << 24);
}

static void lgt_write_page(uint32_t byteAddr, const uint8_t* buf, int size) {
    int i;
    uint16_t a = (uint16_t)(byteAddr / 4); /* 4-Byte-Wortadresse */
    SWD_EEE_CSEQ(0x00, a);
    SWD_EEE_CSEQ(0x84, a);
    SWD_EEE_CSEQ(0x86, a);
    for(i = 0; i < size; i += 4) {
        uint32_t w = (uint32_t)buf[i] | ((uint32_t)buf[i + 1] << 8) |
                     ((uint32_t)buf[i + 2] << 16) | ((uint32_t)buf[i + 3] << 24);
        SWD_EEE_Write(w, a);
        a++;
    }
    SWD_EEE_CSEQ(0x82, (uint16_t)(a - 1));
    SWD_EEE_CSEQ(0x80, (uint16_t)(a - 1));
    SWD_EEE_CSEQ(0x00, (uint16_t)(a - 1));
}
static void lgt_read_page(uint32_t byteAddr, uint8_t* buf, int size) {
    int i;
    uint16_t a = (uint16_t)(byteAddr / 4);
    uint32_t data = 0;
    SWD_EEE_CSEQ(0x00, 0x01);
    for(i = 0; i < size; i++) {
        if(i % 4 == 0) {
            data = SWD_EEE_Read(a);
            a++;
        }
        buf[i] = (uint8_t)(data >> (8 * (i % 4)));
    }
    SWD_EEE_CSEQ(0x00, 0x01);
}

/* ---- Unlock/Programmiermodus (exakt wie ft2232_lgtisp) ---- */
static uint8_t SWD_UnLock(uint8_t chip_erase) {
    uint8_t swdid[4], flag[2];
    SWD_ReadSWDID(swdid);
    SWD_SWDEN();
    if(!(swdid[0] == 0x3e || swdid[0] == 0x3f)) return 0;
    if(swdid[0] == 0x3f && !chip_erase) return 1;
    if(swdid[0] == 0x3e) SWD_UnLock0();
    if(chip_erase)
        SWD_ChipErase();
    else
        SWD_Crack(); /* Voll-Erase (Schreiben) vs. Crack (nur 1. KB, Lesen) */
    if(swdid[0] == 0x3e) {
        SWD_UnLock1();
        SWD_WriteByte(1, 0xb1, 0);
        SWD_WriteByte(0, 0x3d, 0);
        SWD_WriteByte(0, 0x60, 0);
        SWD_WriteByte(0, 0x0c, 0);
        SWD_WriteByte(0, 0x00, 0);
        SWD_WriteByte(0, 0x0f, 1);
        SWD_Idle(40);
        SWD_UnLock2();
    }
    SWD_Idle(40);
    SWD_WriteByte(1, 0xb1, 0);
    SWD_WriteByte(0, 0x0c, 0);
    SWD_WriteByte(0, 0x00, 0);
    SWD_WriteByte(0, 0x17, 1);
    SWD_Idle(40);
    SWD_WriteByte(1, 0xa9, 1);
    SWD_Idle(4);
    swd_read(flag, 2);
    SWD_Idle(4);
    if(flag[1] == 0x20) {
        SWD_WriteByte(1, 0xb1, 0);
        SWD_WriteByte(0, 0x3d, 0);
        SWD_WriteByte(0, 0x20, 0);
        SWD_WriteByte(0, 0x0c, 0);
        SWD_WriteByte(0, 0x00, 0);
        SWD_WriteByte(0, 0x0f, 1);
        SWD_Idle(40);
    } else if(flag[1] == 0x60) {
        /* ok (RESET-Pin-Variante) */
    } else
        return 0;
    SWD_WriteByte(1, 0xb1, 0);
    SWD_WriteByte(0, 0x0d, 1);
    SWD_Idle(2);
    return 1;
}
static void SWD_exit(void) {
    SWD_WriteByte(1, 0xb1, 0);
    SWD_WriteByte(0, 0x0d, 1);
    SWD_Idle(2);
    furi_delay_ms(1);
    SWD_WriteByte(1, 0xb1, 0);
    SWD_WriteByte(0, 0x0c, 1);
    SWD_Idle(2);
    SWD_Idle(40);
}
static uint8_t start_pmode(uint8_t chip_erase) {
    uint8_t pm;
    rstn_hi();
    furi_delay_ms(20);
    rstn_lo();
    SWD_init();
    SWD_Idle(80);
    pm = SWD_UnLock(chip_erase);
    if(!pm) pm = SWD_UnLock(chip_erase);
    return pm;
}
static void end_pmode(void) {
    SWD_exit();
    rstn_hi();
    furi_hal_gpio_init(
        PIN_RSTN, GpioModeInput, GpioPullNo, GpioSpeedLow); /* RESET frei -> Target laeuft */
}

/* ============ High-Level ============ */
static int page_is_empty(const uint8_t* img, uint32_t base) {
    for(uint32_t i = 0; i < LGT_PAGE_BYTES; i++)
        if(img[base + i] != 0xFF) return 0;
    return 1;
}

bool lgt_read_id_guid(uint8_t id[4], uint8_t guid[4]) {
    bool ok;
    guid[0] = guid[1] = guid[2] = guid[3] = 0;
    lgt_gpio_init();
    /* Light-pmode: nur Reset + Init, kein Erase */
    rstn_hi();
    furi_delay_ms(20);
    rstn_lo();
    SWD_init();
    SWD_Idle(80);
    SWD_ReadSWDID(id);
    ok = (id[0] == 0x3e || id[0] == 0x3f);
    if(ok) {
        SWD_SWDEN();
        SWD_ReadGUID(guid);
        if(!guid_plausible(guid)) guid[0] = guid[1] = guid[2] = guid[3] = 0;
    }
    end_pmode();
    lgt_gpio_deinit();
    return ok;
}
bool lgt_read_id(uint8_t id[4]) {
    uint8_t guid[4];
    return lgt_read_id_guid(id, guid);
}

/* Flash auslesen (per Crack, opfert 1. KB). buf muss len Bytes fassen. 0=ok, -1=kein LGT. */
int lgt_dump(uint8_t* buf, uint32_t len, LgtProgressCb cb, void* ctx) {
    uint32_t a;
    lgt_gpio_init();
    if(!start_pmode(0)) {
        end_pmode();
        lgt_gpio_deinit();
        return -1;
    } /* Crack: 1. KB geopfert, Rest lesbar */
    for(a = 0; a < len; a += LGT_PAGE_BYTES) {
        uint32_t nn = (len - a >= LGT_PAGE_BYTES) ? LGT_PAGE_BYTES : (len - a);
        lgt_read_page(a, &buf[a], (int)nn);
        if(cb) cb(ctx, a, len, LGT_PHASE_READ);
    }
    end_pmode();
    lgt_gpio_deinit();
    return 0;
}
/* Leseschutz brechen (Crack), SWDID danach zurueckgeben. true = LGT erkannt. */
bool lgt_crack(uint8_t id[4]) {
    bool ok;
    id[0] = id[1] = id[2] = id[3] = 0;
    lgt_gpio_init();
    ok = (start_pmode(0) != 0); /* Reset + Crack-Unlock (1. KB geopfert) */
    if(ok) SWD_ReadSWDID(id); /* SWDID danach -> 0x3F (in-Session entsperrt) */
    end_pmode();
    lgt_gpio_deinit();
    return ok;
}

int lgt_flash(const uint8_t* img, uint32_t img_len, bool verify, LgtProgressCb cb, void* ctx) {
    uint32_t a;
    int bad = 0;
    lgt_gpio_init();
    if(!start_pmode(1)) {
        end_pmode();
        lgt_gpio_deinit();
        return -1;
    }
    for(a = 0; a < img_len; a += LGT_PAGE_BYTES) {
        if(!page_is_empty(img, a)) lgt_write_page(a, &img[a], LGT_PAGE_BYTES);
        if(cb) cb(ctx, a, img_len, LGT_PHASE_WRITE);
    }
    if(verify) {
        for(a = 0; a < img_len; a += LGT_PAGE_BYTES) {
            uint8_t rb[LGT_PAGE_BYTES];
            uint32_t j;
            uint32_t nn = (img_len - a >= LGT_PAGE_BYTES) ? LGT_PAGE_BYTES : (img_len - a);
            if(page_is_empty(img, a)) continue;
            lgt_read_page(a, rb, LGT_PAGE_BYTES);
            for(j = 0; j < nn; j++)
                if(rb[j] != img[a + j]) bad++;
            if(cb) cb(ctx, a, img_len, LGT_PHASE_VERIFY);
        }
    }
    end_pmode();
    lgt_gpio_deinit();
    return bad;
}

int lgt_verify(const uint8_t* img, uint32_t img_len, LgtProgressCb cb, void* ctx) {
    uint32_t a;
    int bad = 0;
    lgt_gpio_init();
    if(!start_pmode(1)) {
        end_pmode();
        lgt_gpio_deinit();
        return -1;
    } /* Unlock loescht -> Verify nur direkt nach Write sinnvoll */
    for(a = 0; a < img_len; a += LGT_PAGE_BYTES) {
        uint8_t rb[LGT_PAGE_BYTES];
        uint32_t j;
        uint32_t nn = (img_len - a >= LGT_PAGE_BYTES) ? LGT_PAGE_BYTES : (img_len - a);
        if(page_is_empty(img, a)) continue;
        lgt_read_page(a, rb, LGT_PAGE_BYTES);
        for(j = 0; j < nn; j++)
            if(rb[j] != img[a + j]) bad++;
        if(cb) cb(ctx, a, img_len, LGT_PHASE_VERIFY);
    }
    end_pmode();
    lgt_gpio_deinit();
    return bad;
}

/* --- persistente pmode-Session fuer STK500/USB --- */
bool lgt_pmode_enter(void) {
    lgt_gpio_init();
    if(!start_pmode(1)) {
        end_pmode();
        lgt_gpio_deinit();
        return false;
    }
    return true;
}
void lgt_pmode_leave(void) {
    end_pmode();
    lgt_gpio_deinit();
}
void lgt_page_write(uint32_t byteAddr, const uint8_t* buf, int size) {
    lgt_write_page(byteAddr, buf, size);
}
void lgt_page_read(uint32_t byteAddr, uint8_t* buf, int size) {
    lgt_read_page(byteAddr, buf, size);
}
