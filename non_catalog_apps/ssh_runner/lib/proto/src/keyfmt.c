/* Key file recognition (see keyfmt.h). SPDX-License-Identifier: GPL-3.0-only */
#include "proto/keyfmt.h"

#include <stdio.h>
#include <string.h>

/* ---------------- SHA-256 (FIPS 180-4) ---------------- */

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(KeyfmtSha256* s, const uint8_t* p) {
    uint32_t w[64];
    for(int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
               (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for(int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5],
             g = s->h[6], h = s->h[7];
    for(int i = 0; i < 64; i++) {
        uint32_t t1 =
            h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}

void keyfmt_sha256_init(KeyfmtSha256* s) {
    static const uint32_t iv[8] = {
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19};
    memcpy(s->h, iv, sizeof(iv));
    s->bytes = 0;
    s->fill = 0;
}

void keyfmt_sha256_update(KeyfmtSha256* s, const void* data, size_t len) {
    const uint8_t* p = data;
    s->bytes += len;
    while(len) {
        size_t k = 64u - s->fill;
        if(k > len) k = len;
        memcpy(&s->block[s->fill], p, k);
        s->fill = (uint8_t)(s->fill + k);
        p += k;
        len -= k;
        if(s->fill == 64) {
            sha256_block(s, s->block);
            s->fill = 0;
        }
    }
}

void keyfmt_sha256_final(KeyfmtSha256* s, uint8_t out[32]) {
    uint64_t bits = s->bytes * 8u;
    uint8_t pad = 0x80;
    keyfmt_sha256_update(s, &pad, 1);
    pad = 0;
    while(s->fill != 56)
        keyfmt_sha256_update(s, &pad, 1);
    uint8_t len[8];
    for(int i = 0; i < 8; i++)
        len[i] = (uint8_t)(bits >> (56 - 8 * i));
    keyfmt_sha256_update(s, len, 8);
    for(int i = 0; i < 8; i++) {
        out[4 * i] = (uint8_t)(s->h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(s->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(s->h[i] >> 8);
        out[4 * i + 3] = (uint8_t)s->h[i];
    }
}

/* ---------------- base64 ---------------- */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t keyfmt_b64_encode(const uint8_t* in, size_t len, bool pad, char* out, size_t cap) {
    size_t n = 0;
    for(size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        size_t k = len - i;
        if(k > 1) v |= (uint32_t)in[i + 1] << 8;
        if(k > 2) v |= in[i + 2];
        size_t emit = k >= 3 ? 4 : k + 1;
        size_t want = pad ? 4 : emit;
        if(n + want + 1 > cap) return 0;
        for(size_t j = 0; j < 4; j++) {
            if(j < emit)
                out[n++] = B64[(v >> (18 - 6 * j)) & 63];
            else if(pad)
                out[n++] = '=';
        }
    }
    if(n + 1 > cap) return 0;
    out[n] = '\0';
    return n;
}

static int b64_val(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a' + 26;
    if(c >= '0' && c <= '9') return c - '0' + 52;
    if(c == '+') return 62;
    if(c == '/') return 63;
    return -1;
}

size_t keyfmt_b64_decode(const char* in, size_t len, uint8_t* out, size_t cap) {
    size_t n = 0;
    uint32_t acc = 0;
    int bits = 0;
    for(size_t i = 0; i < len && n < cap; i++) {
        char c = in[i];
        if(c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        int v = b64_val(c);
        if(v < 0) break; /* '=', '-' (END line) or anything else ends the data */
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            out[n++] = (uint8_t)(acc >> bits);
        }
    }
    return n;
}

/* ---------------- SSH wire format ---------------- */

typedef struct {
    const uint8_t* p;
    size_t len;
    bool err;
} Wire;

static uint32_t wire_u32(Wire* w) {
    if(w->err || w->len < 4) {
        w->err = true;
        return 0;
    }
    uint32_t v = (uint32_t)w->p[0] << 24 | (uint32_t)w->p[1] << 16 | (uint32_t)w->p[2] << 8 |
                 w->p[3];
    w->p += 4;
    w->len -= 4;
    return v;
}

static const uint8_t* wire_str(Wire* w, size_t* n) {
    uint32_t k = wire_u32(w);
    if(w->err || k > w->len) {
        w->err = true;
        *n = 0;
        return NULL;
    }
    const uint8_t* s = w->p;
    w->p += k;
    w->len -= k;
    *n = k;
    return s;
}

static bool str_is(const uint8_t* s, size_t n, const char* lit) {
    return s && n == strlen(lit) && memcmp(s, lit, n) == 0;
}

/* Bits of a big-endian unsigned integer, leading zero bytes ignored. */
static uint16_t int_bits(const uint8_t* p, size_t n) {
    while(n && *p == 0) {
        p++;
        n--;
    }
    if(!n) return 0;
    uint16_t bits = (uint16_t)(n * 8u);
    for(unsigned top = *p; !(top & 0x80u); top <<= 1)
        bits--;
    return bits;
}

bool keyfmt_blob_info(const uint8_t* blob, size_t len, KeyInfo* ki) {
    Wire w = {blob, len, false};
    size_t tn = 0;
    const uint8_t* t = wire_str(&w, &tn);
    if(w.err) return false;
    ki->bits = 0;
    if(str_is(t, tn, "ssh-ed25519")) {
        ki->type = KeyTypeEd25519;
        ki->bits = 256;
    } else if(
        str_is(t, tn, "ecdsa-sha2-nistp256") || str_is(t, tn, "ecdsa-sha2-nistp384") ||
        str_is(t, tn, "ecdsa-sha2-nistp521")) {
        ki->type = KeyTypeEcdsa;
        ki->bits = t[tn - 1] == '6' ? 256 : t[tn - 1] == '4' ? 384 : 521;
    } else if(str_is(t, tn, "ssh-rsa")) {
        size_t en = 0, nn = 0;
        (void)wire_str(&w, &en);
        const uint8_t* n = wire_str(&w, &nn);
        if(w.err) return false;
        ki->type = KeyTypeRsa;
        ki->bits = int_bits(n, nn);
    } else if(str_is(t, tn, "ssh-dss")) {
        ki->type = KeyTypeDsa;
    } else {
        ki->type = KeyTypeOther; /* sk-*, *-cert-v01@openssh.com, ... */
    }
    return true;
}

/* ---------------- DER (PEM keys) ----------------
 * Only for the size of an unencrypted PEM key. KEYFMT_NO_DER (the Flipper build, where
 * code costs RAM) leaves it out: the type still comes from the header, and the ESP32
 * checks the size when it parses the key. */
#ifndef KEYFMT_NO_DER

typedef struct {
    const uint8_t* p;
    size_t len;
} Der;

/* Next TLV: returns its tag and sets `in` to the contents; false if malformed or cut. */
static bool der_next(Der* d, uint8_t* tag, Der* in) {
    if(d->len < 2) return false;
    *tag = d->p[0];
    size_t n = d->p[1], hdr = 2;
    if(n & 0x80) {
        size_t k = n & 0x7F;
        if(k == 0 || k > 2 || d->len < 2 + k) return false;
        n = 0;
        for(size_t i = 0; i < k; i++)
            n = n << 8 | d->p[2 + i];
        hdr += k;
    }
    in->p = d->p + hdr;
    /* a key cut by the sniff window still tells its type: keep what is there */
    in->len = d->len - hdr < n ? d->len - hdr : n;
    size_t used = hdr + n > d->len ? d->len : hdr + n;
    d->p += used;
    d->len -= used;
    return true;
}

static bool der_enter(Der* d, uint8_t want) {
    uint8_t tag;
    Der in;
    if(!der_next(d, &tag, &in) || tag != want) return false;
    *d = in;
    return true;
}

static const uint8_t OID_RSA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
static const uint8_t OID_EC[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
static const uint8_t OID_ED25519[] = {0x2b, 0x65, 0x70};
static const uint8_t OID_DSA[] = {0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x01};
static const uint8_t OID_P256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
static const uint8_t OID_P384[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
static const uint8_t OID_P521[] = {0x2b, 0x81, 0x04, 0x00, 0x23};

static bool oid_is(const Der* o, const uint8_t* oid, size_t n) {
    return o->len == n && memcmp(o->p, oid, n) == 0;
}

/* RSAPrivateKey ::= SEQUENCE { version, modulus, ... } */
static void der_rsa(Der d, KeyInfo* ki) {
    ki->type = KeyTypeRsa;
    uint8_t tag;
    Der v, n;
    if(der_enter(&d, 0x30) && der_next(&d, &tag, &v) && tag == 0x02 && der_next(&d, &tag, &n) &&
       tag == 0x02)
        ki->bits = int_bits(n.p, n.len);
}

/* ECPrivateKey ::= SEQUENCE { version 1, privateKey OCTET STRING, ... } */
static void der_ec(Der d, KeyInfo* ki) {
    ki->type = KeyTypeEcdsa;
    uint8_t tag;
    Der v, k;
    if(der_enter(&d, 0x30) && der_next(&d, &tag, &v) && tag == 0x02 && der_next(&d, &tag, &k) &&
       tag == 0x04)
        ki->bits = k.len <= 32 ? 256 : k.len <= 48 ? 384 : 521;
}

/* PrivateKeyInfo ::= SEQUENCE { version, AlgorithmIdentifier, privateKey OCTET STRING } */
static void der_pkcs8(Der d, KeyInfo* ki) {
    uint8_t tag;
    Der v, alg, oid, param, key;
    if(!der_enter(&d, 0x30) || !der_next(&d, &tag, &v) || tag != 0x02) return;
    if(!der_next(&d, &tag, &alg) || tag != 0x30 || !der_next(&alg, &tag, &oid) || tag != 0x06)
        return;
    if(oid_is(&oid, OID_ED25519, sizeof(OID_ED25519))) {
        ki->type = KeyTypeEd25519;
        ki->bits = 256;
    } else if(oid_is(&oid, OID_EC, sizeof(OID_EC))) {
        ki->type = KeyTypeEcdsa;
        if(der_next(&alg, &tag, &param) && tag == 0x06)
            ki->bits = oid_is(&param, OID_P256, sizeof(OID_P256)) ? 256 :
                       oid_is(&param, OID_P384, sizeof(OID_P384)) ? 384 :
                       oid_is(&param, OID_P521, sizeof(OID_P521)) ? 521 :
                                                                    0;
        if(!ki->bits) ki->type = KeyTypeOther; /* a curve SSH does not use */
    } else if(oid_is(&oid, OID_RSA, sizeof(OID_RSA))) {
        if(der_next(&d, &tag, &key) && tag == 0x04) der_rsa(key, ki);
        ki->type = KeyTypeRsa;
    } else if(oid_is(&oid, OID_DSA, sizeof(OID_DSA))) {
        ki->type = KeyTypeDsa;
    } else {
        ki->type = KeyTypeOther;
    }
}

#endif /* KEYFMT_NO_DER */

/* ---------------- sniffing ---------------- */

static const char* find(const char* text, size_t len, const char* lit) {
    size_t n = strlen(lit);
    for(size_t i = 0; i + n <= len; i++)
        if(memcmp(text + i, lit, n) == 0) return text + i;
    return NULL;
}

static bool starts(const char* text, size_t len, const char* lit) {
    size_t n = strlen(lit);
    return len >= n && memcmp(text, lit, n) == 0;
}

/* Body of a PEM block: after the BEGIN line and any "Header: value" lines. */
static const char* pem_body(const char* text, size_t len, size_t* body_len) {
    const char* end = text + len;
    const char* p = memchr(text, '\n', len);
    if(!p) {
        *body_len = 0;
        return end;
    }
    p++;
    while(p < end) {
        const char* nl = memchr(p, '\n', (size_t)(end - p));
        const char* line_end = nl ? nl : end;
        if(!memchr(p, ':', (size_t)(line_end - p))) break;
        p = nl ? nl + 1 : end;
    }
    *body_len = (size_t)(end - p);
    return p;
}

static void
    sniff_openssh(const char* body, size_t blen, KeyInfo* ki, uint8_t* scratch, size_t cap) {
    size_t n = keyfmt_b64_decode(body, blen, scratch, cap);
    static const char magic[] = "openssh-key-v1";
    if(n < sizeof(magic) || memcmp(scratch, magic, sizeof(magic)) != 0)
        return; /* includes the NUL */
    Wire w = {scratch + sizeof(magic), n - sizeof(magic), false};
    size_t cn = 0, kn = 0, on = 0, pn = 0;
    const uint8_t* cipher = wire_str(&w, &cn);
    (void)wire_str(&w, &kn);
    (void)wire_str(&w, &on);
    uint32_t keys = wire_u32(&w);
    if(w.err) return;
    ki->encrypted = !str_is(cipher, cn, "none");
    if(keys != 1) return;
    const uint8_t* pub = wire_str(&w, &pn);
    if(w.err || !keyfmt_blob_info(pub, pn, ki)) return;
    ki->pub = pub;
    ki->pub_len = pn;
}

void keyfmt_sniff(const char* text, size_t len, KeyInfo* ki, uint8_t* scratch, size_t cap) {
    memset(ki, 0, sizeof(*ki));
    while(len && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
        text++;
        len--;
    }
    if(starts(text, len, "PuTTY-User-Key-File-")) {
        ki->fmt = KeyFmtPutty;
        return;
    }
    if(starts(text, len, "---- BEGIN SSH2 PUBLIC KEY") ||
       starts(text, len, "-----BEGIN PUBLIC KEY") ||
       starts(text, len, "-----BEGIN RSA PUBLIC KEY") || starts(text, len, "ssh-") ||
       starts(text, len, "ecdsa-sha2-") || starts(text, len, "sk-")) {
        ki->fmt = KeyFmtPublic;
        return;
    }
    if(!starts(text, len, "-----BEGIN ")) return;
    size_t blen = 0;
    const char* body = pem_body(text, len, &blen);
    const char* nl = memchr(text, '\n', len);
    size_t head_len = nl ? (size_t)(nl - text) : len;
    bool proc_encrypted = find(text, (size_t)(body - text), "ENCRYPTED") != NULL;
#ifndef KEYFMT_NO_DER
    Der der = {scratch, 0};
#endif

    if(find(text, head_len, "BEGIN OPENSSH PRIVATE KEY")) {
        ki->fmt = KeyFmtOpenSsh;
        sniff_openssh(body, blen, ki, scratch, cap);
        return;
    }
    if(find(text, head_len, "BEGIN ENCRYPTED PRIVATE KEY")) {
        ki->fmt = KeyFmtPkcs8Enc;
        ki->encrypted = true;
        return;
    }
#ifndef KEYFMT_NO_DER
    if(!proc_encrypted) der.len = keyfmt_b64_decode(body, blen, scratch, cap);
#define DER(fn) \
    if(der.len) fn(der, ki)
#else
    (void)blen;
#define DER(fn)
#endif
    ki->encrypted = proc_encrypted;
    if(find(text, head_len, "BEGIN RSA PRIVATE KEY")) {
        ki->fmt = KeyFmtPemRsa;
        ki->type = KeyTypeRsa;
        DER(der_rsa);
    } else if(find(text, head_len, "BEGIN EC PRIVATE KEY")) {
        ki->fmt = KeyFmtPemEc;
        ki->type = KeyTypeEcdsa;
        DER(der_ec);
    } else if(find(text, head_len, "BEGIN DSA PRIVATE KEY")) {
        ki->fmt = KeyFmtPemDsa;
        ki->type = KeyTypeDsa;
    } else if(find(text, head_len, "BEGIN PRIVATE KEY")) {
        ki->fmt = KeyFmtPkcs8;
        DER(der_pkcs8);
    }
#undef DER
}

const char* keyfmt_problem(const KeyInfo* ki) {
    switch(ki->fmt) {
    case KeyFmtUnknown:
        return "Not a private key file";
    case KeyFmtPublic:
        return "This is a public key; copy the private key file too";
    case KeyFmtPutty:
        return "PuTTY key: export it as OpenSSH first";
    case KeyFmtPemDsa:
        return "DSA keys are not supported";
    default:
        break;
    }
    if(ki->type == KeyTypeDsa) return "DSA keys are not supported";
    if(ki->type == KeyTypeOther) return "Key type not supported";
    if(ki->type == KeyTypeRsa && ki->bits && ki->bits < 2048)
        return "RSA key too short (needs 2048+ bits)";
    if(ki->fmt == KeyFmtOpenSsh && ki->type == KeyTypeUnknown) return "Damaged OpenSSH key file";
    return NULL;
}

/* ---------------- public keys ---------------- */

bool keyfmt_parse_pub_line(
    const char* line,
    size_t len,
    uint8_t* blob,
    size_t cap,
    size_t* blob_len) {
    const char* sp = memchr(line, ' ', len);
    if(!sp) return false;
    size_t tn = (size_t)(sp - line);
    const char* b = sp + 1;
    size_t rest = len - tn - 1;
    while(rest && *b == ' ') {
        b++;
        rest--;
    }
    size_t bl = 0;
    while(bl < rest && b64_val(b[bl]) >= 0)
        bl++;
    while(bl < rest && b[bl] == '=')
        bl++;
    *blob_len = keyfmt_b64_decode(b, bl, blob, cap);
    /* the blob names the same type as the line, and fits */
    Wire w = {blob, *blob_len, false};
    size_t n = 0;
    const uint8_t* t = wire_str(&w, &n);
    return !w.err && n == tn && memcmp(t, line, tn) == 0 && *blob_len < cap;
}

size_t
    keyfmt_pub_line(const uint8_t* blob, size_t len, const char* comment, char* out, size_t cap) {
    Wire w = {blob, len, false};
    size_t tn = 0;
    const uint8_t* t = wire_str(&w, &tn);
    if(w.err || tn + 2 > cap) return 0;
    memcpy(out, t, tn);
    out[tn] = ' ';
    size_t n = keyfmt_b64_encode(blob, len, true, out + tn + 1, cap - tn - 1);
    if(!n) return 0;
    n += tn + 1;
    if(comment && comment[0]) {
        size_t cn = strlen(comment);
        if(n + 1 + cn + 1 > cap) return 0;
        out[n++] = ' ';
        memcpy(out + n, comment, cn + 1);
        n += cn;
    }
    return n;
}

void keyfmt_fingerprint(const uint8_t* blob, size_t len, char out[KEYFMT_FP_SIZE]) {
    KeyfmtSha256 s;
    uint8_t d[32];
    keyfmt_sha256_init(&s);
    keyfmt_sha256_update(&s, blob, len);
    keyfmt_sha256_final(&s, d);
    memcpy(out, "SHA256:", 7);
    keyfmt_b64_encode(d, sizeof(d), false, out + 7, KEYFMT_FP_SIZE - 7);
}

void keyfmt_file_id(const uint8_t digest[32], char out[KEYFMT_FP_SIZE + 5]) {
    memcpy(out, "file:SHA256:", 12);
    keyfmt_b64_encode(digest, 32, false, out + 12, KEYFMT_FP_SIZE + 5 - 12);
}

void keyfmt_label(const KeyInfo* ki, char out[KEYFMT_LABEL_SIZE]) {
    const char* name = ki->type == KeyTypeEd25519 ? "Ed25519" :
                       ki->type == KeyTypeEcdsa   ? "ECDSA" :
                       ki->type == KeyTypeRsa     ? "RSA" :
                       ki->type == KeyTypeDsa     ? "DSA" :
                                                    "Key";
    if(ki->bits && ki->type == KeyTypeEcdsa)
        snprintf(out, KEYFMT_LABEL_SIZE, "%s P-%u", name, (unsigned)ki->bits);
    else if(ki->bits && ki->type == KeyTypeRsa)
        snprintf(out, KEYFMT_LABEL_SIZE, "%s %u", name, (unsigned)ki->bits);
    else
        snprintf(out, KEYFMT_LABEL_SIZE, "%s", name);
}
