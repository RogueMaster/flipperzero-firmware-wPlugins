#include "dnie_mrz.h"

#include <string.h>

int dnie_mrz_check_digit(const char* s, size_t len) {
    static const int weights[3] = {7, 3, 1};
    int sum = 0;
    for(size_t i = 0; i < len; i++) {
        char c = s[i];
        int v;
        if(c >= '0' && c <= '9') {
            v = c - '0';
        } else if(c >= 'A' && c <= 'Z') {
            v = c - 'A' + 10;
        } else if(c == '<') {
            v = 0;
        } else {
            return -1; // caracter ilegal en una MRZ
        }
        sum += v * weights[i % 3];
    }
    return sum % 10;
}

static bool check_field(const char* field, size_t len, char check_char) {
    int d = dnie_mrz_check_digit(field, len);
    if(d < 0) return false;
    if(check_char == '<') return false;
    return check_char == (char)('0' + d);
}

// Copia un campo quitando el relleno '<' del final
static void copy_trim(char* dst, size_t dst_size, const char* src, size_t len) {
    while(len > 0 && src[len - 1] == '<')
        len--;
    if(len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

// Convierte "PEREZ<GARCIA" en "PEREZ GARCIA"
static void filler_to_spaces(char* s) {
    for(; *s; s++) {
        if(*s == '<') *s = ' ';
    }
}

// Lee tag (1 o 2 bytes) + longitud BER. Devuelve bytes de cabecera, 0 si error.
static size_t read_tl(const uint8_t* p, size_t avail, uint16_t* tag, size_t* len) {
    size_t i = 0;
    if(avail < 2) return 0;
    *tag = p[i++];
    if((*tag & 0x1F) == 0x1F) { // tag de dos bytes (5F1F)
        if(avail < 3) return 0;
        *tag = (uint16_t)(*tag << 8 | p[i++]);
    }
    if(i >= avail) return 0;
    uint8_t l = p[i++];
    if(l < 0x80) {
        *len = l;
    } else if(l == 0x81 && i < avail) {
        *len = p[i++];
    } else if(l == 0x82 && i + 1 < avail) {
        *len = (size_t)p[i] << 8 | p[i + 1];
        i += 2;
    } else {
        return 0;
    }
    if(i + *len > avail) return 0;
    return i;
}

// Nombre: APELLIDO1<APELLIDO2<<NOMBRE<NOMBRE2
static void parse_name(const char* f, size_t n, DnieMrz* out) {
    const char* sep = NULL;
    for(size_t i = 0; i + 1 < n; i++) {
        if(f[i] == '<' && f[i + 1] == '<') {
            sep = f + i;
            break;
        }
    }
    if(sep) {
        copy_trim(out->surnames, sizeof(out->surnames), f, (size_t)(sep - f));
        copy_trim(
            out->given_names, sizeof(out->given_names), sep + 2, (size_t)(f + n - (sep + 2)));
    } else {
        copy_trim(out->surnames, sizeof(out->surnames), f, n);
    }
    filler_to_spaces(out->surnames);
    filler_to_spaces(out->given_names);
}

// TD3 (pasaporte):
//  Linea 1: tipo(2) emisor(3) APELLIDOS<<NOMBRE (39)
//  Linea 2: n_doc(9) cd nacionalidad(3) nacimiento(6) cd sexo caducidad(6) cd
//           n_personal(14) cd compuesto   <- el pasaporte espanol pone aqui el DNI
static bool parse_td3(const char* m, DnieMrz* out) {
    const char* l1 = m;
    const char* l2 = m + 44;
    copy_trim(out->doc_type, sizeof(out->doc_type), l1, 2);
    copy_trim(out->issuer, sizeof(out->issuer), l1 + 2, 3);
    parse_name(l1 + 5, 39, out);
    copy_trim(out->doc_number, sizeof(out->doc_number), l2, 9);
    copy_trim(out->nationality, sizeof(out->nationality), l2 + 10, 3);
    memcpy(out->birth, l2 + 13, 6);
    out->sex = l2[20];
    memcpy(out->expiry, l2 + 21, 6);
    copy_trim(out->personal_number, sizeof(out->personal_number), l2 + 28, 14);

    out->check_doc_number = check_field(l2, 9, l2[9]);
    out->check_birth = check_field(l2 + 13, 6, l2[19]);
    out->check_expiry = check_field(l2 + 21, 6, l2[27]);
    char comp[10 + 7 + 22];
    memcpy(comp, l2, 10);
    memcpy(comp + 10, l2 + 13, 7);
    memcpy(comp + 17, l2 + 21, 22);
    out->check_composite = check_field(comp, sizeof(comp), l2[43]);
    return true;
}

bool dnie_mrz_parse_dg1(const uint8_t* dg1, size_t len, DnieMrz* out) {
    memset(out, 0, sizeof(*out));
    uint16_t tag;
    size_t l, h;

    h = read_tl(dg1, len, &tag, &l);
    if(h == 0 || tag != 0x61) return false;
    const uint8_t* inner = dg1 + h;
    size_t inner_len = l;

    h = read_tl(inner, inner_len, &tag, &l);
    // TD1 (tarjeta, 3x30) o TD3 (pasaporte, 2x44)
    if(h == 0 || tag != 0x5F1F || (l != DNIE_MRZ_LEN && l != DNIE_MRZ_TD3_LEN)) return false;
    const char* m = (const char*)(inner + h);

    for(size_t i = 0; i < l; i++) {
        char c = m[i];
        if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '<')) return false;
    }
    memcpy(out->mrz, m, l);
    out->mrz[l] = '\0';
    if(l == DNIE_MRZ_TD3_LEN) return parse_td3(m, out);

    const char* l1 = m;
    const char* l2 = m + 30;
    const char* l3 = m + 60;

    copy_trim(out->doc_type, sizeof(out->doc_type), l1, 2);
    copy_trim(out->issuer, sizeof(out->issuer), l1 + 2, 3);
    copy_trim(out->doc_number, sizeof(out->doc_number), l1 + 5, 9);
    copy_trim(out->personal_number, sizeof(out->personal_number), l1 + 15, 15);

    memcpy(out->birth, l2, 6);
    out->sex = l2[7];
    memcpy(out->expiry, l2 + 8, 6);
    copy_trim(out->nationality, sizeof(out->nationality), l2 + 15, 3);
    parse_name(l3, 30, out);

    out->check_doc_number = check_field(l1 + 5, 9, l1[14]);
    out->check_birth = check_field(l2, 6, l2[6]);
    out->check_expiry = check_field(l2 + 8, 6, l2[14]);

    // Compuesto: linea1[5..29] + linea2[0..6] + linea2[8..14] + linea2[18..28]
    char comp[25 + 7 + 7 + 11];
    memcpy(comp, l1 + 5, 25);
    memcpy(comp + 25, l2, 7);
    memcpy(comp + 32, l2 + 8, 7);
    memcpy(comp + 39, l2 + 18, 11);
    out->check_composite = check_field(comp, sizeof(comp), l2[29]);
    return true;
}

bool dnie_mrz_all_checks_ok(const DnieMrz* mrz) {
    return mrz->check_doc_number && mrz->check_birth && mrz->check_expiry && mrz->check_composite;
}
