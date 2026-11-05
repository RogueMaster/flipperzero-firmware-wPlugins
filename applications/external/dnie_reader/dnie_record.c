#include "dnie_record.h"

#include <string.h>
#include <mbedtls/sha256.h>

static const uint8_t MAGIC[4] = {'D', 'N', 'I', 'E'};

static bool sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    return mbedtls_sha256(data, len, out, 0) == 0;
}

static size_t safe_strlen(const char* s, size_t max) {
    size_t n = 0;
    while(n < max && s[n])
        n++;
    return n;
}

size_t dnie_record_pack(const DnieRecord* rec, uint8_t* out, size_t max) {
    size_t photo_len = rec->has_photo ? (size_t)rec->photo.width * rec->photo.height : 0;
    size_t given_len = safe_strlen(rec->given_names, DNIE_RECORD_NAME_MAX - 1);
    size_t sur_len = safe_strlen(rec->surnames, DNIE_RECORD_NAME_MAX - 1);

    size_t payload;
    if(rec->kind == DnieRecordFull) {
        if(rec->dg1_len > DNIE_RECORD_DG1_MAX) return 0;
        payload = 1 + rec->dg1_len;
    } else if(rec->kind == DnieRecordAlbum) {
        payload = 1 + given_len + 1 + sur_len + 8 + 3;
    } else {
        return 0;
    }
    size_t alias_len = safe_strlen(rec->alias, DNIE_RECORD_ALIAS_MAX - 1);
    size_t body = 4 + 1 + 1 + 4 + payload + 1 + alias_len + 2 + photo_len;
    size_t padded = ((body + 32) / 16 + 1) * 16;
    if(padded > max) return 0;

    size_t i = 0;
    memcpy(out, MAGIC, 4);
    i += 4;
    out[i++] = DNIE_RECORD_VERSION;
    out[i++] = rec->kind;
    out[i++] = (uint8_t)(rec->saved_at >> 24);
    out[i++] = (uint8_t)(rec->saved_at >> 16);
    out[i++] = (uint8_t)(rec->saved_at >> 8);
    out[i++] = (uint8_t)rec->saved_at;
    if(rec->kind == DnieRecordFull) {
        out[i++] = rec->dg1_len;
        memcpy(&out[i], rec->dg1, rec->dg1_len);
        i += rec->dg1_len;
    } else {
        out[i++] = (uint8_t)given_len;
        memcpy(&out[i], rec->given_names, given_len);
        i += given_len;
        out[i++] = (uint8_t)sur_len;
        memcpy(&out[i], rec->surnames, sur_len);
        i += sur_len;
        memcpy(&out[i], rec->birth, 8);
        i += 8;
        memset(&out[i], 0, 3);
        memcpy(&out[i], rec->issuer, safe_strlen(rec->issuer, 3));
        i += 3;
    }
    out[i++] = (uint8_t)alias_len;
    memcpy(&out[i], rec->alias, alias_len);
    i += alias_len;
    out[i++] = rec->has_photo ? rec->photo.width : 0;
    out[i++] = rec->has_photo ? rec->photo.height : 0;
    if(photo_len) memcpy(&out[i], rec->photo.gray, photo_len);
    i += photo_len;

    if(!sha256(out, i, &out[i])) return 0;
    i += 32;

    uint8_t pad = (uint8_t)(padded - i); // PKCS#7: 1..16 bytes con el valor del relleno
    memset(&out[i], pad, pad);
    return padded;
}

bool dnie_record_unpack(const uint8_t* in, size_t len, DnieRecord* out) {
    memset(out, 0, sizeof(*out));
    if(len < 48 || len % 16 != 0) return false;

    uint8_t pad = in[len - 1];
    if(pad == 0 || pad > 16) return false;
    for(size_t k = len - pad; k < len; k++) {
        if(in[k] != pad) return false;
    }
    size_t total = len - pad;
    if(total < 4 + 1 + 4 + 1 + 2 + 32) return false;
    size_t body = total - 32;

    uint8_t digest[32];
    if(!sha256(in, body, digest) || memcmp(digest, &in[body], 32) != 0) return false;

    size_t i = 0;
    if(memcmp(in, MAGIC, 4) != 0) return false;
    i += 4;
    uint8_t version = in[i++];
    if(version == 1) {
        out->kind = DnieRecordFull;
    } else if(version >= 2 && version <= 4) {
        out->kind = in[i++];
        if(out->kind != DnieRecordFull && out->kind != DnieRecordAlbum) return false;
    } else {
        return false;
    }
    if(i + 4 > body) return false;
    out->saved_at = (uint32_t)in[i] << 24 | (uint32_t)in[i + 1] << 16 | (uint32_t)in[i + 2] << 8 |
                    in[i + 3];
    i += 4;

    if(out->kind == DnieRecordFull) {
        if(i + 1 > body) return false;
        out->dg1_len = in[i++];
        if(out->dg1_len > DNIE_RECORD_DG1_MAX || i + out->dg1_len > body) return false;
        memcpy(out->dg1, &in[i], out->dg1_len);
        i += out->dg1_len;
    } else {
        if(i + 1 > body) return false;
        size_t gl = in[i++];
        if(gl >= DNIE_RECORD_NAME_MAX || i + gl + 1 > body) return false;
        memcpy(out->given_names, &in[i], gl);
        i += gl;
        size_t sl = in[i++];
        if(sl >= DNIE_RECORD_NAME_MAX || i + sl + 8 > body) return false;
        memcpy(out->surnames, &in[i], sl);
        i += sl;
        memcpy(out->birth, &in[i], 8);
        i += 8;
        if(version >= 3) {
            if(i + 3 > body) return false;
            memcpy(out->issuer, &in[i], 3);
            out->issuer[3] = '\0';
            i += 3;
        }
    }

    if(version >= 4) {
        if(i + 1 > body) return false;
        size_t al = in[i++];
        if(al >= DNIE_RECORD_ALIAS_MAX || i + al > body) return false;
        memcpy(out->alias, &in[i], al);
        i += al;
    }

    if(i + 2 > body) return false;
    uint8_t w = in[i++], h = in[i++];
    size_t photo_len = (size_t)w * h;
    if(w > DNIE_PIX_MAX_W || h > DNIE_PIX_MAX_H || i + photo_len != body) return false;
    if(photo_len) {
        out->has_photo = true;
        out->photo.width = w;
        out->photo.height = h;
        memcpy(out->photo.gray, &in[i], photo_len);
    }
    return true;
}
