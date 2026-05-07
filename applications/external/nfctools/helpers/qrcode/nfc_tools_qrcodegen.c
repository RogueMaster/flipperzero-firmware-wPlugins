// Minimal QR code generator – byte mode, ECC-L, versions 1-7.
// Based on the QR code specification (ISO/IEC 18004:2015).
// Only single-block versions (1-7 ECC-L) are supported.

#include "nfc_tools_qrcodegen.h"
#include <string.h>
#include <stdlib.h>

// ── ECC-L parameters for versions 1-7 ────────────────────────────────────────
// Data codewords = total codewords − ECC codewords per block.
// All versions 1-7 ECC-L have exactly one block.

// Total codewords in the data module area (data + ECC), version 1-7
static const uint16_t qr_total_cw[8] = { 0, 26, 44, 70, 100, 134, 172, 196 };

// ECC codewords per block for ECC-L, version 1-7
static const uint8_t qr_ecc_cw[8]    = { 0,  7, 10, 15,  20,  26,  18,  20 };

// Data codewords = total - ECC, version 1-7
static const uint8_t qr_data_cw[8]   = { 0, 19, 34, 55,  80, 108, 154, 176 };

// Alignment pattern center coordinate for versions 2-7
// (version 1 has none; versions 2-6 have one at coord; version 7 has three)
static const uint8_t qr_align_coord[8] = { 0, 0, 18, 22, 26, 30, 34, 22 };

// Remainder bits to append after codewords (0 for v1-v6, 0 for v7 also)
// v7 actually has 0 remainder bits
static const uint8_t qr_remainder[8] = { 0, 0, 7, 7, 7, 7, 7, 0 };

// Side length of QR code for version n
static inline int qr_side(int ver) { return ver * 4 + 17; }

// ── GF(256) tables ────────────────────────────────────────────────────────────
// Primitive polynomial: x^8 + x^4 + x^3 + x^2 + 1  (0x11D)

static uint8_t gf_exp[512]; // gf_exp[i] = α^i, extended to 512
static uint8_t gf_log[256]; // gf_log[x] = log_α(x)

static void gf_init(void) {
    uint32_t x = 1;
    for(int i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;
        if(x & 0x100) x ^= 0x11D;
    }
    for(int i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
    gf_log[0] = 0; // undefined but set to 0 to avoid ub
}

static inline uint8_t gf_mul(uint8_t a, uint8_t b) {
    if(!a || !b) return 0;
    return gf_exp[(int)gf_log[a] + (int)gf_log[b]];
}

// ── Reed-Solomon ──────────────────────────────────────────────────────────────

// Compute the generator polynomial for nEcc ECC codewords.
// gen[0] is the highest-degree term (degree nEcc-1), gen[nEcc-1] is the constant term.
static void rs_divisor(int nEcc, uint8_t gen[]) {
    memset(gen, 0, (size_t)nEcc);
    gen[nEcc - 1] = 1; // start: monomial x^0

    uint8_t root = 1; // root = α^0, then α^1, …
    for(int i = 0; i < nEcc; i++) {
        // multiply current polynomial by (x - root)
        for(int j = 0; j < nEcc - 1; j++)
            gen[j] = gf_mul(gen[j], root) ^ gen[j + 1];
        gen[nEcc - 1] = gf_mul(gen[nEcc - 1], root);
        root = gf_mul(root, 0x02); // next root: α * 2 = α^(i+1)
    }
}

// Compute nEcc remainder bytes for dataLen data bytes.
static void rs_remainder(
    const uint8_t* data, int dataLen,
    const uint8_t* gen, int nEcc,
    uint8_t* ecc)
{
    memset(ecc, 0, (size_t)nEcc);
    for(int i = 0; i < dataLen; i++) {
        uint8_t factor = data[i] ^ ecc[0];
        memmove(ecc, ecc + 1, (size_t)(nEcc - 1));
        ecc[nEcc - 1] = 0;
        for(int j = 0; j < nEcc; j++)
            ecc[j] ^= gf_mul(gen[j], factor);
    }
}

// ── Bit-stream builder ────────────────────────────────────────────────────────

typedef struct { uint8_t* buf; int len; } BS;

static void bs_append(BS* b, uint32_t val, int nbits) {
    for(int i = nbits - 1; i >= 0; i--) {
        int bit  = (val >> i) & 1;
        int byte = b->len / 8;
        int pos  = 7 - (b->len % 8);
        if(bit) b->buf[byte] |= (uint8_t)(1u << pos);
        b->len++;
    }
}

// ── QR matrix helpers ─────────────────────────────────────────────────────────

// Module buffer: byte 0 = version (reserved), bytes 1+ = rows packed MSB-first.
// We use  byte = 1 + i/8  so that module data NEVER aliases byte 0.
static inline void qr_set(uint8_t* qr, int sz, int x, int y, bool v) {
    int i    = y * sz + x;          // bit index within module area
    int byte = 1 + i / 8;           // byte 0 is the version byte – skip it
    int bit  = 7 - (i % 8);
    if(v) qr[byte] |=  (uint8_t)(1u << bit);
    else  qr[byte] &= ~(uint8_t)(1u << bit);
}
static inline bool qr_get(const uint8_t* qr, int sz, int x, int y) {
    int i = y * sz + x;
    return (qr[1 + i / 8] >> (7 - (i % 8))) & 1;
}

// Function-module bitmap (stored in tempBuffer, not version-indexed)
static inline void fn_set(uint8_t* fn, int sz, int x, int y) {
    int i = y * sz + x;
    fn[i / 8] |= (uint8_t)(1u << (i % 8));
}
static inline bool fn_get(const uint8_t* fn, int sz, int x, int y) {
    int i = y * sz + x;
    return (fn[i / 8] >> (i % 8)) & 1;
}

// ── Pattern drawing ───────────────────────────────────────────────────────────

// 7×7 finder pattern centred at (cx=x+3, cy=y+3) with 1-module separator.
static void draw_finder(uint8_t* qr, uint8_t* fn, int sz, int ox, int oy) {
    for(int dy = -1; dy <= 7; dy++) {
        for(int dx = -1; dx <= 7; dx++) {
            int px = ox + dx, py = oy + dy;
            if(px < 0 || px >= sz || py < 0 || py >= sz) continue;
            bool dark = (dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6) &&
                        (dx == 0 || dx == 6 || dy == 0 || dy == 6 ||
                         (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4));
            qr_set(qr, sz, px, py, dark);
            fn_set(fn, sz, px, py);
        }
    }
}

// 5×5 alignment pattern centred at (cx, cy).
static void draw_align(uint8_t* qr, uint8_t* fn, int sz, int cx, int cy) {
    for(int dy = -2; dy <= 2; dy++) {
        for(int dx = -2; dx <= 2; dx++) {
            bool dark = (dx == -2 || dx == 2 || dy == -2 || dy == 2 || (dx == 0 && dy == 0));
            qr_set(qr, sz, cx + dx, cy + dy, dark);
            fn_set(fn, sz, cx + dx, cy + dy);
        }
    }
}

// Timing strips (row 6 and column 6).
static void draw_timing(uint8_t* qr, uint8_t* fn, int sz) {
    for(int i = 8; i < sz - 8; i++) {
        bool dark = (i % 2 == 0);
        qr_set(qr, sz, i, 6, dark);  fn_set(fn, sz, i, 6);
        qr_set(qr, sz, 6, i, dark);  fn_set(fn, sz, 6, i);
    }
}

// ── Format information ────────────────────────────────────────────────────────
// ECC-L = 0b01; BCH generator: x^10+x^8+x^5+x^4+x^2+x+1 = 0x537
// Mask constant: 0x5412

static uint16_t fmt_word(int mask) {
    uint32_t data = (uint32_t)((0x01u << 3) | (unsigned)mask); // 5-bit data
    // BCH: polynomial long division over GF(2)
    uint32_t rem = data << 10;
    for(int i = 14; i >= 10; i--) {
        if((rem >> i) & 1u) rem ^= (0x537u << (unsigned)(i - 10));
    }
    return (uint16_t)(((data << 10) | rem) ^ 0x5412u);
}

static void draw_format(uint8_t* qr, uint8_t* fn, int sz, int mask) {
    uint16_t fw = fmt_word(mask);

    // ── Copy 1 : around top-left finder ──────────────────────────────────────
    // Bits 0-5 at (x=8, y=0..5)
    for(int i = 0; i <= 5; i++) {
        bool d = (fw >> i) & 1;
        qr_set(qr, sz, 8, i, d);  fn_set(fn, sz, 8, i);
    }
    // Bit 6 at (x=8, y=7)  — y=6 is timing
    { bool d = (fw >> 6) & 1; qr_set(qr, sz, 8, 7, d); fn_set(fn, sz, 8, 7); }
    // Bit 7 at (x=8, y=8)
    { bool d = (fw >> 7) & 1; qr_set(qr, sz, 8, 8, d); fn_set(fn, sz, 8, 8); }
    // Bit 8 at (x=7, y=8)  — x=6 is timing
    { bool d = (fw >> 8) & 1; qr_set(qr, sz, 7, 8, d); fn_set(fn, sz, 7, 8); }
    // Bits 9-14 at (x=5..0, y=8)
    for(int i = 9; i <= 14; i++) {
        bool d = (fw >> i) & 1;
        qr_set(qr, sz, 14 - i, 8, d);  fn_set(fn, sz, 14 - i, 8);
    }

    // ── Copy 2 : top-right + bottom-left ─────────────────────────────────────
    // Bits 0-7 at (x=sz-1..sz-8, y=8)
    for(int i = 0; i <= 7; i++) {
        bool d = (fw >> i) & 1;
        qr_set(qr, sz, sz - 1 - i, 8, d);  fn_set(fn, sz, sz - 1 - i, 8);
    }
    // Dark module always at (x=8, y=sz-8)
    qr_set(qr, sz, 8, sz - 8, true);  fn_set(fn, sz, 8, sz - 8);
    // Bits 8-14 at (x=8, y=sz-7..sz-1)
    for(int i = 8; i <= 14; i++) {
        bool d = (fw >> i) & 1;
        qr_set(qr, sz, 8, sz - 15 + i, d);  fn_set(fn, sz, 8, sz - 15 + i);
    }
}

// ── Data placement ────────────────────────────────────────────────────────────

static void place_data(
    uint8_t* qr,
    const uint8_t* fn,
    int sz,
    const uint8_t* cw,  // interleaved codewords (data + ECC)
    int nBits,          // total bits to place
    int mask)
{
    int bitIdx = 0;
    for(int right = sz - 1; right >= 1; right -= 2) {
        if(right == 6) right = 5; // skip timing column
        for(int vert = 0; vert < sz; vert++) {
            for(int j = 0; j < 2; j++) {
                int x = right - j;
                bool upward = ((right + 1) & 2) == 0;
                int y = upward ? (sz - 1 - vert) : vert;
                if(fn_get(fn, sz, x, y)) continue;

                bool bit = (bitIdx < nBits) ?
                    ((cw[bitIdx / 8] >> (7 - (bitIdx % 8))) & 1) : false;
                bitIdx++;

                // Apply mask
                bool inv = false;
                switch(mask) {
                case 0: inv = (y + x) % 2 == 0; break;
                case 1: inv = y % 2 == 0; break;
                case 2: inv = x % 3 == 0; break;
                case 3: inv = (y + x) % 3 == 0; break;
                case 4: inv = (y / 2 + x / 3) % 2 == 0; break;
                case 5: inv = (y * x) % 2 + (y * x) % 3 == 0; break;
                case 6: inv = ((y * x) % 2 + (y * x) % 3) % 2 == 0; break;
                case 7: inv = ((y + x) % 2 + (y * x) % 3) % 2 == 0; break;
                }
                qr_set(qr, sz, x, y, bit ^ inv);
            }
        }
    }
}

// ── Penalty score ─────────────────────────────────────────────────────────────

static int penalty(const uint8_t* qr, int sz) {
    int score = 0;

    // Rule 1: 5+ in a row
    for(int y = 0; y < sz; y++) {
        int run = 1;
        bool cur = qr_get(qr, sz, 0, y);
        for(int x = 1; x < sz; x++) {
            bool m = qr_get(qr, sz, x, y);
            if(m == cur) { run++; if(run == 5) score += 3; else if(run > 5) score++; }
            else { cur = m; run = 1; }
        }
    }
    for(int x = 0; x < sz; x++) {
        int run = 1;
        bool cur = qr_get(qr, sz, x, 0);
        for(int y = 1; y < sz; y++) {
            bool m = qr_get(qr, sz, x, y);
            if(m == cur) { run++; if(run == 5) score += 3; else if(run > 5) score++; }
            else { cur = m; run = 1; }
        }
    }

    // Rule 2: 2×2 blocks
    for(int y = 0; y < sz - 1; y++) for(int x = 0; x < sz - 1; x++) {
        bool m = qr_get(qr, sz, x, y);
        if(m == qr_get(qr, sz, x+1, y) &&
           m == qr_get(qr, sz, x,   y+1) &&
           m == qr_get(qr, sz, x+1, y+1)) score += 3;
    }

    // Rule 4: dark module ratio
    int dark = 0;
    for(int y = 0; y < sz; y++) for(int x = 0; x < sz; x++)
        if(qr_get(qr, sz, x, y)) dark++;
    int pct = dark * 100 / (sz * sz);
    score += (abs(pct - 50) / 5) * 10;

    return score;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool nfc_tools_qr_encode(
    const char* text,
    uint8_t     tempBuffer[QRCODEGEN_BUF_MAX],
    uint8_t     qrcode[QRCODEGEN_BUF_MAX])
{
    gf_init();

    size_t textLen = strlen(text);
    if(textLen == 0) return false;

    // Select minimum version (ECC-L, byte mode)
    // Byte mode: 4 (mode) + 8 (count) + textLen*8 (data) bits → needs ceil((12+textLen*8)/8) = textLen+2 bytes min
    int version = 0;
    for(int v = 1; v <= 7; v++) {
        if((size_t)qr_data_cw[v] >= textLen + 2) { version = v; break; }
    }
    if(version == 0) return false;

    int sz       = qr_side(version);
    int nData    = qr_data_cw[version];    // data codewords
    int nEcc     = qr_ecc_cw[version];     // ECC codewords
    int nTotal   = qr_total_cw[version];   // nData + nEcc
    int remBits  = qr_remainder[version];  // remainder bits (0 for v1-v7)
    (void)remBits;

    // ── Build data codeword stream ────────────────────────────────────────────
    static uint8_t data[176]; // max nData for v7
    memset(data, 0, (size_t)nData);
    BS bs = { data, 0 };
    bs_append(&bs, 0x04, 4);            // byte mode
    bs_append(&bs, (uint32_t)textLen, 8); // character count (8 bits for v1-v9)
    for(size_t i = 0; i < textLen; i++)
        bs_append(&bs, (uint8_t)text[i], 8);
    // Terminator (up to 4 zero bits)
    int spare = nData * 8 - bs.len;
    bs_append(&bs, 0, spare < 4 ? spare : 4);
    // Pad to byte boundary
    while(bs.len % 8) bs_append(&bs, 0, 1);
    // Pad codewords
    static const uint8_t pad_bytes[2] = { 0xEC, 0x11 };
    for(int pi = 0; bs.len < nData * 8; pi = (pi + 1) % 2)
        bs_append(&bs, pad_bytes[pi], 8);

    // ── Compute ECC ───────────────────────────────────────────────────────────
    static uint8_t gen[30];
    static uint8_t ecc[30];
    rs_divisor(nEcc, gen);
    rs_remainder(data, nData, gen, nEcc, ecc);

    // ── Assemble codeword stream into tempBuffer ──────────────────────────────
    memcpy(tempBuffer, data, (size_t)nData);
    memcpy(tempBuffer + nData, ecc, (size_t)nEcc);
    // (tempBuffer now holds nTotal bytes = nData + nEcc)

    // ── Prepare function-module bitmap (stored in qrcode temporarily) ─────────
    // We use a separate static buffer for fn map
    static uint8_t fn_buf[QRCODEGEN_BUF_MAX];

    // ── Build the QR matrix for each mask, keep best ──────────────────────────
    static uint8_t best[QRCODEGEN_BUF_MAX];
    int best_score = INT32_MAX;

    for(int m = 0; m < 8; m++) {
        memset(qrcode, 0, QRCODEGEN_BUF_MAX);
        memset(fn_buf,  0, QRCODEGEN_BUF_MAX);
        qrcode[0] = (uint8_t)version;

        // Function patterns
        draw_finder(qrcode, fn_buf, sz, 0, 0);           // top-left
        draw_finder(qrcode, fn_buf, sz, sz - 7, 0);      // top-right
        draw_finder(qrcode, fn_buf, sz, 0, sz - 7);      // bottom-left
        draw_timing(qrcode, fn_buf, sz);

        if(version >= 2) {
            int ac = qr_align_coord[version];
            if(version <= 6) {
                draw_align(qrcode, fn_buf, sz, ac, ac);
            } else {
                // v7: alignment patterns at (6,22), (22,6), (22,22)
                draw_align(qrcode, fn_buf, sz,  6, 22);
                draw_align(qrcode, fn_buf, sz, 22,  6);
                draw_align(qrcode, fn_buf, sz, 22, 22);
            }
        }

        draw_format(qrcode, fn_buf, sz, m);

        place_data(qrcode, fn_buf, sz, tempBuffer, nTotal * 8, m);

        int sc = penalty(qrcode, sz);
        if(sc < best_score) { best_score = sc; memcpy(best, qrcode, QRCODEGEN_BUF_MAX); }
    }

    memcpy(qrcode, best, QRCODEGEN_BUF_MAX);
    return true;
}

int nfc_tools_qr_size(const uint8_t qrcode[QRCODEGEN_BUF_MAX]) {
    return qr_side((int)qrcode[0]);
}

bool nfc_tools_qr_module(const uint8_t qrcode[QRCODEGEN_BUF_MAX], int x, int y) {
    int sz = nfc_tools_qr_size(qrcode);
    if(x < 0 || x >= sz || y < 0 || y >= sz) return false;
    return qr_get(qrcode, sz, x, y);
}
