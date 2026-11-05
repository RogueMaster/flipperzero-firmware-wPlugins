#include "dnie_sm.h"
#include "dnie_crypto.h"

#include <string.h>

#define SM_MAC_LEN  8
#define SM_MAX_DATA 240 // tamano maximo de datos que protegemos/aceptamos

static void ssc_increment(DnieSm* sm) {
    for(int i = sm->block - 1; i >= 0; i--) {
        if(++sm->ssc[i] != 0) break;
    }
}

// Relleno ISO/IEC 9797-1 metodo 2: 0x80 y ceros hasta multiplo del bloque. Devuelve nueva longitud.
static size_t pad_m2(uint8_t* buf, size_t len, size_t max, size_t block) {
    size_t padded = (len / block + 1) * block;
    if(padded > max) return 0;
    buf[len] = 0x80;
    memset(&buf[len + 1], 0, padded - len - 1);
    return padded;
}

static bool unpad_m2(const uint8_t* buf, size_t len, size_t* out_len) {
    while(len > 0 && buf[len - 1] == 0x00)
        len--;
    if(len == 0 || buf[len - 1] != 0x80) return false;
    *out_len = len - 1;
    return true;
}

// MAC de 8 bytes sobre pad(SSC || m): AES-CMAC truncado o MAC retail 3DES
static bool sm_mac(const DnieSm* sm, const uint8_t* m, size_t m_len, uint8_t mac[SM_MAC_LEN]) {
    uint8_t n[DNIE_SM_BLOCK + SM_MAX_DATA + 64];
    if(sm->block + m_len + sm->block > sizeof(n)) return false;
    memcpy(n, sm->ssc, sm->block);
    memcpy(&n[sm->block], m, m_len);
    size_t n_len = pad_m2(n, sm->block + m_len, sizeof(n), sm->block);
    if(n_len == 0) return false;
    if(sm->cipher == DnieSmCipher3Des) return dnie_retail_mac(sm->ks_mac, n, n_len, mac);
    uint8_t full[16];
    if(!dnie_aes128_cmac(sm->ks_mac, n, n_len, full)) return false;
    memcpy(mac, full, SM_MAC_LEN);
    return true;
}

// Cifrado/descifrado de DO87 segun el modo
static bool sm_crypt(const DnieSm* sm, bool encrypt, const uint8_t* in, size_t len, uint8_t* out) {
    if(sm->cipher == DnieSmCipher3Des) return dnie_3des_cbc_iv0(sm->ks_enc, encrypt, in, len, out);
    uint8_t iv[DNIE_SM_BLOCK];
    return dnie_aes128_ecb_encrypt(sm->ks_enc, sm->ssc, iv) &&
           dnie_aes128_cbc(sm->ks_enc, iv, encrypt, in, len, out);
}

// Escribe tag + longitud BER (corta o 0x81). Devuelve bytes escritos.
static size_t put_tl(uint8_t* p, uint8_t tag, size_t len) {
    p[0] = tag;
    if(len < 0x80) {
        p[1] = (uint8_t)len;
        return 2;
    }
    p[1] = 0x81;
    p[2] = (uint8_t)len;
    return 3;
}

void dnie_sm_init(DnieSm* sm, const uint8_t ks_enc[16], const uint8_t ks_mac[16]) {
    sm->cipher = DnieSmCipherAes;
    sm->block = 16;
    memcpy(sm->ks_enc, ks_enc, 16);
    memcpy(sm->ks_mac, ks_mac, 16);
    memset(sm->ssc, 0, DNIE_SM_BLOCK); // AES: SSC = 0 tras PACE
}

void dnie_sm_init_3des(
    DnieSm* sm,
    const uint8_t ks_enc[16],
    const uint8_t ks_mac[16],
    const uint8_t* ssc8) {
    sm->cipher = DnieSmCipher3Des;
    sm->block = 8;
    memcpy(sm->ks_enc, ks_enc, 16);
    memcpy(sm->ks_mac, ks_mac, 16);
    memset(sm->ssc, 0, DNIE_SM_BLOCK);
    if(ssc8) memcpy(sm->ssc, ssc8, 8);
}

void dnie_sm_clear(DnieSm* sm) {
    memset(sm, 0, sizeof(*sm));
}

size_t dnie_sm_wrap(
    DnieSm* sm,
    const uint8_t hdr[4],
    const uint8_t* data,
    size_t len,
    int le,
    uint8_t* out,
    size_t out_max) {
    if(len > SM_MAX_DATA || le > 256) return 0;
    ssc_increment(sm);

    uint8_t dos[3 + 1 + SM_MAX_DATA + DNIE_SM_BLOCK + 3]; // DO87 + DO97
    size_t dos_len = 0;

    // DO87: 01 || cifrado(pad(data))
    if(len > 0) {
        uint8_t padded[SM_MAX_DATA + DNIE_SM_BLOCK];
        memcpy(padded, data, len);
        size_t plen = pad_m2(padded, len, sizeof(padded), sm->block);
        if(plen == 0) return 0;
        dos_len += put_tl(&dos[dos_len], 0x87, plen + 1);
        dos[dos_len++] = 0x01; // indicador de relleno
        if(!sm_crypt(sm, true, padded, plen, &dos[dos_len])) return 0;
        dos_len += plen;
    }
    // DO97: Le
    if(le >= 0) {
        dos[dos_len++] = 0x97;
        dos[dos_len++] = 0x01;
        dos[dos_len++] = (uint8_t)(le == 256 ? 0 : le);
    }

    // MAC sobre pad(cabecera con CLA|0x0C) || DO87 || DO97
    uint8_t m[DNIE_SM_BLOCK + sizeof(dos)];
    memcpy(m, hdr, 4);
    m[0] |= 0x0C;
    pad_m2(m, 4, sm->block, sm->block); // cabecera rellenada a un bloque
    memcpy(&m[sm->block], dos, dos_len);
    uint8_t mac[SM_MAC_LEN];
    if(!sm_mac(sm, m, sm->block + dos_len, mac)) return 0;

    // APDU: CLA' INS P1 P2 Lc DO87 DO97 DO8E 00
    size_t body = dos_len + 2 + SM_MAC_LEN;
    if(body > 255 || 5 + body + 1 > out_max) return 0;
    size_t i = 0;
    memcpy(out, hdr, 4);
    out[0] |= 0x0C;
    i = 4;
    out[i++] = (uint8_t)body;
    memcpy(&out[i], dos, dos_len);
    i += dos_len;
    out[i++] = 0x8E;
    out[i++] = SM_MAC_LEN;
    memcpy(&out[i], mac, SM_MAC_LEN);
    i += SM_MAC_LEN;
    out[i++] = 0x00; // Le
    return i;
}

DnieSmResult dnie_sm_unwrap(
    DnieSm* sm,
    const uint8_t* resp,
    size_t resp_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    uint16_t* sw) {
    *out_len = 0;
    ssc_increment(sm); // la respuesta tambien consume un valor del contador
    if(resp_len < 2) return DnieSmErrorFormat;

    size_t n = resp_len - 2;
    uint16_t plain_sw = (uint16_t)(resp[n] << 8 | resp[n + 1]);
    if(n == 0) {
        // Solo SW: el chip no protegio la respuesta (error de SM o similar)
        *sw = plain_sw;
        return DnieSmErrorPlainStatus;
    }

    // Recorremos los DO: 87 (datos cifrados), 99 (SW), 8E (MAC)
    const uint8_t* do87 = NULL;
    size_t do87_total = 0, enc_off = 0, enc_len = 0;
    const uint8_t* do99 = NULL;
    const uint8_t* mac = NULL;
    size_t pos = 0;
    while(pos + 2 <= n) {
        uint8_t tag = resp[pos];
        size_t hl = 2, l = resp[pos + 1];
        if(l == 0x81) {
            if(pos + 3 > n) return DnieSmErrorFormat;
            l = resp[pos + 2];
            hl = 3;
        } else if(l > 0x81) {
            return DnieSmErrorFormat;
        }
        if(pos + hl + l > n) return DnieSmErrorFormat;
        const uint8_t* v = &resp[pos + hl];
        if(tag == 0x87) {
            if(l < 1u + sm->block || v[0] != 0x01 || (l - 1) % sm->block != 0)
                return DnieSmErrorFormat;
            do87 = &resp[pos];
            do87_total = hl + l;
            enc_off = pos + hl + 1;
            enc_len = l - 1;
        } else if(tag == 0x99) {
            if(l != 2) return DnieSmErrorFormat;
            do99 = &resp[pos];
        } else if(tag == 0x8E) {
            if(l != SM_MAC_LEN) return DnieSmErrorFormat;
            mac = v;
        }
        pos += hl + l;
    }
    if(!do99 || !mac) return DnieSmErrorFormat;

    // Verificar MAC sobre DO87 || DO99
    uint8_t k[3 + SM_MAX_DATA + DNIE_SM_BLOCK + 4];
    size_t k_len = 0;
    if(do87) {
        if(do87_total > sizeof(k) - 4) return DnieSmErrorBuffer;
        memcpy(k, do87, do87_total);
        k_len = do87_total;
    }
    memcpy(&k[k_len], do99, 4);
    k_len += 4;
    uint8_t expected[SM_MAC_LEN];
    if(!sm_mac(sm, k, k_len, expected)) return DnieSmErrorCrypto;
    uint8_t diff = 0;
    for(int i = 0; i < SM_MAC_LEN; i++)
        diff |= (uint8_t)(expected[i] ^ mac[i]);
    if(diff != 0) return DnieSmErrorMac;

    *sw = (uint16_t)(do99[2] << 8 | do99[3]);

    // Descifrar DO87
    if(do87) {
        uint8_t plain[SM_MAX_DATA + DNIE_SM_BLOCK];
        if(enc_len > sizeof(plain)) return DnieSmErrorBuffer;
        if(!sm_crypt(sm, false, &resp[enc_off], enc_len, plain)) return DnieSmErrorCrypto;
        size_t plen;
        if(!unpad_m2(plain, enc_len, &plen)) return DnieSmErrorPadding;
        if(plen > out_max) return DnieSmErrorBuffer;
        memcpy(out, plain, plen);
        *out_len = plen;
        memset(plain, 0, sizeof(plain));
    }
    return DnieSmOk;
}

const char* dnie_sm_result_str(DnieSmResult result) {
    switch(result) {
    case DnieSmOk:
        return "OK";
    case DnieSmErrorMac:
        return "Response MAC\nincorrect";
    case DnieSmErrorFormat:
        return "SM response\nmalformed";
    case DnieSmErrorPadding:
        return "SM padding\nincorrect";
    case DnieSmErrorPlainStatus:
        return "Chip rejected\nthe SM command";
    case DnieSmErrorBuffer:
        return "SM response\ntoo large";
    case DnieSmErrorCrypto:
        return "SM crypto error";
    }
    return "Unknown SM error";
}
