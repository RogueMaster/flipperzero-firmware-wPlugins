#include "dnie_jpeg.h"

#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 512

typedef struct {
    uint8_t counts[17]; // codigos de cada longitud (1..16)
    uint8_t symbols[256];
    int32_t maxcode[18];
    int32_t valptr[17];
    int32_t mincode[17];
    bool present;
} Huff;

typedef struct {
    uint8_t id, h, v, tq, td, ta;
    int pred;
} Comp;

typedef struct {
    DnieJpegRead read;
    void* ctx;
    uint8_t buf[BUF_SIZE];
    size_t pos, len;
    bool eof;
    // Lector de bits de los datos entropicos
    uint32_t bits;
    int nbits;
    bool marker_hit; // se encontro un marcador: a partir de ahi se rellena con ceros
    uint8_t marker; // cual (RSTn o EOI)
    uint16_t qdc[4];
    Huff dc[4], ac[4];
    Comp comp[3];
    int ncomp;
    uint16_t width, height;
    uint16_t restart;
} Jpeg;

static int byte_in(Jpeg* j) {
    if(j->pos >= j->len) {
        if(j->eof) return -1;
        j->len = j->read(j->ctx, j->buf, BUF_SIZE);
        j->pos = 0;
        if(j->len == 0) {
            j->eof = true;
            return -1;
        }
    }
    return j->buf[j->pos++];
}

static int word_in(Jpeg* j) {
    int a = byte_in(j), b = byte_in(j);
    if(a < 0 || b < 0) return -1;
    return a << 8 | b;
}

static bool skip(Jpeg* j, int n) {
    while(n-- > 0)
        if(byte_in(j) < 0) return false;
    return true;
}

// ---- Huffman (ITU T.81, anexo F.2.2.3) ----

static void huff_build(Huff* h) {
    int code = 0, k = 0;
    for(int l = 1; l <= 16; l++) {
        h->valptr[l] = k;
        h->mincode[l] = code;
        code += h->counts[l];
        k += h->counts[l];
        h->maxcode[l] = h->counts[l] ? code - 1 : -1;
        code <<= 1;
    }
    h->maxcode[17] = 0x7FFFFFFF;
    h->present = true;
}

static int bit_in(Jpeg* j) {
    if(j->nbits == 0) {
        int b = 0;
        if(!j->marker_hit) {
            b = byte_in(j);
            if(b < 0) return -1;
            if(b == 0xFF) {
                int n = byte_in(j);
                if(n < 0) return -1;
                if(n != 0) {
                    // Marcador (RST o EOI) dentro de los datos: lo dejamos para quien lo pida
                    j->marker_hit = true;
                    j->marker = (uint8_t)n;
                    b = 0;
                }
            }
        }
        j->bits = (uint32_t)b;
        j->nbits = 8;
    }
    j->nbits--;
    return (j->bits >> j->nbits) & 1;
}

static int bits_in(Jpeg* j, int n) {
    int v = 0;
    while(n-- > 0) {
        int b = bit_in(j);
        if(b < 0) return -1;
        v = v << 1 | b;
    }
    return v;
}

static int huff_decode(Jpeg* j, const Huff* h) {
    int code = 0;
    for(int l = 1; l <= 16; l++) {
        int b = bit_in(j);
        if(b < 0) return -1;
        code = code << 1 | b;
        if(code <= h->maxcode[l]) return h->symbols[h->valptr[l] + code - h->mincode[l]];
    }
    return -1; // codigo invalido
}

// Valor de s bits con signo (F.2.2.1 EXTEND)
static int extend(int v, int s) {
    return v < (1 << (s - 1)) ? v - (1 << s) + 1 : v;
}

// Bloque 8x8: devuelve el DC (sin descuantizar). Los AC solo se saltan.
static bool block(Jpeg* j, Comp* c, int* dc) {
    int s = huff_decode(j, &j->dc[c->td]);
    if(s < 0 || s > 11) return false;
    int diff = 0;
    if(s) {
        int v = bits_in(j, s);
        if(v < 0) return false;
        diff = extend(v, s);
    }
    c->pred += diff;
    *dc = c->pred;
    for(int k = 1; k < 64;) {
        int rs = huff_decode(j, &j->ac[c->ta]);
        if(rs < 0) return false;
        int r = rs >> 4, sz = rs & 15;
        if(sz == 0) {
            if(r != 15) break; // EOB
            k += 16;
        } else {
            k += r + 1;
            if(bits_in(j, sz) < 0) return false;
        }
    }
    return true;
}

// Tras un intervalo de reinicio: saltar hasta el marcador RSTn y reiniciar predictores
static bool restart(Jpeg* j) {
    j->nbits = 0;
    int b;
    if(j->marker_hit) {
        b = j->marker; // ya leido por el lector de bits
    } else {
        do {
            b = byte_in(j);
            if(b < 0) return false;
        } while(b != 0xFF);
        do {
            b = byte_in(j);
        } while(b == 0xFF);
    }
    j->marker_hit = false;
    if(b < 0xD0 || b > 0xD7) return false;
    for(int i = 0; i < j->ncomp; i++)
        j->comp[i].pred = 0;
    return true;
}

static DnieJ2kResult scan(Jpeg* j, DnieGrayImage* out) {
    int hmax = 1, vmax = 1;
    for(int i = 0; i < j->ncomp; i++) {
        if(j->comp[i].h > hmax) hmax = j->comp[i].h;
        if(j->comp[i].v > vmax) vmax = j->comp[i].v;
    }
    int mcux = (j->width + 8 * hmax - 1) / (8 * hmax);
    int mcuy = (j->height + 8 * vmax - 1) / (8 * vmax);
    Comp* y = &j->comp[0];
    // Bloques de luminancia que caen dentro de la imagen
    int yw_img = (j->width * y->h + 8 * hmax - 1) / (8 * hmax);
    int yh_img = (j->height * y->v + 8 * vmax - 1) / (8 * vmax);

    out->width = (uint16_t)yw_img;
    out->height = (uint16_t)yh_img;
    out->reduce = 3;
    out->pixels = malloc((size_t)yw_img * yh_img);
    if(!out->pixels) return DnieJ2kErrorMemory;

    int q = j->qdc[y->tq];
    int count = 0;
    for(int my = 0; my < mcuy; my++) {
        for(int mx = 0; mx < mcux; mx++) {
            if(j->restart && count && count % j->restart == 0) {
                if(!restart(j)) goto bad;
            }
            count++;
            for(int ci = 0; ci < j->ncomp; ci++) {
                Comp* c = &j->comp[ci];
                for(int by = 0; by < c->v; by++) {
                    for(int bx = 0; bx < c->h; bx++) {
                        int dc;
                        if(!block(j, c, &dc)) goto bad;
                        if(ci != 0) continue;
                        int px = mx * c->h + bx, py = my * c->v + by;
                        if(px >= yw_img || py >= yh_img) continue;
                        // Media del bloque = DC*Q/8 + 128
                        int v = ((dc * q + 4) >> 3) + 128; // como libjpeg (DESCALE)
                        out->pixels[py * yw_img + px] = v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)v);
                    }
                }
            }
        }
    }
    return DnieJ2kOk;
bad:
    free(out->pixels);
    out->pixels = NULL;
    return DnieJ2kErrorFormat;
}

DnieJ2kResult dnie_jpeg_decode_dc(DnieJpegRead read, void* ctx, DnieGrayImage* out) {
    memset(out, 0, sizeof(*out));
    Jpeg* j = calloc(1, sizeof(Jpeg));
    if(!j) return DnieJ2kErrorMemory;
    j->read = read;
    j->ctx = ctx;
    DnieJ2kResult r = DnieJ2kErrorFormat;
    bool sof = false;

    if(byte_in(j) != 0xFF || byte_in(j) != 0xD8) goto done; // SOI
    for(;;) {
        int b = byte_in(j);
        if(b < 0) goto done;
        if(b != 0xFF) continue;
        int m;
        do {
            m = byte_in(j);
        } while(m == 0xFF);
        if(m < 0 || m == 0xD9) goto done;
        if(m >= 0xD0 && m <= 0xD7) continue;
        int len = word_in(j);
        if(len < 2) goto done;
        len -= 2;

        if(m == 0xDB) { // DQT
            while(len > 0) {
                int pq = byte_in(j);
                if(pq < 0 || (pq & 15) > 3) goto done;
                int n = (pq >> 4) ? 128 : 64;
                int first = (pq >> 4) ? word_in(j) : byte_in(j);
                if(first < 0) goto done;
                j->qdc[pq & 15] = (uint16_t)first;
                if(!skip(j, n - ((pq >> 4) ? 2 : 1))) goto done;
                len -= 1 + n;
            }
        } else if(m == 0xC4) { // DHT
            while(len > 0) {
                int tc = byte_in(j);
                if(tc < 0 || (tc & 15) > 3) goto done;
                Huff* h = (tc >> 4) ? &j->ac[tc & 15] : &j->dc[tc & 15];
                int total = 0;
                for(int l = 1; l <= 16; l++) {
                    int c = byte_in(j);
                    if(c < 0) goto done;
                    h->counts[l] = (uint8_t)c;
                    total += c;
                }
                if(total > 256) goto done;
                for(int k = 0; k < total; k++) {
                    int s = byte_in(j);
                    if(s < 0) goto done;
                    h->symbols[k] = (uint8_t)s;
                }
                huff_build(h);
                len -= 17 + total;
            }
        } else if(m == 0xC0 || m == 0xC1) { // SOF0/1: secuencial Huffman
            int p = byte_in(j);
            j->height = (uint16_t)word_in(j);
            j->width = (uint16_t)word_in(j);
            j->ncomp = byte_in(j);
            if(p != 8 || !j->width || !j->height || (j->ncomp != 1 && j->ncomp != 3)) {
                r = DnieJ2kErrorUnsupported;
                goto done;
            }
            for(int i = 0; i < j->ncomp; i++) {
                j->comp[i].id = (uint8_t)byte_in(j);
                int hv = byte_in(j);
                j->comp[i].h = hv >> 4;
                j->comp[i].v = hv & 15;
                j->comp[i].tq = (uint8_t)(byte_in(j) & 3);
                if(j->comp[i].h < 1 || j->comp[i].h > 4 || j->comp[i].v < 1 || j->comp[i].v > 4)
                    goto done;
            }
            sof = true;
        } else if(m >= 0xC2 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
            r = DnieJ2kErrorUnsupported; // progresivo, sin perdidas o aritmetico
            goto done;
        } else if(m == 0xDD) { // DRI
            j->restart = (uint16_t)word_in(j);
        } else if(m == 0xDA) { // SOS
            if(!sof) goto done;
            int ns = byte_in(j);
            if(ns != j->ncomp) {
                r = DnieJ2kErrorUnsupported; // escaneos no entrelazados
                goto done;
            }
            for(int i = 0; i < ns; i++) {
                int id = byte_in(j), t = byte_in(j);
                if(id != j->comp[i].id || t < 0) goto done;
                j->comp[i].td = (t >> 4) & 3;
                j->comp[i].ta = t & 3;
                if(!j->dc[j->comp[i].td].present || !j->ac[j->comp[i].ta].present) goto done;
            }
            if(!skip(j, 3)) goto done; // Ss, Se, Ah/Al
            r = scan(j, out);
            goto done;
        } else {
            if(!skip(j, len)) goto done; // APPn, COM...
        }
    }

done:
    memset(j, 0, sizeof(*j));
    free(j);
    return r;
}
