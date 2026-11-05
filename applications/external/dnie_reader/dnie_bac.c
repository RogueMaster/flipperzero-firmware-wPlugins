#include "dnie_bac.h"
#include "dnie_crypto.h"
#include "dnie_mrz.h"
#include "dnie_hash.h"

#include <string.h>
#include <furi_hal_random.h>

#define SW_OK 0x9000

void dnie_bac_mrz_info(const char* doc_number, const char* birth, const char* expiry, char out[25]) {
    memset(out, '<', 9);
    size_t n = strlen(doc_number);
    if(n > 9) n = 9;
    memcpy(out, doc_number, n);
    out[9] = '0' + dnie_mrz_check_digit(out, 9);
    memcpy(out + 10, birth, 6);
    out[16] = '0' + dnie_mrz_check_digit(out + 10, 6);
    memcpy(out + 17, expiry, 6);
    out[23] = '0' + dnie_mrz_check_digit(out + 17, 6);
    out[24] = '\0';
}

void dnie_bac_pace_password(const char* mrz_info, DniePacePassword* pw) {
    pw->ref = DNIE_PACE_PW_MRZ;
    pw->len = 20;
    uint8_t h[DNIE_HASH_MAX];
    dnie_hash(DnieHashSha1, (const uint8_t*)mrz_info, strlen(mrz_info), h);
    memcpy(pw->secret, h, 20);
    memset(h, 0, sizeof(h));
}

// Relleno ISO 9797-1 metodo 2 (80 00..) para el MAC
static size_t pad8(uint8_t* buf, size_t len) {
    buf[len++] = 0x80;
    while(len % 8)
        buf[len++] = 0x00;
    return len;
}

DnieBacResult
    dnie_bac_establish(DnieTransport* poller, const char* mrz_info, DnieSm* sm, uint16_t* sw) {
    DnieBacResult r = DnieBacErrorCrypto;
    BitBuffer* tx = bit_buffer_alloc(64);
    BitBuffer* rx = bit_buffer_alloc(80);
    uint8_t seed[DNIE_HASH_MAX], k_enc[16], k_mac[16];
    uint8_t rnd_ic[8], rnd_ifd[8], k_ifd[16], s[32], buf[48], mac[8], dec[32];

    do {
        // Claves de acceso a partir de la MRZ
        if(!dnie_hash(DnieHashSha1, (const uint8_t*)mrz_info, strlen(mrz_info), seed)) break;
        if(!dnie_kdf_3des(seed, 16, 1, k_enc) || !dnie_kdf_3des(seed, 16, 2, k_mac)) break;

        // GET CHALLENGE -> RND.IC
        static const uint8_t gc[] = {0x00, 0x84, 0x00, 0x00, 0x08};
        if(!dnie_card_apdu(poller, gc, sizeof(gc), tx, rx, sw)) {
            r = DnieBacErrorTransport;
            break;
        }
        if(*sw != SW_OK || bit_buffer_get_size_bytes(rx) != 10) {
            r = DnieBacErrorChallenge;
            break;
        }
        memcpy(rnd_ic, bit_buffer_get_data(rx), 8);

        // S = RND.IFD || RND.IC || K.IFD, cifrado y con MAC
        furi_hal_random_fill_buf(rnd_ifd, 8);
        furi_hal_random_fill_buf(k_ifd, 16);
        memcpy(s, rnd_ifd, 8);
        memcpy(s + 8, rnd_ic, 8);
        memcpy(s + 16, k_ifd, 16);
        uint8_t apdu[5 + 40 + 1] = {0x00, 0x82, 0x00, 0x00, 0x28};
        if(!dnie_3des_cbc_iv0(k_enc, true, s, 32, apdu + 5)) break;
        memcpy(buf, apdu + 5, 32);
        if(!dnie_retail_mac(k_mac, buf, pad8(buf, 32), apdu + 37)) break;
        apdu[45] = 0x28;

        // MUTUAL AUTHENTICATE
        if(!dnie_card_apdu(poller, apdu, sizeof(apdu), tx, rx, sw)) {
            r = DnieBacErrorTransport;
            break;
        }
        if(*sw != SW_OK) {
            r = DnieBacErrorAuth;
            break;
        }
        if(bit_buffer_get_size_bytes(rx) != 42) {
            r = DnieBacErrorChip;
            break;
        }
        const uint8_t* resp = bit_buffer_get_data(rx);
        memcpy(buf, resp, 32);
        if(!dnie_retail_mac(k_mac, buf, pad8(buf, 32), mac)) break;
        if(memcmp(mac, resp + 32, 8) != 0) {
            r = DnieBacErrorChip;
            break;
        }
        // R = RND.IC || RND.IFD || K.IC
        if(!dnie_3des_cbc_iv0(k_enc, false, resp, 32, dec)) break;
        if(memcmp(dec, rnd_ic, 8) != 0 || memcmp(dec + 8, rnd_ifd, 8) != 0) {
            r = DnieBacErrorChip;
            break;
        }

        // Claves de sesion: K.IFD xor K.IC.  SSC = RND.IC[4..8] || RND.IFD[4..8]
        for(int i = 0; i < 16; i++)
            seed[i] = k_ifd[i] ^ dec[16 + i];
        uint8_t ks_enc[16], ks_mac[16], ssc[8];
        if(!dnie_kdf_3des(seed, 16, 1, ks_enc) || !dnie_kdf_3des(seed, 16, 2, ks_mac)) break;
        memcpy(ssc, rnd_ic + 4, 4);
        memcpy(ssc + 4, rnd_ifd + 4, 4);
        dnie_sm_init_3des(sm, ks_enc, ks_mac, ssc);
        memset(ks_enc, 0, 16);
        memset(ks_mac, 0, 16);
        r = DnieBacOk;
    } while(0);

    memset(seed, 0, sizeof(seed));
    memset(k_enc, 0, sizeof(k_enc));
    memset(k_mac, 0, sizeof(k_mac));
    memset(k_ifd, 0, sizeof(k_ifd));
    memset(s, 0, sizeof(s));
    memset(dec, 0, sizeof(dec));
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return r;
}

const char* dnie_bac_result_str(DnieBacResult r) {
    switch(r) {
    case DnieBacOk:
        return "OK";
    case DnieBacErrorTransport:
        return "Radio failure";
    case DnieBacErrorChallenge:
        return "GET CHALLENGE\nrejected";
    case DnieBacErrorAuth:
        return "MRZ data\nincorrect";
    case DnieBacErrorChip:
        return "Chip response\ninvalid";
    default:
        return "Crypto error";
    }
}
