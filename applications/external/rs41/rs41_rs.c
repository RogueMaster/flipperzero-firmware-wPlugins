/*
 * rs41_rs.c — Reed-Solomon (255,231) decoder for RS41 radiosondes.
 *
 * Shortened to (156,132): 24 parity bytes + 132 data bytes per codeword.
 * GF(2^8), primitive polynomial 0x11D (x^8+x^4+x^3+x^2+1), α=2, FCR=0.
 * Can correct up to t=12 symbol errors per codeword.
 *
 * Mathematical convention ("natural polynomial"):
 *   cw[j] is the coefficient of x^j.
 *   Syndrome S_m = Σ_{j=0}^{155} cw[j]·(α^m)^j   (evaluated at root α^m).
 *   Shortening zeros are at positions 156–254 (trailing), so no correction
 *   factor is needed in the syndrome computation.
 *
 * Forney formula (FCR=0):
 *   e_j = α^j · Ω(α^{−j}) / Λ′(α^{−j})
 */

#include "rs41_rs.h"
#include <string.h>
#include <stdbool.h>

/* ── GF(2^8) tables ──────────────────────────────────────────────────────── */
#define GF_POLY  0x11D   /* x^8 + x^4 + x^3 + x^2 + 1                       */
#define NROOTS   24      /* 2t: RS parity bytes per codeword                  */
#define NN       156     /* shortened codeword length = NROOTS + K            */
#define K        132     /* data bytes per codeword                            */
#define RS_T     12      /* max correctable errors per codeword               */

static uint8_t gf_exp[512]; /* gf_exp[i] = α^i; [255..511] mirrors [0..254]  */
static uint8_t gf_log[256]; /* gf_log[v] = i such that α^i = v               */
static bool    gf_ready = false;

static void gf_init(void) {
    if(gf_ready) return;
    uint16_t x = 1;
    for(int i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;
        if(x & 0x100) x ^= GF_POLY;
    }
    for(int i = 255; i < 512; i++)
        gf_exp[i] = gf_exp[i - 255];
    gf_log[0] = 0; /* undefined; never used for 0 operands */
    gf_ready = true;
}

/* All arithmetic checks for zero operands to avoid undefined log lookups. */

static inline uint8_t gf_mul(uint8_t a, uint8_t b) {
    if(a == 0 || b == 0) return 0;
    return gf_exp[(int)gf_log[a] + (int)gf_log[b]];
}

static inline uint8_t gf_div(uint8_t a, uint8_t b) {
    if(a == 0) return 0;
    return gf_exp[(int)gf_log[a] + 255 - (int)gf_log[b]];
}

/* ── Syndrome computation ────────────────────────────────────────────────── */
/*
 * Evaluate S_m = cw(α^m) = Σ cw[j]·(α^m)^j using Horner's method.
 * cw is NN=156 symbols long; trailing shortening zeros contribute nothing.
 */
static void compute_syndromes(const uint8_t* cw, uint8_t* S) {
    for(int m = 0; m < NROOTS; m++) {
        uint8_t root = gf_exp[m]; /* α^m (FCR=0: root for S_m is α^m) */
        uint8_t s    = cw[NN - 1];
        for(int j = NN - 2; j >= 0; j--)
            s = gf_mul(s, root) ^ cw[j];
        S[m] = s;
    }
}

/* ── Berlekamp-Massey ────────────────────────────────────────────────────── */
/*
 * Find the shortest LFSR Λ(x) generating the syndrome sequence S[0..NROOTS-1].
 * Returns the LFSR length L (= number of errors found), or -1 if L > RS_T.
 */
static int berlekamp_massey(const uint8_t* S, uint8_t* Lambda) {
    uint8_t C[NROOTS + 1]; /* current error locator */
    uint8_t B[NROOTS + 1]; /* previous error locator */
    memset(C, 0, sizeof(C));
    memset(B, 0, sizeof(B));
    C[0] = 1;
    B[0] = 1;
    int     L = 0, m = 1;
    uint8_t b = 1;

    for(int r = 0; r < NROOTS; r++) {
        /* Discrepancy d = S[r] + Σ_{i=1}^{L} C[i]·S[r-i] */
        uint8_t d = S[r];
        for(int i = 1; i <= L; i++)
            d ^= gf_mul(C[i], S[r - i]);

        if(d == 0) {
            m++;
        } else if(2 * L <= r) {
            /* Update C, then swap; increase L */
            uint8_t T[NROOTS + 1];
            memcpy(T, C, sizeof(T));
            uint8_t coef = gf_div(d, b);
            for(int j = m; j <= NROOTS; j++)
                C[j] ^= gf_mul(coef, B[j - m]);
            L = r + 1 - L;
            memcpy(B, T, sizeof(B));
            b = d;
            m = 1;
        } else {
            /* Update C only */
            uint8_t coef = gf_div(d, b);
            for(int j = m; j <= NROOTS; j++)
                C[j] ^= gf_mul(coef, B[j - m]);
            m++;
        }
    }

    if(L > RS_T) return -1;
    memcpy(Lambda, C, NROOTS + 1);
    return L;
}

/* ── Chien search ────────────────────────────────────────────────────────── */
/*
 * Find all j ∈ [0,254] where Λ(α^{−j})=0.
 * Valid error positions are j ∈ [0, NN-1]; positions ≥ NN are in the shortening
 * zone and indicate an uncorrectable burst → return -1.
 */
static int chien_search(const uint8_t* Lambda, int L, int* pos) {
    uint8_t reg[RS_T + 1];
    for(int i = 0; i <= L; i++) reg[i] = Lambda[i];

    int count = 0;
    for(int j = 0; j < 255; j++) {
        /* sum = Λ(α^{−j}) = Σ reg[i] */
        uint8_t sum = 0;
        for(int i = 0; i <= L; i++) sum ^= reg[i];

        if(sum == 0) {
            if(j >= NN) return -1; /* root in shortening zone → uncorrectable */
            if(count >= RS_T) return -1;
            pos[count++] = j;
        }

        /* reg[i] *= α^{−i} for next iteration */
        for(int i = 1; i <= L; i++)
            reg[i] = gf_mul(reg[i], gf_exp[255 - i]);
    }
    return (count == L) ? count : -1;
}

/* ── Error evaluator Ω(x) = S(x)·Λ(x) mod x^NROOTS ─────────────────────── */
static void compute_omega(const uint8_t* S, const uint8_t* Lambda, int L, uint8_t* Omega) {
    memset(Omega, 0, NROOTS);
    for(int k = 0; k < NROOTS; k++) {
        int lim = (k < L) ? k : L;
        for(int i = 0; i <= lim; i++)
            Omega[k] ^= gf_mul(S[k - i], Lambda[i]);
    }
}

/* ── Polynomial evaluation ───────────────────────────────────────────────── */
static uint8_t eval_poly(const uint8_t* p, int len, uint8_t v) {
    uint8_t result = 0, pw = 1;
    for(int i = 0; i < len; i++) {
        result ^= gf_mul(p[i], pw);
        pw = gf_mul(pw, v);
    }
    return result;
}

/*
 * Evaluate the formal derivative Λ′(v) in GF(2).
 * Λ′(x) = Σ_{i odd} Λ[i]·x^{i-1}
 *        = Λ[1] + Λ[3]·v + Λ[5]·v² + …
 * pw starts at 1=v^0, advances by one multiplication per two Lambda indices.
 */
static uint8_t eval_lambda_prime(const uint8_t* Lambda, int L, uint8_t v) {
    uint8_t result = 0, pw = 1;
    for(int i = 1; i <= L; i += 2) {
        result ^= gf_mul(Lambda[i], pw);
        pw = gf_mul(pw, v);
    }
    return result;
}

/* ── Correct one codeword in-place ───────────────────────────────────────── */
/*
 * cw[0..NROOTS-1]  : parity symbols
 * cw[NROOTS..NN-1] : data symbols
 * Returns errors corrected (0 if clean), or -1 if uncorrectable.
 */
static int rs_correct_cw(uint8_t* cw) {
    uint8_t S[NROOTS];
    compute_syndromes(cw, S);

    bool all_zero = true;
    for(int i = 0; i < NROOTS; i++) {
        if(S[i]) { all_zero = false; break; }
    }
    if(all_zero) return 0;

    uint8_t Lambda[NROOTS + 1];
    memset(Lambda, 0, sizeof(Lambda));
    int L = berlekamp_massey(S, Lambda);
    if(L < 0) return -1;

    uint8_t Omega[NROOTS];
    compute_omega(S, Lambda, L, Omega);

    int pos[RS_T];
    int count = chien_search(Lambda, L, pos);
    if(count < 0) return -1;

    /* Apply Forney corrections: e_j = α^j · Ω(α^{−j}) / Λ′(α^{−j}) */
    for(int k = 0; k < count; k++) {
        int     j      = pos[k];
        uint8_t Xj_inv = gf_exp[255 - j]; /* α^{−j}; gf_exp[255]=1 for j=0 */
        uint8_t omega_v = eval_poly(Omega, NROOTS, Xj_inv);
        uint8_t lp_v    = eval_lambda_prime(Lambda, L, Xj_inv);
        if(lp_v == 0) return -1; /* degenerate: shouldn't happen */
        uint8_t e = gf_mul(gf_exp[j], gf_div(omega_v, lp_v));
        cw[j] ^= e;
    }
    return count;
}

/* ── Public API ──────────────────────────────────────────────────────────── */
int rs41_rs_correct(uint8_t* frame, int frame_len) {
    if(frame_len < 312) return -1;
    gf_init();

    /*
     * Extract two interleaved codewords from the 312-byte payload.
     * Layout: frame[0..23]=CW1 parity, frame[24..47]=CW2 parity,
     *         frame[48+2i]=CW1 data[i], frame[49+2i]=CW2 data[i]  i=0..131.
     */
    uint8_t cw1[NN], cw2[NN];

    for(int i = 0; i < NROOTS; i++) {
        cw1[i] = frame[i];
        cw2[i] = frame[NROOTS + i];
    }
    for(int i = 0; i < K; i++) {
        cw1[NROOTS + i] = frame[48 + 2 * i];
        cw2[NROOTS + i] = frame[49 + 2 * i];
    }

    int c1 = rs_correct_cw(cw1);
    int c2 = rs_correct_cw(cw2);
    if(c1 < 0 || c2 < 0) return -1;

    /* Write corrections back */
    for(int i = 0; i < NROOTS; i++) {
        frame[i]          = cw1[i];
        frame[NROOTS + i] = cw2[i];
    }
    for(int i = 0; i < K; i++) {
        frame[48 + 2 * i] = cw1[NROOTS + i];
        frame[49 + 2 * i] = cw2[NROOTS + i];
    }

    return c1 + c2;
}
