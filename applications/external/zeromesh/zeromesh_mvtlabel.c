#include "zeromesh_mvtlabel.h"

#include <stdbool.h>
#include <string.h>

static uint64_t rd_varint(const uint8_t* b, size_t len, size_t* p) {
    uint64_t r = 0;
    int s = 0;
    while(*p < len && s < 64) {
        uint8_t c = b[(*p)++];
        r |= (uint64_t)(c & 0x7f) << s;
        if(!(c & 0x80)) break;
        s += 7;
    }
    return r;
}

static bool skip_field(const uint8_t* b, size_t len, size_t* p, uint32_t wire) {
    switch(wire) {
    case 0:
        rd_varint(b, len, p);
        return true;
    case 1:
        *p += 8;
        return *p <= len;
    case 2: {
        uint64_t n = rd_varint(b, len, p);
        *p += (size_t)n;
        return *p <= len;
    }
    case 5:
        *p += 4;
        return *p <= len;
    default:
        return false;
    }
}

static int32_t layer_name_key(const uint8_t* b, size_t len) {
    size_t p = 0;
    int32_t idx = 0;
    while(p < len) {
        uint64_t key = rd_varint(b, len, &p);
        uint32_t field = (uint32_t)(key >> 3), wire = (uint32_t)(key & 7);
        if(field == 3 && wire == 2) {
            uint64_t n = rd_varint(b, len, &p);
            if(p + n > len) return -1;
            if(n == 4 && memcmp(b + p, "name", 4) == 0) return idx;
            p += (size_t)n;
            idx++;
        } else if(!skip_field(b, len, &p, wire)) {
            return -1;
        }
    }
    return -1;
}

static bool
    layer_value_str(const uint8_t* b, size_t len, uint32_t want, char* out, size_t out_sz) {
    size_t p = 0;
    uint32_t idx = 0;
    while(p < len) {
        uint64_t key = rd_varint(b, len, &p);
        uint32_t field = (uint32_t)(key >> 3), wire = (uint32_t)(key & 7);
        if(field == 4 && wire == 2) {
            uint64_t n = rd_varint(b, len, &p);
            if(p + n > len) return false;
            if(idx == want) {
                const uint8_t* v = b + p;
                size_t vlen = (size_t)n, q = 0;
                while(q < vlen) {
                    uint64_t vk = rd_varint(v, vlen, &q);
                    uint32_t vf = (uint32_t)(vk >> 3), vw = (uint32_t)(vk & 7);
                    if(vf == 1 && vw == 2) {
                        uint64_t sl = rd_varint(v, vlen, &q);
                        if(q + sl > vlen) return false;
                        size_t copy = (size_t)sl;
                        if(copy >= out_sz) copy = out_sz - 1;
                        memcpy(out, v + q, copy);
                        out[copy] = '\0';
                        return true;
                    }
                    if(!skip_field(v, vlen, &q, vw)) return false;
                }
                return false;
            }
            p += (size_t)n;
            idx++;
        } else if(!skip_field(b, len, &p, wire)) {
            return false;
        }
    }
    return false;
}

static void
    scan_layer(const uint8_t* b, size_t len, int32_t name_key, MvtLabelCallback cb, void* context) {
    size_t p = 0;
    while(p < len) {
        uint64_t key = rd_varint(b, len, &p);
        uint32_t field = (uint32_t)(key >> 3), wire = (uint32_t)(key & 7);
        if(field != 2 || wire != 2) {
            if(!skip_field(b, len, &p, wire)) return;
            continue;
        }

        uint64_t flen = rd_varint(b, len, &p);
        if(p + flen > len) return;
        const uint8_t* f = b + p;
        size_t fl = (size_t)flen;
        p += fl;

        bool have_name = false;
        uint32_t name_val = 0;
        int32_t ex = 0, ey = 0;
        bool have_pt = false;

        size_t q = 0;
        while(q < fl) {
            uint64_t fk = rd_varint(f, fl, &q);
            uint32_t ff = (uint32_t)(fk >> 3), fw = (uint32_t)(fk & 7);

            if(ff == 2 && fw == 2) {
                uint64_t tl = rd_varint(f, fl, &q);
                if(q + tl > fl) return;
                size_t end = q + (size_t)tl;
                while(q + 1 < end) {
                    uint32_t k = (uint32_t)rd_varint(f, end, &q);
                    uint32_t v = (uint32_t)rd_varint(f, end, &q);
                    if((int32_t)k == name_key) {
                        name_val = v;
                        have_name = true;
                    }
                }
                q = end;
            } else if(ff == 4 && fw == 2) {
                uint64_t gl = rd_varint(f, fl, &q);
                if(q + gl > fl) return;
                size_t end = q + (size_t)gl;

                if(q < end) {
                    uint32_t cmd = (uint32_t)rd_varint(f, end, &q);
                    if((cmd & 7) == 1 && q + 1 < end) {
                        uint32_t zx = (uint32_t)rd_varint(f, end, &q);
                        uint32_t zy = (uint32_t)rd_varint(f, end, &q);
                        ex = (int32_t)((zx >> 1) ^ (~(zx & 1) + 1));
                        ey = (int32_t)((zy >> 1) ^ (~(zy & 1) + 1));
                        have_pt = true;
                    }
                }
                q = end;
            } else if(!skip_field(f, fl, &q, fw)) {
                return;
            }
        }

        if(have_name && have_pt) {
            char text[32];
            if(layer_value_str(b, len, name_val, text, sizeof(text)) && text[0]) {
                cb(text, ex, ey, context);
            }
        }
    }
}

void mvt_scan_labels(
    const uint8_t* tile,
    size_t len,
    const char* const* layers,
    size_t layer_count,
    MvtLabelCallback cb,
    void* context,
    uint32_t* extent_out) {
    if(!tile || !len || !cb) return;
    if(extent_out) *extent_out = 4096;

    size_t p = 0;
    while(p < len) {
        uint64_t key = rd_varint(tile, len, &p);
        uint32_t field = (uint32_t)(key >> 3), wire = (uint32_t)(key & 7);
        if(field != 3 || wire != 2) {
            if(!skip_field(tile, len, &p, wire)) return;
            continue;
        }

        uint64_t llen = rd_varint(tile, len, &p);
        if(p + llen > len) return;
        const uint8_t* lb = tile + p;
        size_t ll = (size_t)llen;
        p += ll;

        char lname[48] = {0};
        size_t q = 0;
        while(q < ll) {
            uint64_t lk = rd_varint(lb, ll, &q);
            uint32_t lf = (uint32_t)(lk >> 3), lw = (uint32_t)(lk & 7);
            if(lf == 1 && lw == 2) {
                uint64_t n = rd_varint(lb, ll, &q);
                if(q + n > ll) break;
                size_t copy = (size_t)n;
                if(copy >= sizeof(lname)) copy = sizeof(lname) - 1;
                memcpy(lname, lb + q, copy);
                lname[copy] = '\0';
                q += (size_t)n;
            } else if(lf == 5 && lw == 0) {
                uint32_t e = (uint32_t)rd_varint(lb, ll, &q);
                if(extent_out && e) *extent_out = e;
            } else if(!skip_field(lb, ll, &q, lw)) {
                break;
            }
        }

        bool wanted = false;
        for(size_t i = 0; i < layer_count; i++) {
            if(strcmp(lname, layers[i]) == 0) {
                wanted = true;
                break;
            }
        }
        if(!wanted) continue;

        int32_t nk = layer_name_key(lb, ll);
        if(nk < 0) continue;
        scan_layer(lb, ll, nk, cb, context);
    }
}
