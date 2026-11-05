#include "dnie_dg2.h"

#include <string.h>

static uint32_t be16(const uint8_t* p) {
    return (uint32_t)p[0] << 8 | p[1];
}
static uint32_t be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

// Tag de 1 o 2 bytes + longitud BER. Devuelve bytes de cabecera, 0 si no cabe.
// No exige que el valor entero este en el buffer (DG2 es mucho mas largo que la cabecera leida).
static size_t read_tl(const uint8_t* p, size_t avail, uint16_t* tag, size_t* len) {
    size_t i = 0;
    if(avail < 2) return 0;
    *tag = p[i++];
    if((*tag & 0x1F) == 0x1F) {
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
        *len = be16(&p[i]);
        i += 2;
    } else {
        return 0;
    }
    return i;
}

bool dnie_dg2_find_image(const uint8_t* head, size_t head_len, size_t total, DnieDg2Info* out) {
    memset(out, 0, sizeof(*out));
    size_t pos = 0;
    uint16_t tag;
    size_t len, h;

    // 75 (DG2) -> 7F61 (grupo biometrico) -> 02 (numero de instancias) -> 7F60 (plantilla)
    h = read_tl(head, head_len, &tag, &len);
    if(!h || tag != 0x75) return false;
    pos += h;
    h = read_tl(head + pos, head_len - pos, &tag, &len);
    if(!h || tag != 0x7F61) return false;
    pos += h;
    h = read_tl(head + pos, head_len - pos, &tag, &len);
    if(!h || tag != 0x02) return false;
    pos += h + len;
    if(pos >= head_len) return false;
    h = read_tl(head + pos, head_len - pos, &tag, &len);
    if(!h || tag != 0x7F60) return false;
    pos += h;

    // Dentro de 7F60: A1 (cabecera biometrica, la saltamos) y 5F2E/7F2E (datos)
    size_t bdb_len = 0;
    bool found = false;
    while(pos < head_len) {
        h = read_tl(head + pos, head_len - pos, &tag, &len);
        if(!h) return false;
        if(tag == 0x5F2E || tag == 0x7F2E) {
            pos += h;
            bdb_len = len;
            found = true;
            break;
        }
        pos += h + len;
    }
    if(!found || pos + 14 + 20 > head_len) return false;

    // Cabecera general ISO 19794-5: "FAC\0" "010\0" longitud(4) n_imagenes(2)
    const uint8_t* b = head + pos;
    if(memcmp(b, "FAC\0", 4) != 0) return false;
    // Cabecera del registro facial: longitud(4) n_puntos(2) ...
    const uint8_t* fr = b + 14;
    uint32_t record_len = be32(fr);
    out->feature_points = (uint16_t)be16(fr + 4);
    size_t info_off = pos + 14 + 20 + 8 * (size_t)out->feature_points;
    if(info_off + 12 > head_len) return false;

    // Info de la imagen: tipo_cara(1) tipo_datos(1) ancho(2) alto(2) ...
    const uint8_t* ii = head + info_off;
    out->image_type = ii[1];
    out->width = (uint16_t)be16(ii + 2);
    out->height = (uint16_t)be16(ii + 4);

    out->image_offset = info_off + 12;
    size_t overhead = 20 + 8 * (size_t)out->feature_points + 12;
    if(record_len < overhead) return false;
    out->image_len = record_len - overhead;

    // Coherencia con los tamanos de DG2
    if(out->image_offset + out->image_len > total) return false;
    if(14 + record_len > bdb_len) return false;
    return out->image_len > 0;
}

bool dnie_j2k_parse_header(const uint8_t* data, size_t len, DnieJ2kInfo* out) {
    memset(out, 0, sizeof(*out));
    size_t pos = 0;

    // Contenedor JP2: cajas [longitud(4) tipo(4)] hasta 'jp2c' (el codestream)
    if(len >= 12 && be32(data) == 12 && memcmp(data + 4, "jP  ", 4) == 0) {
        out->jp2_container = true;
        while(pos + 8 <= len) {
            uint32_t box_len = be32(data + pos);
            size_t hdr = 8;
            if(memcmp(data + pos + 4, "jp2c", 4) == 0) {
                pos += hdr;
                break;
            }
            if(box_len == 1 || box_len < 8)
                return false; // cajas XL o raras: no las esperamos aqui
            pos += box_len;
        }
    }
    out->codestream_offset = pos;

    // Codestream: SOC (FF4F) y segmentos de la cabecera principal hasta SOT (FF90)
    if(pos + 2 > len || data[pos] != 0xFF || data[pos + 1] != 0x4F) return false;
    pos += 2;
    while(pos + 4 <= len) {
        if(data[pos] != 0xFF) return false;
        uint8_t marker = data[pos + 1];
        if(marker == 0x90 || marker == 0x93) break; // SOT / SOD: fin de la cabecera principal
        size_t seg = be16(data + pos + 2); // incluye los 2 bytes de longitud
        const uint8_t* s = data + pos + 4;
        if(pos + 2 + seg > len) break; // segmento cortado: nos quedamos con lo que tengamos

        if(marker == 0x51 && seg >= 41) { // SIZ
            out->width = be32(s + 2);
            out->height = be32(s + 6);
            out->x_off = be32(s + 10);
            out->y_off = be32(s + 14);
            out->tile_w = be32(s + 18);
            out->tile_h = be32(s + 22);
            out->components = (uint16_t)be16(s + 34);
            out->depth = (uint8_t)((s[36] & 0x7F) + 1);
            out->is_signed = (s[36] & 0x80) != 0;
            out->has_siz = true;
        } else if(marker == 0x52 && seg >= 12) { // COD
            uint8_t scod = s[0];
            out->custom_precincts = (scod & 0x01) != 0;
            out->progression = s[1];
            out->layers = (uint16_t)be16(s + 2);
            out->mct = s[4];
            out->levels = s[5];
            out->cb_w = (uint8_t)(s[6] + 2);
            out->cb_h = (uint8_t)(s[7] + 2);
            out->cb_style = s[8];
            out->transform = s[9];
            out->has_cod = true;
        } else if(marker == 0x5C && seg >= 3) { // QCD
            out->qcd_style = s[0] & 0x1F;
            out->has_qcd = true;
        }
        pos += 2 + seg;
    }
    return out->has_siz && out->has_cod;
}

const char* dnie_j2k_progression_str(uint8_t p) {
    static const char* const names[] = {"LRCP", "RLCP", "RPCL", "PCRL", "CPRL"};
    return p < 5 ? names[p] : "?";
}
