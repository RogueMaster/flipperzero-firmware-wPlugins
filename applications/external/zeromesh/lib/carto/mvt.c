#include "mvt.h"
#include <string.h>

/* mvt_ctx is a stack local and holds two arrays of this many entries. At
   4096 that is a 32 KB frame, which overflows every Flipper thread stack.
   The count resets per layer; the densest tile we pack peaks at 231. */
#define MVT_MAX_VALUES 384

typedef struct {
    carto_framebuffer *fb;
    const carto_style *style;
    float ox, oy, tile_px, per_ext;
    carto_layer_kind category;
    carto_ipt *scratch;
    int scratch_cap;
    float road_scale;
    int min_road_prio;

    const uint8_t *val_ptr[MVT_MAX_VALUES];
    int            val_len[MVT_MAX_VALUES];
    int            nvals;
    int            class_key_idx;
} mvt_ctx;

static uint64_t rvarint(const uint8_t *b, size_t len, size_t *pos) {
    uint64_t r = 0;
    int s = 0;
    while (*pos < len) {
        uint8_t c = b[(*pos)++];
        r |= (uint64_t)(c & 0x7f) << s;
        if (!(c & 0x80)) break;
        s += 7;
        if (s > 63) break;
    }
    return r;
}

static int32_t zigzag(uint32_t n) {
    return (int32_t)((n >> 1) ^ (0u - (n & 1)));
}

static int key_is_class(const uint8_t *p, int len) {
    return (len == 4 && memcmp(p, "kind", 4) == 0)
        || (len == 5 && memcmp(p, "class", 5) == 0)
        || (len == 9 && memcmp(p, "pmap:kind", 9) == 0);
}

static int is_park_kind(const char *k) {
    static const char *parks[] = {
        "park", "wood", "forest", "grass", "playground", "garden",
        "nature_reserve", "meadow", "recreation_ground", "cemetery",
        "allotments", "golf_course", "pitch", "village_green",
    };
    for (size_t i = 0; i < sizeof(parks) / sizeof(parks[0]); ++i)
        if (strcmp(k, parks[i]) == 0) return 1;
    return 0;
}

static void parse_value(const uint8_t *b, size_t len,
                        const uint8_t **sptr, int *slen) {
    *sptr = NULL;
    *slen = 0;
    size_t p = 0;
    while (p < len) {
        uint64_t key = rvarint(b, len, &p);
        uint32_t fn = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
        if (wt == 2) {
            uint64_t l = rvarint(b, len, &p);
            if (fn == 1) { *sptr = b + p; *slen = (int)l; }
            p += (size_t)l;
        } else if (wt == 0) { rvarint(b, len, &p); }
        else if (wt == 5) { p += 4; }
        else if (wt == 1) { p += 8; }
        else break;
    }
}

static void feature_colors(const mvt_ctx *m, int prio, carto_rgb *fill,
                           carto_rgb *line, carto_rgb *pt, int *lw) {
    const carto_style *s = m->style;
    *lw = 1;
    switch (m->category) {
        case CARTO_LAYER_WATER:    *fill = *line = *pt = s->water; break;
        case CARTO_LAYER_LANDUSE:  *fill = *line = *pt = s->park; break;
        case CARTO_LAYER_BUILDING: *fill = *line = *pt = s->building; break;
        case CARTO_LAYER_ROAD: {
            if (prio < CARTO_ROAD_PRIO_MIN) prio = CARTO_ROAD_PRIO_MIN;
            if (prio > CARTO_ROAD_PRIO_MAX) prio = CARTO_ROAD_PRIO_MAX;
            *fill = *line = *pt = s->road_color_by_prio[prio];
            int w = (int)(s->road_width[prio] * m->road_scale + 0.5f);
            *lw = w < 1 ? 1 : w;
            break;
        }
        default: *fill = *line = *pt = s->label_color; break;
    }
}

static void geom_bbox_ext(const uint8_t *g, size_t glen, int *w, int *h) {
    size_t p = 0;
    int cx = 0, cy = 0, minx = 0, miny = 0, maxx = 0, maxy = 0, have = 0;
    while (p < glen) {
        uint64_t cmd = rvarint(g, glen, &p);
        uint32_t id = (uint32_t)(cmd & 7), count = (uint32_t)(cmd >> 3);
        if (id == 1 || id == 2) {
            for (uint32_t k = 0; k < count; ++k) {
                cx += zigzag((uint32_t)rvarint(g, glen, &p));
                cy += zigzag((uint32_t)rvarint(g, glen, &p));
                if (!have) { minx = maxx = cx; miny = maxy = cy; have = 1; }
                else {
                    if (cx < minx) minx = cx;
                    if (cx > maxx) maxx = cx;
                    if (cy < miny) miny = cy;
                    if (cy > maxy) maxy = cy;
                }
            }
        }

    }
    *w = have ? (maxx - minx) : 0;
    *h = have ? (maxy - miny) : 0;
}

static void render_feature(mvt_ctx *m, const uint8_t *g, size_t glen,
                           int geomtype, int prio) {
    carto_rgb fillc, linec, ptc;
    int lw;
    feature_colors(m, prio, &fillc, &linec, &ptc, &lw);

    int stipple = (geomtype == 3 && m->category == CARTO_LAYER_WATER);

    if ((geomtype == 2 || geomtype == 3) && m->per_ext > 0.0f) {
        int bw, bh;
        geom_bbox_ext(g, glen, &bw, &bh);
        float thresh = (geomtype == 2 ? CARTO_MIN_LINE_PX : CARTO_MIN_POLY_PX)
                        / m->per_ext;
        if ((float)bw < thresh && (float)bh < thresh) return;
    }

    size_t p = 0;
    int cx = 0, cy = 0, n = 0;
    carto_ipt *pts = m->scratch;
    m->fb->stipple = stipple;

    while (p < glen) {
        uint64_t cmdint = rvarint(g, glen, &p);
        uint32_t id = (uint32_t)(cmdint & 7);
        uint32_t count = (uint32_t)(cmdint >> 3);

        if (id == 1) {
            if (geomtype == 2 && n >= 2) carto_polyline(m->fb, pts, n, lw, linec);
            n = 0;
            for (uint32_t k = 0; k < count; ++k) {
                cx += zigzag((uint32_t)rvarint(g, glen, &p));
                cy += zigzag((uint32_t)rvarint(g, glen, &p));
                int fx = (int)(m->ox + cx * m->per_ext);
                int fy = (int)(m->oy + cy * m->per_ext);
                if (geomtype == 1) {
                    carto_fill_rect(m->fb, fx - 1, fy - 1, 3, 3, ptc);
                } else if (n < m->scratch_cap) {
                    pts[n].x = fx; pts[n].y = fy; ++n;
                }
            }
        } else if (id == 2) {
            for (uint32_t k = 0; k < count; ++k) {
                cx += zigzag((uint32_t)rvarint(g, glen, &p));
                cy += zigzag((uint32_t)rvarint(g, glen, &p));
                if (n < m->scratch_cap) {
                    pts[n].x = (int)(m->ox + cx * m->per_ext);
                    pts[n].y = (int)(m->oy + cy * m->per_ext);
                    ++n;
                }
            }
        } else if (id == 7) {
            if (geomtype == 3 && n >= 3) carto_fill_polygon(m->fb, pts, n, fillc);
            n = 0;
        }
    }
    if (geomtype == 2 && n >= 2) carto_polyline(m->fb, pts, n, lw, linec);
    m->fb->stipple = 0;
}

static void feature_class(mvt_ctx *m, const uint8_t *tags, size_t taglen,
                          char *out, int outcap) {
    out[0] = 0;
    if (m->class_key_idx < 0 || !tags) return;
    size_t tp = 0;
    while (tp < taglen) {
        uint32_t ki = (uint32_t)rvarint(tags, taglen, &tp);
        if (tp >= taglen) break;
        uint32_t vi = (uint32_t)rvarint(tags, taglen, &tp);
        if ((int)ki == m->class_key_idx && (int)vi < m->nvals
                && m->val_len[vi] > 0) {
            int cl = m->val_len[vi];
            if (cl > outcap - 1) cl = outcap - 1;
            memcpy(out, m->val_ptr[vi], (size_t)cl);
            out[cl] = 0;
            return;
        }
    }
}

static void render_feature_msg(mvt_ctx *m, const uint8_t *b, size_t len) {
    int geomtype = 0;
    const uint8_t *geom = NULL, *tags = NULL;
    size_t geomlen = 0, taglen = 0, p = 0;
    while (p < len) {
        uint64_t key = rvarint(b, len, &p);
        uint32_t fn = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
        if (wt == 0) {
            uint64_t v = rvarint(b, len, &p);
            if (fn == 3) geomtype = (int)v;
        } else if (wt == 2) {
            uint64_t l = rvarint(b, len, &p);
            if (fn == 4) { geom = b + p; geomlen = (size_t)l; }
            else if (fn == 2) { tags = b + p; taglen = (size_t)l; }
            p += (size_t)l;
        } else if (wt == 5) { p += 4; }
        else if (wt == 1) { p += 8; }
        else break;
    }
    if (!geom) return;

    int prio = CARTO_ROAD_PRIO_MIN;
    if (m->category == CARTO_LAYER_ROAD || m->category == CARTO_LAYER_LANDUSE) {
        char cls[40];
        feature_class(m, tags, taglen, cls, (int)sizeof cls);
        if (m->category == CARTO_LAYER_ROAD) {
            prio = carto_road_priority(cls);
            if (prio < m->min_road_prio)
                return;
        } else if (!is_park_kind(cls)) {
            return;
        }
    }
    render_feature(m, geom, geomlen, geomtype, prio);
}

static void render_layer(mvt_ctx *m, const uint8_t *b, size_t len) {
    char name[80];
    int extent = 4096;
    size_t p = 0;
    int key_idx = 0;
    name[0] = 0;
    m->nvals = 0;
    m->class_key_idx = -1;

    while (p < len) {
        uint64_t key = rvarint(b, len, &p);
        uint32_t fn = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
        if (wt == 2) {
            uint64_t l = rvarint(b, len, &p);
            if (fn == 1) {
                int cn = (int)l; if (cn > 79) cn = 79;
                for (int i = 0; i < cn; ++i) name[i] = (char)b[p + i];
                name[cn] = 0;
            } else if (fn == 3) {
                if (m->class_key_idx < 0 && key_is_class(b + p, (int)l))
                    m->class_key_idx = key_idx;
                ++key_idx;
            } else if (fn == 4) {
                if (m->nvals < MVT_MAX_VALUES) {
                    const uint8_t *sp; int sl;
                    parse_value(b + p, (size_t)l, &sp, &sl);
                    m->val_ptr[m->nvals] = sp;
                    m->val_len[m->nvals] = (sp ? sl : 0);
                    ++m->nvals;
                }
            }
            p += (size_t)l;
        } else if (wt == 0) {
            uint64_t v = rvarint(b, len, &p);
            if (fn == 5) extent = (int)v;
        } else if (wt == 5) { p += 4; }
        else if (wt == 1) { p += 8; }
        else break;
    }

    if (carto_classify_layer(name) != m->category) return;
    m->per_ext = m->tile_px / (float)(extent > 0 ? extent : 4096);

    p = 0;
    while (p < len) {
        uint64_t key = rvarint(b, len, &p);
        uint32_t fn = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
        if (wt == 2) {
            uint64_t l = rvarint(b, len, &p);
            if (fn == 2) render_feature_msg(m, b + p, (size_t)l);
            p += (size_t)l;
        } else if (wt == 0) { rvarint(b, len, &p); }
        else if (wt == 5) { p += 4; }
        else if (wt == 1) { p += 8; }
        else break;
    }
}

void carto_mvt_render_category(carto_framebuffer *fb, const carto_style *style,
                              const uint8_t *tile, size_t len,
                              carto_layer_kind category,
                              float ox, float oy, float tile_px,
                              int zoom,
                              carto_ipt *scratch, int scratch_cap) {
    mvt_ctx m;
    m.fb = fb;
    m.style = style;
    m.ox = ox;
    m.oy = oy;
    m.tile_px = tile_px;
    m.per_ext = tile_px / 4096.0f;
    m.category = category;
    m.scratch = scratch;
    m.scratch_cap = scratch_cap;

    float rs = 0.125f + (zoom - 9) * 0.0625f;
    m.road_scale = rs < 0.125f ? 0.125f : (rs > 0.55f ? 0.55f : rs);
    int mp = 16 - zoom;
    m.min_road_prio = mp < 1 ? 1 : (mp > 10 ? 10 : mp);

    m.nvals = 0;
    m.class_key_idx = -1;

    size_t p = 0;
    while (p < len) {
        uint64_t key = rvarint(tile, len, &p);
        uint32_t fn = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
        if (wt == 2) {
            uint64_t l = rvarint(tile, len, &p);
            if (fn == 3) render_layer(&m, tile + p, (size_t)l);
            p += (size_t)l;
        } else if (wt == 0) { rvarint(tile, len, &p); }
        else if (wt == 5) { p += 4; }
        else if (wt == 1) { p += 8; }
        else break;
    }
}
