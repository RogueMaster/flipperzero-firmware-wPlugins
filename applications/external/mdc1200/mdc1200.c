/*
 * MDC1200 v0.2.2beta - Flipper Zero (Momentum dev)
 * Created by Marcopiiit
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <lib/toolbox/level_duration.h>
#include <stdio.h>
#include <string.h>

#define FEC_K 7
#define MDC_LEAD 12
#define MDC_PRE 3
#define MDC_TAIL 2
#define MDC_TX_MAX (MDC_LEAD + MDC_PRE + 5 + FEC_K * 4 + MDC_TAIL)
#define RSSI_CARRIER (-90.0f)
#define LOG_LEN 24
#define OP_CALL_FIRST 0x35

#define PMR_COUNT 16
#define FRS_COUNT 15

static const uint8_t frs_num[FRS_COUNT] = {
    1, 2, 3, 4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 22
};

static const uint32_t frs_freq[FRS_COUNT] = {
    462562500, 462587500, 462612500, 462637500,
    462662500, 462687500, 462712500, 462550000,
    462575000, 462600000, 462625000, 462650000,
    462675000, 462700000, 462725000,
};

typedef enum {
    BandPMR = 0,
    BandFRS = 1,
} Band;

typedef enum {
    KindSingle,
    KindDouble,
    KindArg,
    KindCustom,
} FnKind;

typedef struct {
    const char* name;
    FnKind kind;
    uint8_t op;
    uint8_t arg;
    uint8_t op2;
    uint8_t arg2;
} MdcFn;

#define FN_COUNT 20

static const MdcFn fns[FN_COUNT] = {
    {"PTT ID (Start)", KindSingle, 0x01, 0x80, 0, 0},
    {"PTT ID (End)", KindSingle, 0x01, 0x00, 0, 0},
    {"Emergency", KindSingle, 0x00, 0x81, 0, 0},
    {"Emergency ACK", KindSingle, 0x20, 0x00, 0, 0},
    {"Status Request", KindSingle, 0x22, 0x06, 0, 0},
    {"Radio Check", KindSingle, 0x63, 0x85, 0, 0},
    {"Radio Check ACK", KindSingle, 0x30, 0x00, 0, 0},
    {"Call Alert", KindDouble, OP_CALL_FIRST, 0x89, 0x81, 0x0D},
    {"Selective Call", KindDouble, OP_CALL_FIRST, 0x89, 0x80, 0x15},
    {"Remote Monitor", KindSingle, 0x11, 0x8A, 0, 0},
    {"Radio Enable", KindSingle, 0x2B, 0x0C, 0, 0},
    {"Radio Disable", KindSingle, 0x2B, 0x00, 0, 0},
    {"Disable ACK", KindSingle, 0x0B, 0x00, 0, 0},
    {"Enable ACK", KindSingle, 0x0B, 0x0C, 0, 0},
    {"Request to Talk", KindSingle, 0x41, 0x01, 0, 0},
    {"General ACK", KindSingle, 0x23, 0x00, 0, 0},
    {"Send Status (0-15)", KindArg, 0x46, 0x00, 0, 0},
    {"Status Response (0-15)", KindArg, 0x06, 0x00, 0, 0},
    {"Message (0-15)", KindArg, 0x47, 0x00, 0, 0},
    {"Custom", KindCustom, 0x01, 0x80, 0, 0},
};

typedef struct {
    uint8_t op;
    uint8_t arg;
    const char* name;
    const char* shrt;
} KnownOp;

static const KnownOp known[] = {
    {0x01, 0x80, "PTT ID", "PTT"},
    {0x01, 0x00, "PTT ID End", "POST"},
    {0x00, 0x81, "Emergency", "EMRG"},
    {0x00, 0x00, "Emergency", "EMRG"},
    {0x20, 0x00, "Emergency ACK", "EACK"},
    {0x22, 0x06, "Status Request", "STAT"},
    {0x22, 0x00, "Status Request", "STAT"},
    {0x63, 0x85, "Radio Check", "CHK"},
    {0x30, 0x00, "Check ACK", "CACK"},
    {0x11, 0x8A, "Remote Monitor", "MON"},
    {0x2B, 0x0C, "Radio Enable", "RAEN"},
    {0x2B, 0x00, "Radio Disable", "RADI"},
    {0x0B, 0x00, "Disable ACK", "IACK"},
    {0x0B, 0x0C, "Enable ACK", "EACK"},
    {0x41, 0x01, "Request to Talk", "RTT"},
    {0x40, 0x01, "Request to Talk", "RTT"},
    {0x23, 0x00, "General ACK", "ACK"},
    {OP_CALL_FIRST, 0x89, "Call (1/2)", "CALL"},
};

static const uint8_t mdc_preset[] = {
    0x02, 0x0D, 0x03, 0x47, 0x07, 0x04, 0x08, 0x32,
    0x0B, 0x06, 0x0C, 0x00, 0x10, 0xF8, 0x11, 0x83,
    0x12, 0x10, 0x13, 0x22, 0x14, 0xF8, 0x15, 0x04,
    0x17, 0x00, 0x18, 0x18, 0x19, 0x16, 0x1B, 0x43,
    0x1C, 0x40, 0x1D, 0x91, 0x20, 0xFB, 0x21, 0xB6,
    0x22, 0x10, 0x23, 0xE9, 0x24, 0x2A, 0x25, 0x00,
    0x26, 0x1F, 0x2C, 0x81, 0x2D, 0x35, 0x2E, 0x09,
    0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
};

static const uint8_t mdc_sync[5] = {0x07, 0x09, 0x2a, 0x44, 0x6f};

typedef struct {
    uint8_t op;
    uint8_t arg;
    uint16_t id;
    bool dbl;
    uint8_t op2;
    uint8_t arg2;
    uint16_t id2;
} MdcPkt;

static uint16_t mdc_crc(const uint8_t* d, unsigned len) {
    uint16_t crc = 0;

    for(unsigned i = 0; i < len; i++) {
        crc ^= d[i];

        for(int k = 0; k < 8; k++) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0x8408 : crc >> 1;
        }
    }

    return crc ^ 0xffff;
}

static void mdc_xor_modulation(uint8_t* data, unsigned size) {
    uint8_t prev_bit = 0;

    for(unsigned i = 0; i < size; i++) {
        uint8_t in = data[i];
        uint8_t out = 0;

        for(int bn = 7; bn >= 0; bn--) {
            uint8_t nb = (in >> bn) & 1u;

            if(nb != prev_bit) out |= (uint8_t)(1u << bn);

            prev_bit = nb;
        }

        data[i] = out ^ 0xff;
    }
}

static void mdc_encode_data(uint8_t* data8) {
    uint8_t sr = 0;

    for(int i = 0; i < FEC_K; i++) {
        const uint8_t bi = data8[i];
        uint8_t bo = 0;

        for(int bn = 0; bn < 8; bn++) {
            sr = (uint8_t)((sr << 1) | ((bi >> bn) & 1u));

            bo |= (uint8_t)(
                (((sr >> 6) ^ (sr >> 5) ^ (sr >> 2) ^ (sr >> 0)) & 1u)
                << bn);
        }

        data8[FEC_K + i] = bo;
    }

    uint8_t il[FEC_K * 2 * 8];
    unsigned k = 0;

    for(int i = 0; i < FEC_K * 2; i++) {
        const uint8_t b = data8[i];

        for(int bn = 0; bn < 8; bn++) {
            il[k] = (b >> bn) & 1u;
            k += 16;

            if(k >= sizeof(il)) k -= sizeof(il) - 1;
        }
    }

    k = 0;

    for(int i = 0; i < FEC_K * 2; i++) {
        uint8_t b = 0;

        for(int bn = 7; bn >= 0; bn--) {
            if(il[k++]) b |= (uint8_t)(1u << bn);
        }

        data8[i] = b;
    }
}

static uint8_t* mdc_put_cw(uint8_t* p, uint8_t op, uint8_t arg, uint16_t id) {
    p[0] = op;
    p[1] = arg;
    p[2] = (id >> 8) & 0xff;
    p[3] = id & 0xff;

    uint16_t crc = mdc_crc(p, 4);

    p[4] = crc & 0xff;
    p[5] = (crc >> 8) & 0xff;
    p[6] = 0;

    mdc_encode_data(p);

    return p + FEC_K * 2;
}

static unsigned mdc_build(uint8_t* out, const MdcPkt* pk) {
    uint8_t* p = out;

    memset(p, 0xFF, MDC_LEAD);
    p += MDC_LEAD;

    uint8_t* pkt = p;

    memset(p, 0x00, MDC_PRE);
    p += MDC_PRE;

    memcpy(p, mdc_sync, sizeof(mdc_sync));
    p += sizeof(mdc_sync);

    p = mdc_put_cw(p, pk->op, pk->arg, pk->id);

    if(pk->dbl) {
        p = mdc_put_cw(p, pk->op2, pk->arg2, pk->id2);
    }

    mdc_xor_modulation(pkt, (unsigned)(p - pkt));

    memset(p, 0xFF, MDC_TAIL);
    p += MDC_TAIL;

    return (unsigned)(p - out);
}

static void mdc_fec_correct(uint8_t* d) {
    uint8_t sr = 0;
    uint8_t syn = 0;

    for(int i = 0; i < FEC_K; i++) {
        const uint8_t bi = d[i];

        for(int bn = 0; bn < 8; bn++) {
            unsigned k = 0;

            sr = (uint8_t)((sr << 1) | ((bi >> bn) & 1u));

            uint8_t b =
                ((sr >> 6) ^ (sr >> 5) ^ (sr >> 2) ^ (sr >> 0)) & 1u;

            syn = (uint8_t)(
                (syn << 1) |
                (((b ^ (d[i + FEC_K] >> bn)) & 1u) ? 1u : 0u));

            if(syn & 0x80) k++;
            if(syn & 0x20) k++;
            if(syn & 0x04) k++;
            if(syn & 0x02) k++;

            if(k >= 3) {
                int ii = i;
                int bit = bn - 7;

                if(bit < 0) {
                    bit += 8;
                    ii--;
                }

                if(ii >= 0) d[ii] ^= (uint8_t)(1u << bit);

                syn ^= 0xA6;
            }
        }
    }
}

static bool mdc_decode(uint8_t* d) {
    uint8_t deint[FEC_K * 2 * 8];
    unsigned k = 0;

    for(unsigned i = 0; i < 16; i++) {
        for(unsigned m = 0; m < FEC_K; m++) {
            unsigned n = (m * 16) + i;

            deint[k++] = (d[n >> 3] >> ((7 - n) & 7u)) & 1u;
        }
    }

    k = 0;

    for(unsigned i = 0; i < FEC_K * 2; i++) {
        uint8_t b = 0;

        for(unsigned bn = 0; bn < 8; bn++) {
            if(deint[k++]) b |= (uint8_t)(1u << bn);
        }

        d[i] = b;
    }

    mdc_fec_correct(d);

    uint16_t crc1 = mdc_crc(d, 4);
    uint16_t crc2 = ((uint16_t)d[5] << 8) | d[4];

    return crc1 == crc2;
}

typedef enum {
    ScreenMenu,
    ScreenRx,
    ScreenTxList,
    ScreenTxSession,
    ScreenAbout,
} Screen;

typedef struct {
    uint32_t t;
    uint8_t op;
    uint8_t arg;
    uint16_t id;
    bool dbl;
    uint8_t op2;
    uint8_t arg2;
    uint16_t id2;
} LogEntry;

typedef struct {
    FuriMutex* mutex;
    FuriStreamBuffer* edges;
    uint32_t t0;

    Screen screen;
    uint8_t menu_sel;

    Band band;
    uint8_t ch[2];

    bool rx_on;
    bool want_rx;
    bool retune;
    bool do_tx;
    float rssi;
    bool carrier;
    uint8_t nocar;
    uint32_t last_rssi_tick;

    LogEntry log[LOG_LEN];
    uint8_t log_head;
    uint8_t log_count;
    uint8_t rx_sel;
    uint8_t rx_top;
    uint32_t rx_ok;
    uint32_t rx_err;

    uint8_t tx_sel;
    uint8_t tx_top;
    uint8_t tx_fn;
    uint8_t pos;
    uint8_t id_nib[4];
    uint8_t src_nib[4];
    uint8_t cust_nib[4];
    uint8_t arg_nib;
    bool inv_tx;
    char status[28];

    volatile uint32_t acc;

    uint64_t sync_raw;
    uint8_t run_label;
    uint32_t run_time;
    uint32_t prev_dur;
    uint64_t sr;
    uint8_t state;
    bool inv_rx;
    uint8_t dprev;
    uint8_t count;
    uint8_t data[FEC_K * 2];

    uint8_t txbuf[MDC_TX_MAX];
    uint32_t tx_nbits;
    uint32_t tx_bit;
    uint8_t tx_half;
    bool tx_level;
    bool tx_inv;
} App;

static const char* const menu_items[] = {
    "Receive",
    "Transmit",
    "About"
};

#define MENU_COUNT 3

static const char* fn_titles[FN_COUNT];

static uint32_t cur_freq(const App* app) {
    if(app->band == BandPMR) {
        return 446006250UL + 12500UL * app->ch[BandPMR];
    }

    return frs_freq[app->ch[BandFRS]];
}

static uint8_t band_count(Band b) {
    return b == BandPMR ? PMR_COUNT : FRS_COUNT;
}

static uint8_t cur_ch_number(const App* app) {
    return app->band == BandPMR
        ? (uint8_t)(app->ch[BandPMR] + 1)
        : frs_num[app->ch[BandFRS]];
}

static LogEntry* log_get(App* app, uint8_t i) {
    uint8_t idx = (uint8_t)((app->log_head + LOG_LEN - 1 - i) % LOG_LEN);
    return &app->log[idx];
}

static void log_add(App* app, uint8_t op, uint8_t arg, uint16_t id) {
    LogEntry* e = &app->log[app->log_head];

    memset(e, 0, sizeof(*e));

    e->t = (furi_get_tick() - app->t0) / 1000;
    e->op = op;
    e->arg = arg;
    e->id = id;

    app->log_head = (uint8_t)((app->log_head + 1) % LOG_LEN);

    if(app->log_count < LOG_LEN) app->log_count++;

    if(app->rx_sel > 0 && app->rx_sel < app->log_count - 1) {
        app->rx_sel++;
    }
}

static void entry_names(const LogEntry* e, const char** name, const char** shrt) {
    *name = NULL;
    *shrt = "?";

    if(e->dbl) {
        if(e->op2 == 0x81) {
            *name = "Call Alert";
            *shrt = "ALRT";
        } else if(e->op2 == 0x80 || e->op2 == 0x82) {
            *name = "Selective Call";
            *shrt = "SEL";
        } else {
            *name = "Double";
            *shrt = "DBL";
        }

        return;
    }

    for(unsigned i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if(known[i].op == e->op && known[i].arg == e->arg) {
            *name = known[i].name;
            *shrt = known[i].shrt;
            return;
        }
    }

    if(e->op == 0x46) {
        *name = "Status Sent";
        *shrt = "STS";
    } else if(e->op == 0x06) {
        *name = "Status Reply";
        *shrt = "STSR";
    } else if(e->op == 0x47 || e->op == 0x07) {
        *name = "Message";
        *shrt = "MSG";
    }
}

static void dec_reset(App* app) {
    app->run_label = 0;
    app->run_time = 0;
    app->prev_dur = 0;
    app->sr = 0;
    app->state = 0;
    app->count = 0;
}

static void finish_codeword(App* app) {
    uint8_t tmp[FEC_K * 2];

    memcpy(tmp, app->data, sizeof(tmp));

    bool ok = mdc_decode(tmp);
    uint16_t id = ((uint16_t)tmp[2] << 8) | tmp[3];

    if(app->state == 1) {
        if(ok) {
            log_add(app, tmp[0], tmp[1], id);
            app->rx_ok++;

            if(tmp[0] == OP_CALL_FIRST) {
                app->state = 2;
                app->count = 0;
                memset(app->data, 0, sizeof(app->data));
                return;
            }
        } else {
            app->rx_err++;
        }
    } else if(ok && app->log_count > 0) {
        LogEntry* e = log_get(app, 0);

        e->dbl = true;
        e->op2 = tmp[0];
        e->arg2 = tmp[1];
        e->id2 = id;
    }

    app->state = 0;
    app->sr = 0;
    app->count = 0;
}

static void push_bit(App* app, uint8_t bit) {
    app->sr = (app->sr << 1) | bit;

    if(app->state == 0) {
        const uint64_t mask40 = 0xFFFFFFFFFFULL;
        uint64_t x = (app->sr ^ app->sync_raw) & mask40;
        unsigned diff = (unsigned)__builtin_popcountll(x);

        bool match = false;
        bool inv = false;

        if(diff <= 4) {
            match = true;
        } else if(diff >= 36) {
            match = true;
            inv = true;
        }

        if(match) {
            app->inv_rx = inv;
            app->state = 1;
            app->count = 0;
            app->dprev = 1;
            memset(app->data, 0, sizeof(app->data));
        }

        return;
    }

    uint8_t r = app->inv_rx ? (uint8_t)(bit ^ 1u) : bit;
    uint8_t d = (uint8_t)(app->dprev ^ r ^ 1u);

    app->dprev = d;

    if(d) {
        app->data[app->count >> 3] |=
            (uint8_t)(1u << (7 - (app->count & 7)));
    }

    app->count++;

    if(app->count >= FEC_K * 16) finish_codeword(app);
}

static void dec_flush(App* app) {
    if(app->run_label == 0) return;

    uint32_t nbits = (app->run_time * 3 + 1250) / 2500;

    if(nbits > 64) nbits = 64;

    uint8_t bit = (app->run_label == 2) ? 1 : 0;

    while(nbits--) push_bit(app, bit);

    app->run_label = 0;
    app->run_time = 0;
}

static void dec_interval(App* app, uint32_t dur) {
    if(dur > 1100) {
        dec_flush(app);
        app->prev_dur = 0;
        return;
    }

    uint32_t p = app->prev_dur ? (app->prev_dur + dur) : (dur * 2);

    app->prev_dur = dur;

    uint8_t label = (p < 694) ? 1 : 2;

    if(label == app->run_label) {
        app->run_time += dur;
        return;
    }

    dec_flush(app);

    app->run_label = label;
    app->run_time = dur;
}

static void process_edges(App* app) {
    LevelDuration ld;
    int n = 0;

    while(n++ < 200 &&
          furi_stream_buffer_receive(app->edges, &ld, sizeof(ld), 0) == sizeof(ld)) {
        dec_interval(app, level_duration_get_duration(ld));
    }
}

static void rx_cb(bool level, uint32_t duration, void* ctx) {
    App* app = ctx;

    app->acc += duration;

    if(app->acc < 200) return;

    LevelDuration ld = level_duration_make(level, app->acc);

    furi_stream_buffer_send(app->edges, &ld, sizeof(ld), 0);

    app->acc = 0;
}

static void rx_start(App* app) {
    app->acc = 0;

    furi_stream_buffer_reset(app->edges);
    dec_reset(app);

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(mdc_preset);

    furi_hal_gpio_init(
        &gpio_cc1101_g0,
        GpioModeInput,
        GpioPullNo,
        GpioSpeedLow);

    furi_hal_subghz_set_frequency_and_path(cur_freq(app));
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
    furi_hal_subghz_start_async_rx(rx_cb, app);

    app->rx_on = true;
}

static void rx_stop(App* app) {
    if(!app->rx_on) return;

    furi_hal_subghz_stop_async_rx();
    furi_hal_subghz_idle();

    app->rx_on = false;
}

static LevelDuration tx_cb(void* ctx) {
    App* app = ctx;

    if(app->tx_bit >= app->tx_nbits) {
        return level_duration_reset();
    }

    bool bit =
        (app->txbuf[app->tx_bit >> 3] >> (7 - (app->tx_bit & 7))) & 1u;

    if(app->tx_inv) bit = !bit;

    uint8_t halves = bit ? 2 : 3;
    uint32_t dur = bit ? 417 : 278;

    app->tx_level = !app->tx_level;

    LevelDuration ld = level_duration_make(app->tx_level, dur);

    if(++app->tx_half >= halves) {
        app->tx_half = 0;
        app->tx_bit++;
    }

    return ld;
}

static uint8_t nib_count(FnKind k) {
    if(k == KindSingle) return 4;
    if(k == KindArg) return 5;

    return 8;
}

static uint8_t* nib_ptr(App* app, FnKind k, uint8_t i) {
    if(k == KindCustom) {
        return i < 4 ? &app->cust_nib[i] : &app->id_nib[i - 4];
    }

    if(k == KindDouble) {
        return i < 4 ? &app->id_nib[i] : &app->src_nib[i - 4];
    }

    if(k == KindArg) {
        return i == 0 ? &app->arg_nib : &app->id_nib[i - 1];
    }

    return &app->id_nib[i];
}

static uint16_t nib_val(const uint8_t* n) {
    return (uint16_t)(
        ((n[0] & 0xF) << 12) |
        ((n[1] & 0xF) << 8) |
        ((n[2] & 0xF) << 4) |
        (n[3] & 0xF));
}

static void make_pkt(App* app, MdcPkt* pk) {
    const MdcFn* f = &fns[app->tx_fn];

    memset(pk, 0, sizeof(*pk));

    pk->id = nib_val(app->id_nib);

    if(f->kind == KindCustom) {
        pk->op = (uint8_t)((app->cust_nib[0] << 4) | app->cust_nib[1]);
        pk->arg = (uint8_t)((app->cust_nib[2] << 4) | app->cust_nib[3]);
    } else if(f->kind == KindArg) {
        pk->op = f->op;
        pk->arg = (uint8_t)((f->arg & 0xF0) | (app->arg_nib & 0x0F));
    } else {
        pk->op = f->op;
        pk->arg = f->arg;
    }

    if(f->kind == KindDouble) {
        pk->dbl = true;
        pk->op2 = f->op2;
        pk->arg2 = f->arg2;
        pk->id2 = nib_val(app->src_nib);
    }
}

static void tx_send(App* app) {
    rx_stop(app);

    MdcPkt pk;

    make_pkt(app, &pk);

    unsigned n = mdc_build(app->txbuf, &pk);

    app->tx_nbits = n * 8;
    app->tx_bit = 0;
    app->tx_half = 0;
    app->tx_level = false;
    app->tx_inv = app->inv_tx;

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(mdc_preset);

    furi_hal_gpio_init(
        &gpio_cc1101_g0,
        GpioModeOutputPushPull,
        GpioPullNo,
        GpioSpeedLow);

    furi_hal_gpio_write(&gpio_cc1101_g0, false);

    furi_hal_subghz_set_frequency_and_path(cur_freq(app));

    furi_hal_gpio_write(&gpio_cc1101_g0, false);

    bool ok = furi_hal_subghz_start_async_tx(tx_cb, app);

    if(ok) {
        while(!furi_hal_subghz_is_async_tx_complete()) {
            furi_delay_ms(5);
        }

        furi_hal_subghz_stop_async_tx();
    }

    furi_hal_subghz_idle();

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    snprintf(
        app->status,
        sizeof(app->status),
        ok ? "TX sent" : "TX blocked (region)");

    furi_mutex_release(app->mutex);
}

static void fix_top(uint8_t sel, uint8_t* top, uint8_t rows) {
    if(sel < *top) *top = sel;

    if(sel >= *top + rows) {
        *top = (uint8_t)(sel - rows + 1);
    }
}

static void draw_list(
    Canvas* c,
    const char* title,
    const char* const* items,
    uint8_t count,
    uint8_t sel,
    uint8_t top) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, title);

    canvas_set_font(c, FontSecondary);

    for(uint8_t i = 0; i < 5 && (top + i) < count; i++) {
        uint8_t y = (uint8_t)(22 + i * 10);
        bool s = (top + i) == sel;

        if(s) {
            canvas_draw_box(c, 0, y - 8, 124, 10);
            canvas_set_color(c, ColorWhite);
        }

        canvas_draw_str(c, 4, y, items[top + i]);

        if(s) canvas_set_color(c, ColorBlack);
    }

    if(count > 5) elements_scrollbar(c, sel, count);
}

static void draw_menu(Canvas* c, App* app) {
    canvas_set_font(c, FontPrimary);

    canvas_draw_str(c, 38, 11, "MDC1200");
    canvas_draw_line(c, 10, 14, 117, 14);

    canvas_set_font(c, FontSecondary);

    for(uint8_t i = 0; i < MENU_COUNT; i++) {
        uint8_t y = (uint8_t)(20 + i * 14);
        bool sel = (app->menu_sel == i);

        if(sel) {
            canvas_draw_rbox(c, 12, y, 104, 12, 3);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_draw_rframe(c, 12, y, 104, 12, 3);
        }

        canvas_draw_str(c, 20, y + 9, menu_items[i]);

        if(sel) canvas_set_color(c, ColorBlack);
    }
}

static void draw_rx(Canvas* c, App* app) {
    char buf[48];
    char nm[8];

    canvas_set_font(c, FontSecondary);

    snprintf(
        nm,
        sizeof(nm),
        "%s%02u",
        app->band == BandPMR ? "PMR" : "FRS",
        cur_ch_number(app));

    uint32_t f = cur_freq(app);

    snprintf(
        buf,
        sizeof(buf),
        "%s %lu.%04lu",
        nm,
        f / 1000000UL,
        (f % 1000000UL) / 100UL);

    canvas_draw_str(c, 0, 8, buf);

    uint32_t ok = app->rx_ok > 99 ? 99 : app->rx_ok;
    uint32_t er = app->rx_err > 99 ? 99 : app->rx_err;

    snprintf(buf, sizeof(buf), "%lu/%lu", ok, er);
    canvas_draw_str(c, 90, 8, buf);

    if(app->carrier) canvas_draw_disc(c, 123, 5, 3);

    canvas_draw_line(c, 0, 11, 127, 11);

    if(app->log_count == 0) {
        canvas_draw_str(c, 2, 27, "Listening...");
        canvas_draw_str(c, 2, 40, "Left/Right: channel");
        canvas_draw_str(c, 2, 52, "OK: change band");
        canvas_draw_str(c, 2, 62, "Hold OK: clear log");
        return;
    }

    fix_top(app->rx_sel, &app->rx_top, 4);

    for(uint8_t i = 0; i < 4 && (app->rx_top + i) < app->log_count; i++) {
        uint8_t idx = (uint8_t)(app->rx_top + i);
        const LogEntry* e = log_get(app, idx);

        const char* name;
        const char* shrt;

        entry_names(e, &name, &shrt);

        uint8_t y = (uint8_t)(20 + i * 9);
        bool s = idx == app->rx_sel;

        if(s) {
            canvas_draw_box(c, 0, y - 7, 124, 9);
            canvas_set_color(c, ColorWhite);
        }

        snprintf(
            buf,
            sizeof(buf),
            "%02lu:%02lu %02X%02X %04X %s",
            (e->t / 60) % 100,
            e->t % 60,
            e->op,
            e->arg,
            e->id,
            shrt);

        canvas_draw_str(c, 2, y, buf);

        if(s) canvas_set_color(c, ColorBlack);
    }

    if(app->log_count > 4) {
        elements_scrollbar(c, app->rx_sel, app->log_count);
    }

    canvas_draw_line(c, 0, 51, 127, 51);

    const LogEntry* e = log_get(app, app->rx_sel);
    const char* name;
    const char* shrt;

    entry_names(e, &name, &shrt);

    if(e->dbl) {
        snprintf(buf, sizeof(buf), "%s %04X<-%04X", name, e->id, e->id2);
    } else if(name) {
        snprintf(buf, sizeof(buf), "%s %04X", name, e->id);
    } else {
        snprintf(buf, sizeof(buf), "OP%02X ARG%02X ID%04X", e->op, e->arg, e->id);
    }

    canvas_draw_str(c, 2, 61, buf);
}

static void draw_about(Canvas* c) {
    canvas_set_font(c, FontPrimary);

    canvas_draw_str(c, 32, 14, "About");
    canvas_draw_line(c, 10, 18, 117, 18);

    canvas_set_font(c, FontSecondary);

    canvas_draw_str(c, 14, 34, "MDC1200 v0.2.2beta");
    canvas_draw_str(c, 14, 50, "Created by Marcopiiit");
}

static void nib_pos(FnKind k, uint8_t i, uint8_t* x, uint8_t* y) {
    if(k == KindSingle) {
        *x = (uint8_t)(16 + 8 * i);
        *y = 38;
    } else if(k == KindDouble) {
        *x = (uint8_t)(30 + 8 * (i & 3));
        *y = (i < 4) ? 34 : 46;
    } else if(k == KindArg) {
        if(i == 0) {
            *x = 20;
            *y = 34;
        } else {
            *x = (uint8_t)(16 + 8 * (i - 1));
            *y = 46;
        }
    } else {
        if(i < 2) {
            *x = (uint8_t)(16 + 8 * i);
            *y = 34;
        } else if(i < 4) {
            *x = (uint8_t)(70 + 8 * (i - 2));
            *y = 34;
        } else {
            *x = (uint8_t)(16 + 8 * (i - 4));
            *y = 46;
        }
    }
}

static void draw_tx_session(Canvas* c, App* app) {
    const MdcFn* f = &fns[app->tx_fn];
    FnKind k = f->kind;
    uint8_t n = nib_count(k);

    char buf[32];

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 0, 9, f->name);

    canvas_set_font(c, FontSecondary);

    canvas_draw_str(c, 2, 21, app->band == BandPMR ? "PMR" : "FRS");

    snprintf(buf, sizeof(buf), "%02u", cur_ch_number(app));
    canvas_draw_str(c, 28, 21, buf);

    uint32_t fr = cur_freq(app);

    snprintf(
        buf,
        sizeof(buf),
        "%lu.%05lu",
        fr / 1000000UL,
        (fr % 1000000UL) / 10UL);

    canvas_draw_str(c, 50, 21, buf);

    if(app->pos == 0) canvas_draw_frame(c, 0, 12, 22, 11);
    if(app->pos == 1) canvas_draw_frame(c, 26, 12, 14, 11);

    if(k == KindSingle) {
        canvas_draw_str(c, 2, 38, "ID");

        snprintf(buf, sizeof(buf), "%02X/%02X", f->op, f->arg);
        canvas_draw_str(c, 76, 38, buf);
    } else if(k == KindDouble) {
        canvas_draw_str(c, 2, 34, "Dest");
        canvas_draw_str(c, 2, 46, "Source");

        snprintf(buf, sizeof(buf), "%02X/%02X", f->op, f->arg);
        canvas_draw_str(c, 76, 34, buf);

        snprintf(buf, sizeof(buf), "%02X/%02X", f->op2, f->arg2);
        canvas_draw_str(c, 76, 46, buf);
    } else if(k == KindArg) {
        canvas_draw_str(c, 2, 34, "Arg");
        canvas_draw_str(c, 2, 46, "ID");

        uint8_t arg_now =
            (uint8_t)((f->arg & 0xF0) | (app->arg_nib & 0x0F));

        snprintf(buf, sizeof(buf), "%02X/%02X", f->op, arg_now);
        canvas_draw_str(c, 76, 34, buf);
    } else {
        canvas_draw_str(c, 2, 34, "OP");
        canvas_draw_str(c, 48, 34, "ARG");
        canvas_draw_str(c, 2, 46, "ID");
    }

    for(uint8_t i = 0; i < n; i++) {
        uint8_t x, y;

        nib_pos(k, i, &x, &y);

        uint8_t v = *nib_ptr(app, k, i) & 0xF;

        char ch[2] = {
            (char)(v < 10 ? '0' + v : 'A' + v - 10),
            0
        };

        canvas_draw_str(c, x, y, ch);

        if(app->pos == 2 + i) {
            canvas_draw_frame(c, x - 2, y - 9, 9, 11);
        }
    }

    canvas_draw_str(c, 2, 62, app->inv_tx ? "Pol: inverted" : "Pol: normal");

    if(app->pos == 2 + n) {
        canvas_draw_frame(c, 0, 53, 50, 11);
    }

    canvas_draw_str(c, 62, 62, app->status[0] ? app->status : "OK: transmit");
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;

    if(furi_mutex_acquire(app->mutex, 25) == FuriStatusOk) {
        canvas_clear(canvas);

        switch(app->screen) {
        case ScreenMenu:
            draw_menu(canvas, app);
            break;

        case ScreenRx:
            draw_rx(canvas, app);
            break;

        case ScreenTxList:
            draw_list(
                canvas,
                "MDC1200 Function",
                fn_titles,
                FN_COUNT,
                app->tx_sel,
                app->tx_top);
            break;

        case ScreenTxSession:
            draw_tx_session(canvas, app);
            break;

        case ScreenAbout:
            draw_about(canvas);
            break;
        }

        furi_mutex_release(app->mutex);
    }
}

static void input_cb(InputEvent* ev, void* ctx) {
    furi_message_queue_put((FuriMessageQueue*)ctx, ev, 0);
}

static void change_channel(App* app, int delta) {
    uint8_t c = band_count(app->band);

    app->ch[app->band] =
        (uint8_t)((app->ch[app->band] + c + delta) % c);

    app->retune = true;
}

static void tx_edit(App* app, int delta) {
    FnKind k = fns[app->tx_fn].kind;
    uint8_t n = nib_count(k);

    if(app->pos == 0) {
        app->band = (app->band == BandPMR) ? BandFRS : BandPMR;
    } else if(app->pos == 1) {
        uint8_t c = band_count(app->band);

        app->ch[app->band] =
            (uint8_t)((app->ch[app->band] + c + delta) % c);
    } else if(app->pos < 2 + n) {
        uint8_t* p = nib_ptr(app, k, (uint8_t)(app->pos - 2));

        *p = (uint8_t)((*p + delta + 16) & 0xF);
    } else {
        app->inv_tx = !app->inv_tx;
    }
}

static bool handle_input(App* app, const InputEvent* ev) {
    app->status[0] = 0;

    bool is_short = ev->type == InputTypeShort;

    switch(app->screen) {
    case ScreenMenu:
        if(ev->key == InputKeyBack && is_short) return false;

        if(ev->key == InputKeyUp) {
            app->menu_sel =
                (uint8_t)((app->menu_sel + MENU_COUNT - 1) % MENU_COUNT);
        }

        if(ev->key == InputKeyDown) {
            app->menu_sel =
                (uint8_t)((app->menu_sel + 1) % MENU_COUNT);
        }

        if(ev->key == InputKeyOk && is_short) {
            if(app->menu_sel == 0) {
                app->screen = ScreenRx;
                app->want_rx = true;
            } else if(app->menu_sel == 1) {
                app->screen = ScreenTxList;
            } else {
                app->screen = ScreenAbout;
            }
        }

        break;

    case ScreenRx:
        if(ev->key == InputKeyBack && is_short) {
            app->screen = ScreenMenu;
            app->want_rx = false;
        } else if(ev->key == InputKeyLeft) {
            change_channel(app, -1);
        } else if(ev->key == InputKeyRight) {
            change_channel(app, 1);
        } else if(ev->key == InputKeyUp) {
            if(app->rx_sel > 0) app->rx_sel--;
        } else if(ev->key == InputKeyDown) {
            if(app->log_count > 0 && app->rx_sel < app->log_count - 1) {
                app->rx_sel++;
            }
        } else if(ev->key == InputKeyOk) {
            if(ev->type == InputTypeLong) {
                app->log_count = 0;
                app->log_head = 0;
                app->rx_sel = 0;
                app->rx_top = 0;
                app->rx_ok = 0;
                app->rx_err = 0;
            } else if(is_short) {
                app->band = (app->band == BandPMR) ? BandFRS : BandPMR;
                app->retune = true;
            }
        }

        break;

    case ScreenTxList:
        if(ev->key == InputKeyBack && is_short) {
            app->screen = ScreenMenu;
        } else if(ev->key == InputKeyUp) {
            app->tx_sel =
                (uint8_t)((app->tx_sel + FN_COUNT - 1) % FN_COUNT);

            fix_top(app->tx_sel, &app->tx_top, 5);
        } else if(ev->key == InputKeyDown) {
            app->tx_sel = (uint8_t)((app->tx_sel + 1) % FN_COUNT);

            fix_top(app->tx_sel, &app->tx_top, 5);
        } else if(ev->key == InputKeyOk && is_short) {
            app->tx_fn = app->tx_sel;
            app->pos = 2;
            app->screen = ScreenTxSession;
        }

        break;

    case ScreenTxSession: {
        uint8_t total =
            (uint8_t)(2 + nib_count(fns[app->tx_fn].kind) + 1);

        if(ev->key == InputKeyBack && is_short) {
            app->screen = ScreenTxList;
        } else if(ev->key == InputKeyLeft) {
            app->pos = (uint8_t)((app->pos + total - 1) % total);
        } else if(ev->key == InputKeyRight) {
            app->pos = (uint8_t)((app->pos + 1) % total);
        } else if(ev->key == InputKeyUp) {
            tx_edit(app, 1);
        } else if(ev->key == InputKeyDown) {
            tx_edit(app, -1);
        } else if(ev->key == InputKeyOk && is_short) {
            app->do_tx = true;
        }

        break;
    }

    case ScreenAbout:
        if(ev->key == InputKeyBack && is_short) {
            app->screen = ScreenMenu;
        }

        break;
    }

    return true;
}

static uint64_t calc_sync_raw(void) {
    uint8_t s[5];

    memcpy(s, mdc_sync, sizeof(s));
    mdc_xor_modulation(s, sizeof(s));

    uint64_t v = 0;

    for(int i = 0; i < 5; i++) {
        v = (v << 8) | s[i];
    }

    return v;
}

int32_t mdc1200_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));

    memset(app, 0, sizeof(App));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->edges = furi_stream_buffer_alloc(4096, sizeof(LevelDuration));
    app->sync_raw = calc_sync_raw();
    app->t0 = furi_get_tick();
    app->screen = ScreenMenu;
    app->band = BandPMR;
    app->rssi = -120.0f;

    const uint8_t id0[4] = {1, 2, 3, 4};
    const uint8_t src0[4] = {5, 6, 7, 8};
    const uint8_t cust0[4] = {0, 1, 8, 0};

    memcpy(app->id_nib, id0, 4);
    memcpy(app->src_nib, src0, 4);
    memcpy(app->cust_nib, cust0, 4);

    for(int i = 0; i < FN_COUNT; i++) {
        fn_titles[i] = fns[i].name;
    }

    FuriMessageQueue* queue =
        furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();

    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);

    Gui* gui = furi_record_open(RECORD_GUI);

    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    bool running = true;

    while(running) {
        InputEvent ev;

        if(furi_message_queue_get(queue, &ev, 25) == FuriStatusOk) {
            if(ev.type == InputTypeShort ||
               ev.type == InputTypeRepeat ||
               ev.type == InputTypeLong) {
                if(furi_mutex_acquire(app->mutex, 50) == FuriStatusOk) {
                    running = handle_input(app, &ev);
                    furi_mutex_release(app->mutex);
                }
            }
        }

        if(!running) break;

        if(app->want_rx && !app->rx_on) {
            rx_start(app);
        }

        if(!app->want_rx && app->rx_on) {
            rx_stop(app);
        }

        if(app->retune) {
            app->retune = false;

            if(app->rx_on) {
                rx_stop(app);
                rx_start(app);
            }
        }

        if(app->do_tx) {
            app->do_tx = false;

            if(furi_mutex_acquire(app->mutex, 50) == FuriStatusOk) {
                snprintf(
                    app->status,
                    sizeof(app->status),
                    "TX in progress...");

                furi_mutex_release(app->mutex);
            }

            view_port_update(vp);
            furi_delay_ms(60);

            tx_send(app);
        }

        if(furi_mutex_acquire(app->mutex, 50) == FuriStatusOk) {
            if(app->rx_on) {
                process_edges(app);

                uint32_t now = furi_get_tick();

                if(now - app->last_rssi_tick >= 100) {
                    app->last_rssi_tick = now;
                    app->rssi = furi_hal_subghz_get_rssi();
                    app->carrier = app->rssi > RSSI_CARRIER;

                    if(app->carrier) {
                        app->nocar = 0;
                    } else if(++app->nocar >= 5 && app->state != 0) {
                        app->state = 0;
                        app->sr = 0;
                        app->count = 0;
                    }
                }
            } else {
                app->carrier = false;
            }

            furi_mutex_release(app->mutex);
        }

        view_port_update(vp);
    }

    rx_stop(app);

    furi_hal_subghz_sleep();

    gui_remove_view_port(gui, vp);

    furi_record_close(RECORD_GUI);

    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_stream_buffer_free(app->edges);
    furi_mutex_free(app->mutex);

    free(app);

    return 0;
}