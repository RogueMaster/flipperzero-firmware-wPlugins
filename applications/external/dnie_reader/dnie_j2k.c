#include "dnie_j2k.h"
#include "dnie_dg2.h"

#include <stdlib.h>
#include <string.h>

// ===========================================================================
// Parametros del codestream
// ===========================================================================

#define J2K_MAX_COMPS  4
#define J2K_MAX_LEVELS 8
#define J2K_MAX_BANDS  (3 * J2K_MAX_LEVELS + 1)

typedef struct {
    uint8_t levels; // niveles de descomposicion
    uint8_t xcb, ycb; // exponentes del code-block
    uint8_t cb_style;
    uint8_t transform; // 0 = 9/7, 1 = 5/3
    bool custom_precincts;
} CompCod;

typedef struct {
    uint8_t style; // 0 sin cuantizar, 1 derivada, 2 expandida
    uint8_t guard;
    uint8_t eps[J2K_MAX_BANDS];
    uint16_t mu[J2K_MAX_BANDS];
    uint8_t count;
} CompQcd;

typedef struct {
    uint32_t width, height; // tile unico = imagen
    uint16_t comps;
    uint8_t prec0; // bits del componente 0
    bool signed0;
    uint8_t scod; // bit1 SOP, bit2 EPH
    uint8_t prog;
    uint16_t layers;
    uint8_t mct;
    CompCod cod[J2K_MAX_COMPS];
    CompQcd qcd0; // solo necesitamos la cuantizacion de la luminancia
    const uint8_t* tile_data;
    const uint8_t* tile_end;
} J2kParams;

static uint32_t rd16(const uint8_t* p) {
    return (uint32_t)p[0] << 8 | p[1];
}
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

// SPcod / SPcoc (a partir del byte de niveles)
static DnieJ2kResult parse_spcod(const uint8_t* s, size_t len, bool custom, CompCod* c) {
    if(len < 5) return DnieJ2kErrorFormat;
    c->levels = s[0];
    c->xcb = (uint8_t)(s[1] + 2);
    c->ycb = (uint8_t)(s[2] + 2);
    c->cb_style = s[3];
    c->transform = s[4];
    c->custom_precincts = false;
    if(custom) {
        // Aceptamos precintos "personalizados" solo si son el maximo (15): un precinto por resolucion
        if(len < 5u + c->levels + 1u) return DnieJ2kErrorFormat;
        for(int i = 0; i <= c->levels; i++) {
            if(s[5 + i] != 0xFF) c->custom_precincts = true;
        }
    }
    if(c->levels > J2K_MAX_LEVELS) return DnieJ2kErrorUnsupported;
    return DnieJ2kOk;
}

static DnieJ2kResult parse_sqcd(const uint8_t* s, size_t len, CompQcd* q) {
    if(len < 1) return DnieJ2kErrorFormat;
    q->style = s[0] & 0x1F;
    q->guard = s[0] >> 5;
    s++;
    len--;
    if(q->style == 0) {
        q->count = (uint8_t)(len > J2K_MAX_BANDS ? J2K_MAX_BANDS : len);
        for(int i = 0; i < q->count; i++) {
            q->eps[i] = s[i] >> 3;
            q->mu[i] = 0;
        }
    } else if(q->style == 1 || q->style == 2) {
        size_t n = len / 2;
        if(n == 0) return DnieJ2kErrorFormat;
        q->count = (uint8_t)(n > J2K_MAX_BANDS ? J2K_MAX_BANDS : n);
        for(int i = 0; i < q->count; i++) {
            uint32_t v = rd16(s + 2 * i);
            q->eps[i] = (uint8_t)(v >> 11);
            q->mu[i] = (uint16_t)(v & 0x7FF);
        }
    } else {
        return DnieJ2kErrorUnsupported;
    }
    return DnieJ2kOk;
}

// Recorre segmentos de marcador desde p hasta SOT (cabecera principal) o SOD (cabecera de tile)
static DnieJ2kResult parse_markers(
    const uint8_t** pp,
    const uint8_t* end,
    J2kParams* j,
    bool main_header,
    uint32_t* psot,
    const uint8_t** sot_start) {
    const uint8_t* p = *pp;
    DnieJ2kResult r;
    while(p + 2 <= end) {
        if(p[0] != 0xFF) return DnieJ2kErrorFormat;
        uint8_t m = p[1];
        if(main_header && m == 0x90) break; // SOT
        if(!main_header && m == 0x93) { // SOD
            p += 2;
            break;
        }
        if(p + 4 > end) return DnieJ2kErrorFormat;
        size_t seg = rd16(p + 2);
        if(seg < 2 || p + 2 + seg > end) return DnieJ2kErrorFormat;
        const uint8_t* s = p + 4;
        size_t sl = seg - 2;

        switch(m) {
        case 0x51: { // SIZ
            if(sl < 36 + 3) return DnieJ2kErrorFormat;
            uint32_t xsiz = rd32(s + 2), ysiz = rd32(s + 6), xo = rd32(s + 10), yo = rd32(s + 14);
            uint32_t xt = rd32(s + 18), yt = rd32(s + 22), xto = rd32(s + 26), yto = rd32(s + 30);
            j->comps = (uint16_t)rd16(s + 34);
            if(xo || yo || xto || yto) return DnieJ2kErrorUnsupported; // origenes distintos de 0
            if(xt < xsiz || yt < ysiz) return DnieJ2kErrorUnsupported; // varios tiles
            if(j->comps == 0 || j->comps > J2K_MAX_COMPS || sl < 36u + 3u * j->comps)
                return DnieJ2kErrorUnsupported;
            for(int c = 0; c < j->comps; c++) {
                if(s[37 + 3 * c] != 1 || s[38 + 3 * c] != 1) return DnieJ2kErrorUnsupported;
            }
            j->width = xsiz;
            j->height = ysiz;
            j->prec0 = (uint8_t)((s[36] & 0x7F) + 1);
            j->signed0 = (s[36] & 0x80) != 0;
            break;
        }
        case 0x52: { // COD (vale para todos los componentes salvo COC)
            if(sl < 10) return DnieJ2kErrorFormat;
            j->scod = s[0];
            j->prog = s[1];
            j->layers = (uint16_t)rd16(s + 2);
            j->mct = s[4];
            CompCod c;
            if((r = parse_spcod(s + 5, sl - 5, s[0] & 1, &c)) != DnieJ2kOk) return r;
            for(int i = 0; i < J2K_MAX_COMPS; i++)
                j->cod[i] = c;
            break;
        }
        case 0x53: { // COC
            size_t cb = j->comps < 257 ? 1 : 2;
            if(sl < cb + 1) return DnieJ2kErrorFormat;
            uint32_t comp = cb == 1 ? s[0] : rd16(s);
            if(comp < J2K_MAX_COMPS) {
                if((r = parse_spcod(s + cb + 1, sl - cb - 1, s[cb] & 1, &j->cod[comp])) !=
                   DnieJ2kOk)
                    return r;
            }
            break;
        }
        case 0x5C: // QCD
            if((r = parse_sqcd(s, sl, &j->qcd0)) != DnieJ2kOk) return r;
            break;
        case 0x5D: { // QCC
            size_t cb = j->comps < 257 ? 1 : 2;
            if(sl < cb + 1) return DnieJ2kErrorFormat;
            uint32_t comp = cb == 1 ? s[0] : rd16(s);
            if(comp == 0 && (r = parse_sqcd(s + cb, sl - cb, &j->qcd0)) != DnieJ2kOk) return r;
            break;
        }
        case 0x60: // PPM
        case 0x61: // PPT
            return DnieJ2kErrorUnsupported; // cabeceras de paquete empaquetadas
        case 0x5E: // RGN (region de interes)
            return DnieJ2kErrorUnsupported;
        default:
            break; // COM, TLM, PLM, PLT, CRG... no nos afectan
        }
        p += 2 + seg;
    }
    (void)psot;
    (void)sot_start;
    *pp = p;
    return DnieJ2kOk;
}

static DnieJ2kResult parse_codestream(const uint8_t* cs, size_t len, J2kParams* j) {
    memset(j, 0, sizeof(*j));
    const uint8_t* end = cs + len;
    if(len < 4 || cs[0] != 0xFF || cs[1] != 0x4F) return DnieJ2kErrorFormat; // SOC
    const uint8_t* p = cs + 2;
    DnieJ2kResult r = parse_markers(&p, end, j, true, NULL, NULL);
    if(r != DnieJ2kOk) return r;
    if(j->width == 0 || j->layers == 0) return DnieJ2kErrorFormat;
    if(j->layers != 1) return DnieJ2kErrorUnsupported;

    // SOT: Lsot(2)=10 Isot(2) Psot(4) TPsot(1) TNsot(1)
    if(p + 12 > end || p[0] != 0xFF || p[1] != 0x90) return DnieJ2kErrorFormat;
    const uint8_t* sot = p;
    uint32_t psot = rd32(p + 6);
    uint8_t tnsot = p[11];
    if(tnsot > 1) return DnieJ2kErrorUnsupported; // varias partes de tile
    p += 12;
    if((r = parse_markers(&p, end, j, false, NULL, NULL)) != DnieJ2kOk) return r;

    j->tile_data = p;
    j->tile_end = psot ? sot + psot : end;
    if(j->tile_end > end) j->tile_end = end; // solo tenemos el principio: avisaremos si falta
    if(j->tile_end < j->tile_data) return DnieJ2kErrorFormat;

    // Comprobaciones de lo que soportamos
    for(int c = 0; c < j->comps; c++) {
        if(j->cod[c].cb_style != 0) return DnieJ2kErrorUnsupported;
        if(j->cod[c].custom_precincts) return DnieJ2kErrorUnsupported;
    }
    if(j->prog > 4) return DnieJ2kErrorFormat;
    return DnieJ2kOk;
}

// ===========================================================================
// Geometria: resoluciones, subbandas y code-blocks (tile en el origen)
// ===========================================================================

static uint32_t ceil_div_pow2(uint32_t a, uint32_t b) {
    return (a + (1u << b) - 1) >> b;
}

static void res_size(const J2kParams* j, const CompCod* c, int r, uint32_t* w, uint32_t* h) {
    uint32_t s = (uint32_t)(c->levels - r);
    *w = ceil_div_pow2(j->width, s);
    *h = ceil_div_pow2(j->height, s);
}

// orient: 0 LL, 1 HL, 2 LH, 3 HH. Tamano de la subbanda y su posicion en el buffer de resolucion.
static void band_geom(
    const J2kParams* j,
    const CompCod* c,
    int r,
    int orient,
    uint32_t* bw,
    uint32_t* bh,
    uint32_t* ox,
    uint32_t* oy) {
    uint32_t rw, rh, lw = 0, lh = 0;
    res_size(j, c, r, &rw, &rh);
    if(r > 0) res_size(j, c, r - 1, &lw, &lh);
    if(r == 0) {
        *bw = rw;
        *bh = rh;
        *ox = *oy = 0;
        return;
    }
    *bw = (orient & 1) ? rw - lw : lw;
    *bh = (orient & 2) ? rh - lh : lh;
    *ox = (orient & 1) ? lw : 0;
    *oy = (orient & 2) ? lh : 0;
}

// ===========================================================================
// Lector de bits de las cabeceras de paquete (con "bit stuffing" tras 0xFF)
// ===========================================================================

typedef struct {
    const uint8_t* start;
    const uint8_t* p;
    const uint8_t* end;
    uint32_t buf;
    int ct;
    bool overflow; // se intento leer mas alla de los datos disponibles
} Bio;

static void bio_init(Bio* b, const uint8_t* p, const uint8_t* end) {
    b->start = b->p = p;
    b->end = end;
    b->buf = 0;
    b->ct = 0;
    b->overflow = false;
}

static void bio_bytein(Bio* b) {
    b->buf = (b->buf << 8) & 0xFFFF;
    b->ct = b->buf == 0xFF00 ? 7 : 8;
    if(b->p < b->end) {
        b->buf |= *b->p++;
    } else {
        b->overflow = true;
    }
}

static uint32_t bio_bit(Bio* b) {
    if(b->ct == 0) bio_bytein(b);
    b->ct--;
    return (b->buf >> b->ct) & 1;
}

static uint32_t bio_read(Bio* b, int n) {
    uint32_t v = 0;
    for(int i = n - 1; i >= 0; i--)
        v |= bio_bit(b) << i;
    return v;
}

static void bio_inalign(Bio* b) {
    if((b->buf & 0xFF) == 0xFF) bio_bytein(b);
    b->ct = 0;
}

// ===========================================================================
// Arboles de etiquetas (tag trees, B.10.2)
// ===========================================================================

typedef struct {
    int32_t value, low;
    int32_t parent;
} TgtNode;

typedef struct {
    TgtNode* nodes;
    uint32_t leaves_w, leaves_h;
} Tgt;

static bool tgt_create(Tgt* t, uint32_t w, uint32_t h) {
    uint32_t total = 0, lw = w, lh = h;
    uint32_t level_off[32], level_w[32], nlevels = 0;
    do {
        level_off[nlevels] = total;
        level_w[nlevels] = lw;
        total += lw * lh;
        nlevels++;
        if(lw * lh <= 1) break;
        lw = (lw + 1) / 2;
        lh = (lh + 1) / 2;
    } while(nlevels < 32);
    t->nodes = malloc(sizeof(TgtNode) * total);
    if(!t->nodes) return false;
    t->leaves_w = w;
    t->leaves_h = h;
    // Padre de (x,y) en el nivel i: (x/2, y/2) en el nivel i+1
    lw = w;
    lh = h;
    for(uint32_t i = 0; i < nlevels; i++) {
        for(uint32_t y = 0; y < lh; y++) {
            for(uint32_t x = 0; x < lw; x++) {
                TgtNode* n = &t->nodes[level_off[i] + y * level_w[i] + x];
                n->value = 999;
                n->low = 0;
                n->parent = (i + 1 < nlevels) ?
                                (int32_t)(level_off[i + 1] + (y / 2) * level_w[i + 1] + x / 2) :
                                -1;
            }
        }
        lw = (lw + 1) / 2;
        lh = (lh + 1) / 2;
    }
    return true;
}

static bool tgt_decode(Bio* b, Tgt* t, uint32_t leaf, int32_t threshold) {
    int32_t stack[32];
    int sp = 0;
    int32_t n = (int32_t)leaf;
    while(t->nodes[n].parent >= 0 && sp < 32) {
        stack[sp++] = n;
        n = t->nodes[n].parent;
    }
    int32_t low = 0;
    for(;;) {
        TgtNode* node = &t->nodes[n];
        if(low > node->low) {
            node->low = low;
        } else {
            low = node->low;
        }
        while(low < threshold && low < node->value) {
            if(bio_bit(b)) {
                node->value = low;
            } else {
                low++;
            }
        }
        node->low = low;
        if(sp == 0) break;
        n = stack[--sp];
    }
    return t->nodes[n].value < threshold;
}

// ===========================================================================
// Decodificador aritmetico MQ (anexo C)
// ===========================================================================

static const uint16_t MQ_QE[47] = {0x5601, 0x3401, 0x1801, 0x0AC1, 0x0521, 0x0221, 0x5601, 0x5401,
                                   0x4801, 0x3801, 0x3001, 0x2401, 0x1C01, 0x1601, 0x5601, 0x5401,
                                   0x5101, 0x4801, 0x3801, 0x3401, 0x3001, 0x2801, 0x2401, 0x2201,
                                   0x1C01, 0x1801, 0x1601, 0x1401, 0x1201, 0x1101, 0x0AC1, 0x09C1,
                                   0x08A1, 0x0521, 0x0441, 0x02A1, 0x0221, 0x0141, 0x0111, 0x0085,
                                   0x0049, 0x0025, 0x0015, 0x0009, 0x0005, 0x0001, 0x5601};
static const uint8_t MQ_NMPS[47] = {1,  2,  3,  4,  5,  38, 7,  8,  9,  10, 11, 12, 13, 29, 15, 16,
                                    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                                    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 45, 46};
static const uint8_t MQ_NLPS[47] = {1,  6,  9,  12, 29, 33, 6,  14, 14, 14, 17, 18, 20, 21, 14, 14,
                                    15, 16, 17, 18, 19, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                                    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 46};
static const uint8_t MQ_SWITCH[47] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define CTX_ZC  0 // 0..8
#define CTX_SC  9 // 9..13
#define CTX_MR  14 // 14..16
#define CTX_RL  17
#define CTX_UNI 18
#define NUM_CTX 19

typedef struct {
    const uint8_t* data;
    size_t len;
    size_t bp;
    uint32_t a, c;
    int ct;
    uint8_t state[NUM_CTX];
    uint8_t mps[NUM_CTX];
} Mq;

// Mas alla del final de los datos el flujo se rellena con 0xFF (como hace OpenJPEG)
static uint8_t mq_byte(const Mq* m, size_t i) {
    return i < m->len ? m->data[i] : 0xFF;
}

static void mq_bytein(Mq* m) {
    uint8_t next = mq_byte(m, m->bp + 1);
    if(mq_byte(m, m->bp) == 0xFF) {
        if(next > 0x8F) {
            m->c += 0xFF00;
            m->ct = 8;
        } else {
            m->bp++;
            m->c += (uint32_t)next << 9;
            m->ct = 7;
        }
    } else {
        m->bp++;
        m->c += (uint32_t)next << 8;
        m->ct = 8;
    }
}

static void mq_init(Mq* m, const uint8_t* data, size_t len) {
    m->data = data;
    m->len = len;
    m->bp = 0;
    m->c = (uint32_t)mq_byte(m, 0) << 16;
    mq_bytein(m);
    m->c <<= 7;
    m->ct -= 7;
    m->a = 0x8000;
    memset(m->state, 0, sizeof(m->state));
    memset(m->mps, 0, sizeof(m->mps));
    m->state[CTX_UNI] = 46;
    m->state[CTX_RL] = 3;
    m->state[CTX_ZC] = 4;
}

static void mq_renorm(Mq* m) {
    do {
        if(m->ct == 0) mq_bytein(m);
        m->a <<= 1;
        m->c <<= 1;
        m->ct--;
    } while(m->a < 0x8000);
}

static int mq_lps(Mq* m, int cx) {
    int d = 1 - m->mps[cx];
    if(MQ_SWITCH[m->state[cx]]) m->mps[cx] = (uint8_t)(1 - m->mps[cx]);
    m->state[cx] = MQ_NLPS[m->state[cx]];
    return d;
}

static int mq_decode(Mq* m, int cx) {
    uint32_t qe = MQ_QE[m->state[cx]];
    int d;
    m->a -= qe;
    if((m->c >> 16) < qe) {
        // LPS_EXCHANGE
        if(m->a < qe) {
            m->a = qe;
            d = m->mps[cx];
            m->state[cx] = MQ_NMPS[m->state[cx]];
        } else {
            m->a = qe;
            d = mq_lps(m, cx);
        }
        mq_renorm(m);
    } else {
        m->c -= qe << 16;
        if((m->a & 0x8000) == 0) {
            // MPS_EXCHANGE
            if(m->a < qe) {
                d = mq_lps(m, cx);
            } else {
                d = m->mps[cx];
                m->state[cx] = MQ_NMPS[m->state[cx]];
            }
            mq_renorm(m);
        } else {
            d = m->mps[cx];
        }
    }
    return d;
}

// ===========================================================================
// Tier-1: decodificacion de un code-block por planos de bits (EBCOT, anexo D)
// ===========================================================================

#define F_SIG     0x01
#define F_NEG     0x02
#define F_VISIT   0x04
#define F_REFINED 0x08

typedef struct {
    int w, h, stride;
    uint8_t* f; // flags con borde de 1
    int32_t* v; // valores (escala OpenJPEG: x2)
    Mq mq;
    int orient;
} T1;

#define T1F(t, x, y) ((t)->f[((y) + 1) * (t)->stride + (x) + 1])

static int sig_at(const T1* t, int x, int y) {
    return T1F(t, x, y) & F_SIG;
}

static int zc_context(const T1* t, int x, int y) {
    int h = sig_at(t, x - 1, y) + sig_at(t, x + 1, y);
    int v = sig_at(t, x, y - 1) + sig_at(t, x, y + 1);
    int d = sig_at(t, x - 1, y - 1) + sig_at(t, x + 1, y - 1) + sig_at(t, x - 1, y + 1) +
            sig_at(t, x + 1, y + 1);
    if(t->orient == 1) { // HL: H y V intercambiados
        int tmp = h;
        h = v;
        v = tmp;
    }
    if(t->orient == 3) { // HH
        int hv = h + v;
        if(d >= 3) return 8;
        if(d == 2) return hv >= 1 ? 7 : 6;
        if(d == 1) return hv >= 2 ? 5 : (hv == 1 ? 4 : 3);
        return hv >= 2 ? 2 : (hv == 1 ? 1 : 0);
    }
    // LL y LH
    if(h == 2) return 8;
    if(h == 1) return v >= 1 ? 7 : (d >= 1 ? 6 : 5);
    if(v == 2) return 4;
    if(v == 1) return 3;
    return d >= 2 ? 2 : (d == 1 ? 1 : 0);
}

static int contribution(const T1* t, int x, int y) {
    uint8_t f = T1F(t, x, y);
    if(!(f & F_SIG)) return 0;
    return (f & F_NEG) ? -1 : 1;
}

static int clamp1(int v) {
    return v > 1 ? 1 : (v < -1 ? -1 : v);
}

// Devuelve 1 si el coeficiente es negativo
static int decode_sign(T1* t, int x, int y) {
    int h = clamp1(contribution(t, x - 1, y) + contribution(t, x + 1, y));
    int v = clamp1(contribution(t, x, y - 1) + contribution(t, x, y + 1));
    int ctx, xr = 0;
    if(h == 1) {
        ctx = v == 1 ? 13 : (v == 0 ? 12 : 11);
    } else if(h == 0) {
        ctx = v == 0 ? 9 : 10;
        xr = v == -1;
    } else {
        ctx = v == 1 ? 11 : (v == 0 ? 12 : 13);
        xr = 1;
    }
    return mq_decode(&t->mq, ctx) ^ xr;
}

static void become_significant(T1* t, int x, int y, int32_t oneplushalf) {
    int neg = decode_sign(t, x, y);
    t->v[y * t->w + x] = neg ? -oneplushalf : oneplushalf;
    T1F(t, x, y) |= F_SIG | (neg ? F_NEG : 0);
}

static void sig_pass(T1* t, int bpno) {
    int32_t oneplushalf = (1 << bpno) | ((1 << bpno) >> 1);
    for(int y0 = 0; y0 < t->h; y0 += 4) {
        for(int x = 0; x < t->w; x++) {
            for(int y = y0; y < y0 + 4 && y < t->h; y++) {
                uint8_t f = T1F(t, x, y);
                if(f & F_SIG) continue;
                int ctx = zc_context(t, x, y);
                if(ctx == 0) continue; // sin vecinos significativos: lo vera la limpieza
                if(mq_decode(&t->mq, CTX_ZC + ctx)) become_significant(t, x, y, oneplushalf);
                T1F(t, x, y) |= F_VISIT;
            }
        }
    }
}

static void ref_pass(T1* t, int bpno) {
    int32_t half = (1 << bpno) >> 1;
    for(int y0 = 0; y0 < t->h; y0 += 4) {
        for(int x = 0; x < t->w; x++) {
            for(int y = y0; y < y0 + 4 && y < t->h; y++) {
                uint8_t f = T1F(t, x, y);
                if((f & (F_SIG | F_VISIT)) != F_SIG) continue;
                int ctx;
                if(f & F_REFINED) {
                    ctx = 16;
                } else {
                    int n = sig_at(t, x - 1, y) + sig_at(t, x + 1, y) + sig_at(t, x, y - 1) +
                            sig_at(t, x, y + 1) + sig_at(t, x - 1, y - 1) +
                            sig_at(t, x + 1, y - 1) + sig_at(t, x - 1, y + 1) +
                            sig_at(t, x + 1, y + 1);
                    ctx = n ? 15 : 14;
                }
                int32_t tt = mq_decode(&t->mq, ctx) ? half : -half;
                int32_t* vp = &t->v[y * t->w + x];
                *vp += *vp < 0 ? -tt : tt;
                T1F(t, x, y) |= F_REFINED;
            }
        }
    }
}

static void cln_pass(T1* t, int bpno) {
    int32_t oneplushalf = (1 << bpno) | ((1 << bpno) >> 1);
    for(int y0 = 0; y0 < t->h; y0 += 4) {
        bool full = y0 + 4 <= t->h;
        for(int x = 0; x < t->w; x++) {
            int start = y0;
            if(full) {
                // Modo "run-length": las 4 muestras sin significar, sin visitar y sin vecinos
                bool agg = true;
                for(int y = y0; y < y0 + 4 && agg; y++) {
                    if(T1F(t, x, y) & (F_SIG | F_VISIT))
                        agg = false;
                    else if(zc_context(t, x, y) != 0)
                        agg = false;
                }
                if(agg) {
                    if(!mq_decode(&t->mq, CTX_RL)) continue; // las 4 siguen a cero
                    int run = mq_decode(&t->mq, CTX_UNI) << 1;
                    run |= mq_decode(&t->mq, CTX_UNI);
                    become_significant(t, x, y0 + run, oneplushalf);
                    start = y0 + run + 1;
                }
            }
            for(int y = start; y < y0 + 4 && y < t->h; y++) {
                uint8_t f = T1F(t, x, y);
                if(f & (F_SIG | F_VISIT)) continue;
                if(mq_decode(&t->mq, CTX_ZC + zc_context(t, x, y)))
                    become_significant(t, x, y, oneplushalf);
            }
        }
    }
    // Fin del plano: se borran las marcas de visita
    for(int y = 0; y < t->h; y++) {
        for(int x = 0; x < t->w; x++)
            T1F(t, x, y) &= (uint8_t)~F_VISIT;
    }
}

// Decodifica un code-block. out recibe w*h valores en escala x2 (como OpenJPEG).
static bool t1_decode(
    const uint8_t* data,
    size_t len,
    int w,
    int h,
    int orient,
    int numbps,
    int passes,
    int32_t* out) {
    T1 t;
    t.w = w;
    t.h = h;
    t.stride = w + 2;
    t.orient = orient;
    t.v = out;
    memset(out, 0, sizeof(int32_t) * w * h);
    t.f = calloc((size_t)(w + 2) * (h + 2), 1);
    if(!t.f) return false;
    mq_init(&t.mq, data, len);

    int bpno = numbps; // "bpno_plus_one": la reconstruccion queda multiplicada por 2
    int type = 2; // el primer pase es de limpieza
    for(int p = 0; p < passes && bpno >= 1; p++) {
        if(type == 0) {
            sig_pass(&t, bpno);
        } else if(type == 1) {
            ref_pass(&t, bpno);
        } else {
            cln_pass(&t, bpno);
        }
        if(++type == 3) {
            type = 0;
            bpno--;
        }
    }
    free(t.f);
    return true;
}

// ===========================================================================
// Paquetes (tier-2) y lectura de los code-blocks de la luminancia
// ===========================================================================

typedef struct {
    const uint8_t* data;
    uint32_t len;
    uint16_t passes;
    uint8_t zbp;
    bool included;
} CbInfo;

// Code-blocks de la luminancia para las resoluciones que vamos a reconstruir
typedef struct {
    CbInfo* cbs; // [r][banda][cb]
    uint32_t count[J2K_MAX_LEVELS + 1][3];
    uint32_t cbw[J2K_MAX_LEVELS + 1][3]; // code-blocks por fila
    uint32_t offset[J2K_MAX_LEVELS + 1][3];
} LumaCbs;

static int bands_in_res(int r) {
    return r == 0 ? 1 : 3;
}
static int band_orient(int r, int b) {
    return r == 0 ? 0 : b + 1;
}

static uint32_t floorlog2(uint32_t v) {
    uint32_t l = 0;
    while(v >>= 1)
        l++;
    return l;
}

// Lee un paquete (capa 0) de (componente c, resolucion r). Si store != NULL guarda los code-blocks.
static DnieJ2kResult
    read_packet(const J2kParams* j, int c, int r, const uint8_t** pp, LumaCbs* store) {
    const CompCod* cod = &j->cod[c];
    const uint8_t* p = *pp;
    const uint8_t* end = j->tile_end;

    if((j->scod & 0x02) && p + 6 <= end && p[0] == 0xFF && p[1] == 0x91) p += 6; // SOP

    // Geometria de los code-blocks de cada banda de esta resolucion
    uint32_t ncb_x[3] = {0}, ncb_y[3] = {0};
    uint8_t xcb = cod->xcb, ycb = cod->ycb;
    // Con precintos maximos (2^15) el tamano efectivo del code-block es el nominal
    for(int b = 0; b < bands_in_res(r); b++) {
        uint32_t bw, bh, ox, oy;
        band_geom(j, cod, r, band_orient(r, b), &bw, &bh, &ox, &oy);
        ncb_x[b] = bw ? ceil_div_pow2(bw, xcb) : 0;
        ncb_y[b] = bh ? ceil_div_pow2(bh, ycb) : 0;
    }

    // Longitudes y pases de cada code-block (temporales si no los guardamos)
    uint32_t total_cbs = 0;
    for(int b = 0; b < 3; b++)
        total_cbs += ncb_x[b] * ncb_y[b];
    CbInfo* tmp = NULL;
    if(!store && total_cbs) {
        tmp = calloc(total_cbs, sizeof(CbInfo));
        if(!tmp) return DnieJ2kErrorMemory;
    }

    Bio bio;
    bio_init(&bio, p, end);
    DnieJ2kResult res = DnieJ2kOk;
    bool present = bio_bit(&bio);
    uint32_t base = 0;

    for(int b = 0; present && b < bands_in_res(r) && res == DnieJ2kOk; b++) {
        uint32_t n = ncb_x[b] * ncb_y[b];
        if(n == 0) continue;
        CbInfo* cbs = store ? &store->cbs[store->offset[r][b]] : &tmp[base];
        Tgt incl, imsb;
        incl.nodes = imsb.nodes = NULL;
        if(!tgt_create(&incl, ncb_x[b], ncb_y[b]) || !tgt_create(&imsb, ncb_x[b], ncb_y[b])) {
            free(incl.nodes);
            free(imsb.nodes);
            res = DnieJ2kErrorMemory;
            break;
        }
        for(uint32_t k = 0; k < n; k++) {
            CbInfo* cb = &cbs[k];
            cb->included = tgt_decode(&bio, &incl, k, 1); // capa 0: incluido si valor < 1
            if(!cb->included) continue;
            uint32_t i = 1;
            while(!tgt_decode(&bio, &imsb, k, (int32_t)i) && i < 64)
                i++;
            cb->zbp = (uint8_t)(i - 1);
            // Numero de pases
            uint32_t np;
            if(!bio_bit(&bio)) {
                np = 1;
            } else if(!bio_bit(&bio)) {
                np = 2;
            } else if((np = bio_read(&bio, 2)) != 3) {
                np += 3;
            } else if((np = bio_read(&bio, 5)) != 31) {
                np += 6;
            } else {
                np = 37 + bio_read(&bio, 7);
            }
            cb->passes = (uint16_t)np;
            // Lblock (empieza en 3) + codigo "coma"
            uint32_t lblock = 3;
            while(bio_bit(&bio))
                lblock++;
            uint32_t bits = lblock + floorlog2(np);
            if(bits > 31) {
                res = DnieJ2kErrorFormat;
                break;
            }
            cb->len = bio_read(&bio, (int)bits);
        }
        free(incl.nodes);
        free(imsb.nodes);
        base += n;
    }
    bio_inalign(&bio);
    if(bio.overflow && res == DnieJ2kOk) res = DnieJ2kErrorTruncated;
    p = bio.p;
    if((j->scod & 0x04) && p + 2 <= end && p[0] == 0xFF && p[1] == 0x92) p += 2; // EPH

    // Cuerpo: los datos de cada code-block, en el mismo orden
    base = 0;
    for(int b = 0; present && b < bands_in_res(r) && res == DnieJ2kOk; b++) {
        uint32_t n = ncb_x[b] * ncb_y[b];
        CbInfo* cbs = store ? &store->cbs[store->offset[r][b]] : &tmp[base];
        for(uint32_t k = 0; k < n; k++) {
            if(!cbs[k].included) continue;
            if(cbs[k].len > (size_t)(end - p)) {
                res = DnieJ2kErrorTruncated; // el resto del paquete esta mas adelante
                break;
            }
            cbs[k].data = p;
            p += cbs[k].len;
        }
        base += n;
    }
    free(tmp);
    *pp = p;
    return res;
}

// ===========================================================================
// Wavelet inversa (anexo F), con extension simetrica
// ===========================================================================

#define K97     1.230174104914001f
#define ALPHA97 -1.586134342059924f
#define BETA97  -0.052980118572961f
#define GAMMA97 0.882911075530934f
#define DELTA97 0.443506852043971f

static int mirror(int i, int n) {
    if(n == 1) return 0;
    while(i < 0 || i >= n) {
        if(i < 0) i = -i;
        if(i >= n) i = 2 * (n - 1) - i;
    }
    return i;
}

// line: [L0..Lsn-1 H0..Hdn-1] -> muestras reconstruidas (n = sn + dn), origen par
static void idwt_1d(float* line, float* tmp, int n, int sn, bool rev) {
    if(n == 1) return; // una sola muestra: se deja tal cual
    for(int i = 0; i < sn; i++)
        tmp[2 * i] = line[i];
    for(int i = 0; i < n - sn; i++)
        tmp[2 * i + 1] = line[sn + i];

    if(rev) {
        // 5/3 reversible
        for(int i = 0; i < n; i += 2)
            tmp[i] -= (float)(int)((tmp[mirror(i - 1, n)] + tmp[mirror(i + 1, n)] + 2) / 4);
        for(int i = 1; i < n; i += 2)
            tmp[i] += (float)(int)((tmp[mirror(i - 1, n)] + tmp[mirror(i + 1, n)]) / 2);
    } else {
        // 9/7 irreversible: escalado (la paso alta lleva un 2/K, como OpenJPEG) y 4 pasos de lifting
        for(int i = 0; i < n; i += 2)
            tmp[i] *= K97;
        for(int i = 1; i < n; i += 2)
            tmp[i] *= 2.0f / K97;
        for(int i = 0; i < n; i += 2)
            tmp[i] -= DELTA97 * (tmp[mirror(i - 1, n)] + tmp[mirror(i + 1, n)]);
        for(int i = 1; i < n; i += 2)
            tmp[i] -= GAMMA97 * (tmp[mirror(i - 1, n)] + tmp[mirror(i + 1, n)]);
        for(int i = 0; i < n; i += 2)
            tmp[i] -= BETA97 * (tmp[mirror(i - 1, n)] + tmp[mirror(i + 1, n)]);
        for(int i = 1; i < n; i += 2)
            tmp[i] -= ALPHA97 * (tmp[mirror(i - 1, n)] + tmp[mirror(i + 1, n)]);
    }
    memcpy(line, tmp, sizeof(float) * n);
}

// ===========================================================================
// API
// ===========================================================================

static float pow2f(int e) {
    float v = 1.0f;
    while(e > 0) {
        v *= 2.0f;
        e--;
    }
    while(e < 0) {
        v *= 0.5f;
        e++;
    }
    return v;
}

DnieJ2kResult
    dnie_j2k_decode_luma(const uint8_t* data, size_t len, uint16_t min_height, DnieGrayImage* out) {
    memset(out, 0, sizeof(*out));

    // .jp2 o .j2k: localizar el codestream
    DnieJ2kInfo info;
    if(!dnie_j2k_parse_header(data, len, &info)) return DnieJ2kErrorFormat;
    J2kParams j;
    DnieJ2kResult res =
        parse_codestream(data + info.codestream_offset, len - info.codestream_offset, &j);
    if(res != DnieJ2kOk) return res;

    const CompCod* c0 = &j.cod[0];
    int nl = c0->levels;

    // Resolucion objetivo: la mas pequena con altura >= min_height
    int rmax = nl;
    for(int r = 0; r <= nl; r++) {
        uint32_t w, h;
        res_size(&j, c0, r, &w, &h);
        if(h >= min_height) {
            rmax = r;
            break;
        }
    }
    uint32_t out_w, out_h;
    res_size(&j, c0, rmax, &out_w, &out_h);

    // Reservar el registro de code-blocks de la luminancia
    LumaCbs luma;
    memset(&luma, 0, sizeof(luma));
    uint32_t total = 0;
    for(int r = 0; r <= rmax; r++) {
        for(int b = 0; b < bands_in_res(r); b++) {
            uint32_t bw, bh, ox, oy;
            band_geom(&j, c0, r, band_orient(r, b), &bw, &bh, &ox, &oy);
            luma.cbw[r][b] = bw ? ceil_div_pow2(bw, c0->xcb) : 0;
            luma.count[r][b] = luma.cbw[r][b] * (bh ? ceil_div_pow2(bh, c0->ycb) : 0);
            luma.offset[r][b] = total;
            total += luma.count[r][b];
        }
    }
    luma.cbs = calloc(total ? total : 1, sizeof(CbInfo));
    if(!luma.cbs) return DnieJ2kErrorMemory;

    // Recorrer paquetes en el orden de progresion hasta tener toda la luminancia necesaria.
    // Con 1 capa y 1 precinto: LRCP/RLCP/RPCL = resolucion y luego componente; PCRL/CPRL = al reves.
    const uint8_t* p = j.tile_data;
    bool res_major = j.prog <= 2;
    int max_levels = 0;
    for(int c = 0; c < j.comps; c++)
        if(j.cod[c].levels > max_levels) max_levels = j.cod[c].levels;
    bool done = false;
    for(int outer = 0;
        outer <= (res_major ? max_levels : j.comps - 1) && !done && res == DnieJ2kOk;
        outer++) {
        for(int inner = 0; inner <= (res_major ? j.comps - 1 : max_levels) && res == DnieJ2kOk;
            inner++) {
            int r = res_major ? outer : inner;
            int c = res_major ? inner : outer;
            if(r > j.cod[c].levels) continue;
            res = read_packet(&j, c, r, &p, (c == 0 && r <= rmax) ? &luma : NULL);
            if(c == 0 && r == rmax) {
                done = true;
                break;
            }
        }
    }
    if(res != DnieJ2kOk) {
        free(luma.cbs);
        return res;
    }

    // Buffer de la luminancia a la resolucion objetivo (subbandas colocadas como en OpenJPEG)
    float* img = calloc((size_t)out_w * out_h, sizeof(float));
    // Un code-block nunca supera su subbanda, y con rmax > 0 ninguna subbanda pasa de la
    // mitad (redondeada hacia arriba) de la imagen de salida
    uint32_t band_max_w = rmax > 0 ? (out_w + 1) / 2 : out_w;
    uint32_t band_max_h = rmax > 0 ? (out_h + 1) / 2 : out_h;
    uint32_t cb_max_w = (1u << c0->xcb) < band_max_w ? (1u << c0->xcb) : band_max_w;
    uint32_t cb_max_h = (1u << c0->ycb) < band_max_h ? (1u << c0->ycb) : band_max_h;
    int32_t* cbuf = malloc(sizeof(int32_t) * cb_max_w * cb_max_h);
    if(!img || !cbuf) {
        free(img);
        free(cbuf);
        free(luma.cbs);
        return DnieJ2kErrorMemory;
    }

    const CompQcd* q = &j.qcd0;
    for(int r = 0; r <= rmax; r++) {
        for(int b = 0; b < bands_in_res(r); b++) {
            int orient = band_orient(r, b);
            uint32_t bw, bh, ox, oy;
            band_geom(&j, c0, r, orient, &bw, &bh, &ox, &oy);
            if(!bw || !bh) continue;

            // Cuantizacion de la banda
            int band_index = r == 0 ? 0 : 3 * (r - 1) + orient;
            int eps;
            float mu;
            if(q->style == 1) { // derivada: todo sale del LL
                eps = (int)q->eps[0] - (r == 0 ? 0 : r - 1);
                if(eps < 0) eps = 0;
                mu = q->mu[0];
            } else {
                if(band_index >= q->count) {
                    res = DnieJ2kErrorFormat;
                    break;
                }
                eps = q->eps[band_index];
                mu = q->mu[band_index];
            }
            int mb = q->guard + eps - 1; // bits de magnitud de la banda
            float step = (1.0f + mu / 2048.0f) * pow2f((int)j.prec0 - eps);
            if(c0->transform == 1) step = 1.0f; // 5/3: sin cuantizar

            for(uint32_t k = 0; k < luma.count[r][b]; k++) {
                const CbInfo* cb = &luma.cbs[luma.offset[r][b] + k];
                uint32_t cx = k % luma.cbw[r][b], cy = k / luma.cbw[r][b];
                uint32_t x0 = cx << c0->xcb, y0 = cy << c0->ycb;
                uint32_t w = bw - x0 < (1u << c0->xcb) ? bw - x0 : (1u << c0->xcb);
                uint32_t h = bh - y0 < (1u << c0->ycb) ? bh - y0 : (1u << c0->ycb);
                if(!cb->included || cb->passes == 0) continue; // todo ceros
                if(w > cb_max_w || h > cb_max_h) { // no deberia pasar nunca: no desbordar
                    res = DnieJ2kErrorFormat;
                    break;
                }
                int numbps = mb - cb->zbp;
                if(numbps <= 0 || numbps > 30) continue;
                if(!t1_decode(
                       cb->data, cb->len, (int)w, (int)h, orient, numbps, cb->passes, cbuf)) {
                    res = DnieJ2kErrorMemory;
                    break;
                }
                for(uint32_t yy = 0; yy < h; yy++) {
                    float* row = &img[(oy + y0 + yy) * out_w + ox + x0];
                    for(uint32_t xx = 0; xx < w; xx++) {
                        int32_t v = cbuf[yy * w + xx];
                        row[xx] = c0->transform == 1 ? (float)(v / 2) : (float)v * 0.5f * step;
                    }
                }
            }
            if(res != DnieJ2kOk) break;
        }
        if(res != DnieJ2kOk) break;
    }
    free(cbuf);
    free(luma.cbs);

    // Wavelet inversa nivel a nivel: filas y luego columnas
    if(res == DnieJ2kOk) {
        uint32_t maxdim = out_w > out_h ? out_w : out_h;
        float* line = malloc(sizeof(float) * maxdim);
        float* tmp = malloc(sizeof(float) * maxdim);
        if(!line || !tmp) {
            res = DnieJ2kErrorMemory;
        } else {
            for(int r = 1; r <= rmax; r++) {
                uint32_t rw, rh, lw, lh;
                res_size(&j, c0, r, &rw, &rh);
                res_size(&j, c0, r - 1, &lw, &lh);
                bool rev = c0->transform == 1;
                for(uint32_t y = 0; y < rh; y++) {
                    idwt_1d(&img[y * out_w], tmp, (int)rw, (int)lw, rev);
                }
                for(uint32_t x = 0; x < rw; x++) {
                    for(uint32_t y = 0; y < rh; y++)
                        line[y] = img[y * out_w + x];
                    idwt_1d(line, tmp, (int)rh, (int)lh, rev);
                    for(uint32_t y = 0; y < rh; y++)
                        img[y * out_w + x] = line[y];
                }
            }
        }
        free(line);
        free(tmp);
    }

    if(res == DnieJ2kOk) {
        // La salida de 8 bits se escribe sobre el propio buffer de floats (cabe de sobra)
        // y luego se encoge: evitamos tener los dos a la vez en memoria.
        uint8_t* px = (uint8_t*)img;
        {
            // Desplazamiento de nivel DC; con MCT el componente 0 es la luminancia Y
            float dc = j.signed0 ? 128.0f : (float)(1u << (j.prec0 - 1));
            float scale = j.prec0 > 8 ? 1.0f / (float)(1u << (j.prec0 - 8)) : 1.0f;
            for(uint32_t i = 0; i < out_w * out_h; i++) {
                float v = (img[i] + dc) * scale + 0.5f;
                px[i] =
                    (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); // i <= 4*i: no pisa lo pendiente
            }
            void* shrunk = realloc(img, (size_t)out_w * out_h);
            // Si realloc falla, el bloque original (con los pixeles ya escritos) sigue valido
            out->pixels = shrunk ? (uint8_t*)shrunk : px;
            img = NULL;
            out->width = (uint16_t)out_w;
            out->height = (uint16_t)out_h;
            out->reduce = (uint8_t)(nl - rmax);
        }
    }
    free(img);
    return res;
}

void dnie_gray_free(DnieGrayImage* img) {
    free(img->pixels);
    img->pixels = NULL;
}

const char* dnie_j2k_result_str(DnieJ2kResult result) {
    switch(result) {
    case DnieJ2kOk:
        return "OK";
    case DnieJ2kErrorFormat:
        return "JPEG2000 mal formado";
    case DnieJ2kErrorUnsupported:
        return "Variante JPEG2000\nno soportada";
    case DnieJ2kErrorMemory:
        return "Sin memoria";
    case DnieJ2kErrorTruncated:
        return "JPEG2000 incompleto";
    }
    return "Unknown error";
}
