#include "zeromesh_map.h"
#include "zeromesh_history.h"

#include <storage/storage.h>
#include <core/memmgr.h>
#include <string.h>

#include "zeromesh_mvtlabel.h"
#include "zeromesh_pmtiles.h"
#include "mvt.h"
#include "carto/raster.h"
#include "carto/style.h"
#include "carto/framebuffer.h"

#define TAG "zeromesh_map"

#define MAP_DIR            APP_DATA_PATH("map")
#define MAP_PMTILES        APP_DATA_PATH("map.pmtiles")
#define MAP_DIR_OLD        "/ext/zeromesh/map"
#define MAP_PMTILES_OLD    "/ext/zeromesh/map.pmtiles"
#define MAP_TILE_MAX   24576
#define MAP_SCRATCH_PT 384
#define MAP_MAX_LABELS 6
#define MAP_MAX_TOWNS  16
#define MAP_TOOLBAR_N  6
#define MAP_PAN_STEP   16

#define MAP_HOME_LAT 43.4443f
#define MAP_HOME_LON (-71.6478f)

#define ZM_PI 3.14159265f

typedef struct {
    int16_t sx, sy;
    char text[28];
} MapLabel;

typedef struct {
    int gx, gy;
    char name[22];
} MapTown;

struct MapState {
    uint8_t* fb_pixels;
    carto_framebuffer fb;
    carto_ipt* scratch;
    int scratch_cap;
    carto_style style;

    PmTiles* pm;

    uint8_t* tile;
    size_t tile_cap;
    size_t tile_len;

    int z;
    int tile_px;

    int gx, gy;
    int good_gx, good_gy;
    bool have_good;

    uint8_t zoom_min, zoom_max;

    float lat, lon;
    uint32_t focus_id;
    int focus_idx;

    bool dirty;
    bool reload;
    bool pan_active;
    bool show_labels;

    bool edge_n, edge_s, edge_e, edge_w;
    uint32_t blink;

    bool ui_toolbar;
    bool ui_towns;
    bool ui_gps;
    bool want_towns;
    int8_t toolbar_sel;
    int8_t town_sel;
    uint8_t town_count;
    MapTown towns[MAP_MAX_TOWNS];

    MapLabel labels[MAP_MAX_LABELS];
    int16_t taken[MAP_MAX_LABELS + 2 + ROSTER_MAX_NODES][4];
    uint8_t label_count;
    uint32_t extent;

    char status[32];
};

static void map_focus_index(ZeroMeshApp* app, int idx);
static void map_cycle_focus(ZeroMeshApp* app, int dir);
static void map_set_zoom(MapState* m, int nz);
static bool map_fetch_tile(MapState* m, int z, int tx, int ty);
static void map_clamp(MapState* m);

static void map_mono_style(carto_style* s) {
    memset(s, 0, sizeof(*s));

    const carto_rgb ink = {255, 255, 255};
    const carto_rgb off = {0, 0, 0};

    s->bg = off;
    s->water = ink;
    s->park = off;
    s->building = ink;
    s->road_color = ink;
    s->label_color = ink;
    s->halo_color = off;

    for(int i = 0; i <= CARTO_ROAD_PRIO_MAX; i++) {
        s->road_width[i] = 1;
        s->road_color_by_prio[i] = ink;
    }
}

static float zm_sinf(float x) {
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f + x2 * x2 / 120.0f - x2 * x2 * x2 / 5040.0f +
                x2 * x2 * x2 * x2 / 362880.0f - x2 * x2 * x2 * x2 * x2 / 39916800.0f);
}

static float zm_cosf(float x) {
    float x2 = x * x;
    return 1.0f - x2 / 2.0f + x2 * x2 / 24.0f - x2 * x2 * x2 / 720.0f +
           x2 * x2 * x2 * x2 / 40320.0f;
}

static float zm_logf(float x) {
    if(x <= 0.0f) return 0.0f;
    int e = 0;
    while(x > 1.5f) {
        x *= 0.5f;
        e++;
    }
    while(x < 0.75f) {
        x *= 2.0f;
        e--;
    }
    float t = (x - 1.0f) / (x + 1.0f);
    float t2 = t * t;
    float s = t * (2.0f + t2 * (2.0f / 3.0f + t2 * (2.0f / 5.0f + t2 * (2.0f / 7.0f + t2 * (2.0f / 9.0f + t2 * 2.0f / 11.0f)))));
    return s + (float)e * 0.69314718f;
}

/* u8g2 takes unsigned coordinates: a negative x arrives as 65535 and walks off
   the framebuffer. Clip before anything reaches the canvas. */
static void map_box(Canvas* canvas, int x, int y, int w, int h) {
    if(w <= 0 || h <= 0) return;
    if(x < 0) {
        w += x;
        x = 0;
    }
    if(y < 0) {
        h += y;
        y = 0;
    }
    if(x >= MAP_W || y >= MAP_H) return;
    if(x + w > MAP_W) w = MAP_W - x;
    if(y + h > MAP_H) h = MAP_H - y;
    if(w <= 0 || h <= 0) return;
    canvas_draw_box(canvas, x, y, (size_t)w, (size_t)h);
}

static uint32_t zm_isqrt(uint32_t v) {
    uint32_t r = 0, bit = 1u << 30;
    while(bit > v)
        bit >>= 2;
    while(bit) {
        if(v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

static float zm_mercator_y(float lat_deg) {
    if(lat_deg > 85.0f) lat_deg = 85.0f;
    if(lat_deg < -85.0f) lat_deg = -85.0f;
    float s = zm_sinf(lat_deg * ZM_PI / 180.0f);
    if(s > 0.999999f) s = 0.999999f;
    if(s < -0.999999f) s = -0.999999f;
    return 0.5f - (0.5f * zm_logf((1.0f + s) / (1.0f - s))) / (2.0f * ZM_PI);
}

static void latlon_global(float lat, float lon, int z, float* gx, float* gy) {
    float n = (float)(1 << z);
    *gx = (lon + 180.0f) / 360.0f * n;
    *gy = zm_mercator_y(lat) * n;
}

static bool map_fetch_tile(MapState* m, int z, int tx, int ty) {
    m->tile_len = 0;
    if(!m->tile || tx < 0 || ty < 0) return false;

    if(m->pm) {
        size_t got = 0;
        if(pmtiles_get_tile(
               m->pm, (uint8_t)z, (uint32_t)tx, (uint32_t)ty, m->tile, m->tile_cap, &got)) {
            m->tile_len = got;
            return true;
        }
        return false;
    }

    bool ok = false;
    char path[96];
    snprintf(path, sizeof(path), MAP_DIR "/%d/%d/%d.mvt", z, tx, ty);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);

    if(!storage_file_exists(storage, path))
        snprintf(path, sizeof(path), MAP_DIR_OLD "/%d/%d/%d.mvt", z, tx, ty);

    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > 0 && sz <= m->tile_cap) {
            uint16_t got = storage_file_read(f, m->tile, (uint16_t)sz);
            if(got == sz) {
                m->tile_len = (size_t)sz;
                ok = true;
            }
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void map_focus_name(ZeroMeshApp* app, char* out, size_t cap) {
    MapState* m = app->map;
    if(m->focus_idx < 0 || m->focus_idx >= (int)app->roster.count) {
        strncpy(out, "Home", cap - 1);
        out[cap - 1] = 0;
        return;
    }
    NodeEntry* nd = &app->roster.nodes[m->focus_idx];
    if(nd->has_name && nd->short_name[0]) {
        strncpy(out, nd->short_name, cap - 1);
        out[cap - 1] = 0;
    } else {
        snprintf(out, cap, "%04lx", (unsigned long)(nd->node_id & 0xFFFF));
    }
}

static void map_clamp(MapState* m) {
    int world = (1 << m->z) * m->tile_px;
    if(m->gx > world - MAP_W) m->gx = world - MAP_W;
    if(m->gy > world - MAP_H) m->gy = world - MAP_H;
    if(m->gx < 0) m->gx = 0;
    if(m->gy < 0) m->gy = 0;
}

static void map_recenter(MapState* m) {
    float gxf, gyf;
    latlon_global(m->lat, m->lon, m->z, &gxf, &gyf);
    m->gx = (int)(gxf * (float)m->tile_px) - MAP_W / 2;
    m->gy = (int)(gyf * (float)m->tile_px) - MAP_H / 2;
    map_clamp(m);
}

typedef struct {
    MapLabel* out;
    uint8_t count;
    float per;
    float ox, oy;
} LabelSink;

static void label_cb(const char* name, int32_t ex, int32_t ey, void* ctx) {
    LabelSink* sink = ctx;
    if(sink->count >= MAP_MAX_LABELS) return;

    /* Some OSM features carry their element id as a name. A run of digits is
       never a useful place name, so it only costs a label slot. */
    bool all_digits = true;
    for(const char* c = name; *c; c++) {
        if(*c < 0x30 || *c > 0x39) {
            all_digits = false;
            break;
        }
    }
    if(all_digits) return;

    int px = (int)((float)ex * sink->per + sink->ox);
    int py = (int)((float)ey * sink->per + sink->oy);
    if(px < -20 || px >= MAP_W || py < 8 || py >= MAP_H) return;

    MapLabel* l = &sink->out[sink->count];
    l->sx = (int16_t)px;
    l->sy = (int16_t)py;
    strncpy(l->text, name, sizeof(l->text) - 1);
    l->text[sizeof(l->text) - 1] = 0;
    sink->count++;
}

typedef struct {
    MapTown* out;
    uint8_t count;
    int tx, ty, tile_px;
    uint32_t extent;
} TownSink;

static void town_cb(const char* name, int32_t ex, int32_t ey, void* ctx) {
    TownSink* sink = ctx;
    if(sink->count >= MAP_MAX_TOWNS) return;

    bool all_digits = true;
    for(const char* c = name; *c; c++) {
        if(*c < 0x30 || *c > 0x39) {
            all_digits = false;
            break;
        }
    }
    if(all_digits || !name[0]) return;

    float per = (float)sink->tile_px / (float)(sink->extent ? sink->extent : 4096);
    MapTown* t = &sink->out[sink->count];
    t->gx = sink->tx * sink->tile_px + (int)((float)ex * per);
    t->gy = sink->ty * sink->tile_px + (int)((float)ey * per);
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = 0;
    sink->count++;
}

static void map_scan_towns(ZeroMeshApp* app) {
    MapState* m = app->map;
    const int tpx = m->tile_px;
    int tx = (m->gx + MAP_W / 2) / tpx;
    int ty = (m->gy + MAP_H / 2) / tpx;

    MapTown staging[MAP_MAX_TOWNS];
    memset(staging, 0, sizeof(staging));
    TownSink sink = {staging, 0, tx, ty, tpx, m->extent};

    if(map_fetch_tile(m, m->z, tx, ty)) {
        static const char* const town_layers[] = {"place_labels"};
        uint32_t ext = m->extent;
        mvt_scan_labels(m->tile, m->tile_len, town_layers, 1, town_cb, &sink, &ext);
    }

    int64_t cx = m->gx + MAP_W / 2;
    int64_t cy = m->gy + MAP_H / 2;
    for(int i = 1; i < (int)sink.count; i++) {
        MapTown key = staging[i];
        int64_t kdx = key.gx - cx, kdy = key.gy - cy;
        int64_t kd = kdx * kdx + kdy * kdy;
        int j = i - 1;
        while(j >= 0) {
            int64_t dx = staging[j].gx - cx, dy = staging[j].gy - cy;
            if(dx * dx + dy * dy <= kd) break;
            staging[j + 1] = staging[j];
            j--;
        }
        staging[j + 1] = key;
    }

    furi_mutex_acquire(app->lock, FuriWaitForever);
    memcpy(m->towns, staging, sizeof(staging));
    m->town_count = sink.count;
    m->town_sel = 0;
    m->ui_towns = true;
    furi_mutex_release(app->lock);

    /* the scan reused the tile buffer, so the framebuffer is stale */
    m->dirty = true;
}

void map_tick(ZeroMeshApp* app) {
    if(!app || !app->map) return;
    MapState* m = app->map;
    /* Markers sit on top of map ink and are easy to lose, so the focused
       one pulses. Only the phase flip costs a redraw. */
    uint32_t phase = furi_get_tick() / 300;
    if(phase != m->blink) {
        m->blink = phase;
        app->need_render = true;
    }

    if(!m->fb_pixels || (!m->dirty && !m->want_towns)) return;

    if(m->want_towns) {
        m->want_towns = false;
        map_scan_towns(app);
    }

    if(m->reload) {
        m->reload = false;
        map_recenter(m);
    }
    map_clamp(m);

    MapLabel staging[MAP_MAX_LABELS];
    LabelSink sink = {staging, 0, 0.0f, 0.0f, 0.0f};

    static const carto_layer_kind order[] = {
        CARTO_LAYER_WATER,
        CARTO_LAYER_ROAD,
    };
    static const char* const label_layers[] = {
        "place_labels",
        "water_polygons_labels",
        "water_lines_labels",
    };

    const int tpx = m->tile_px;
    int tx0 = 0, tx1 = 0, ty0 = 0, ty1 = 0;
    int drawn = 0;

    for(int attempt = 0; attempt < 2; attempt++) {
    memset(m->fb_pixels, 0, (size_t)m->fb.stride * MAP_H);
    memset(staging, 0, sizeof(staging));
    sink.count = 0;
    drawn = 0;

    tx0 = m->gx / tpx;
    tx1 = (m->gx + MAP_W - 1) / tpx;
    ty0 = m->gy / tpx;
    ty1 = (m->gy + MAP_H - 1) / tpx;

    for(int ty = ty0; ty <= ty1; ty++) {
        for(int tx = tx0; tx <= tx1; tx++) {
            if(!map_fetch_tile(m, m->z, tx, ty)) continue;
            drawn++;

            float ox = (float)(tx * tpx - m->gx);
            float oy = (float)(ty * tpx - m->gy);

            for(size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
                carto_mvt_render_category(
                    &m->fb,
                    &m->style,
                    m->tile,
                    m->tile_len,
                    order[i],
                    ox,
                    oy,
                    (float)tpx,
                    m->z,
                    m->scratch,
                    m->scratch_cap);
            }

            if(!m->show_labels) continue;

            sink.per = (float)tpx / (float)(m->extent ? m->extent : 4096);
            sink.ox = ox;
            sink.oy = oy;
            mvt_scan_labels(
                m->tile,
                m->tile_len,
                label_layers,
                sizeof(label_layers) / sizeof(label_layers[0]),
                label_cb,
                &sink,
                &m->extent);
        }
    }

    /* Panning past the edge of the archive draws nothing. Step back to the
       last position that rendered and redraw inside this same tick, so the
       screen never shows a blank frame still carrying the old labels. */
    if(drawn > 0) break;
    if(!m->have_good) break;
    if(m->gx == m->good_gx && m->gy == m->good_gy) break;
    m->gx = m->good_gx;
    m->gy = m->good_gy;
    }

    if(drawn > 0) {
        m->good_gx = m->gx;
        m->good_gy = m->gy;
        m->have_good = true;
    }

    if(m->pm) {
        int ctx = (tx0 + tx1) / 2;
        int cty = (ty0 + ty1) / 2;
        uint8_t z = (uint8_t)m->z;
        m->edge_w = !pmtiles_has_tile(m->pm, z, (uint32_t)(tx0 - 1), (uint32_t)cty);
        m->edge_e = !pmtiles_has_tile(m->pm, z, (uint32_t)(tx1 + 1), (uint32_t)cty);
        m->edge_n = !pmtiles_has_tile(m->pm, z, (uint32_t)ctx, (uint32_t)(ty0 - 1));
        m->edge_s = !pmtiles_has_tile(m->pm, z, (uint32_t)ctx, (uint32_t)(ty1 + 1));
    } else {
        m->edge_n = m->edge_s = m->edge_e = m->edge_w = false;
    }

    if(sink.count > MAP_MAX_LABELS) sink.count = MAP_MAX_LABELS;

    char name[16];
    map_focus_name(app, name, sizeof(name));

    furi_mutex_acquire(app->lock, FuriWaitForever);
    memcpy(m->labels, staging, sizeof(staging));
    m->label_count = sink.count;
    snprintf(m->status, sizeof(m->status), drawn ? "%s z%d" : "%s z%d -", name, m->z);
    furi_mutex_release(app->lock);

    m->dirty = false;
}

typedef struct {
    int px, py;
    int tx, ty, tw;
    int halo;
    const char* tag;
    char idbuf[8];
} NodeDraw;

/* Marker and tag geometry, shared so the label placer reserves precisely
   what the markers go on to draw. */
static bool node_layout(Canvas* canvas, MapState* m, NodeEntry* n, NodeDraw* out) {
    if(!n->has_position) return false;

    float gxf, gyf;
    latlon_global(n->latitude_i / 1e7f, n->longitude_i / 1e7f, m->z, &gxf, &gyf);
    out->px = (int)(gxf * (float)m->tile_px) - m->gx;
    out->py = (int)(gyf * (float)m->tile_px) - m->gy;

    if(out->px < 8 || out->py < 16 || out->px >= MAP_W - 8 || out->py >= MAP_H - 8)
        return false;

    out->halo = (n->node_id == m->focus_id) ? 7 : 4;

    out->tag = (n->has_name && n->short_name[0]) ? n->short_name : NULL;
    if(!out->tag) {
        snprintf(out->idbuf, sizeof(out->idbuf), "%04lx",
                 (unsigned long)(n->node_id & 0xFFFF));
        out->tag = out->idbuf;
    }

    out->tw = canvas_string_width(canvas, out->tag);
    out->tx = out->px + 8;
    if(out->tx + out->tw > MAP_W) out->tx = out->px - 8 - out->tw;
    if(out->tx < 0) out->tx = 0;
    out->ty = out->py + 3;
    if(out->ty < 8) out->ty = 8;
    if(out->ty > MAP_H - 1) out->ty = MAP_H - 1;
    return true;
}

static void map_draw_labels(Canvas* canvas, ZeroMeshApp* app) {
    MapState* m = app->map;
    canvas_set_font(canvas, FontSecondary);

    int ntaken = 0;

    m->taken[ntaken][0] = 0;
    m->taken[ntaken][1] = 0;
    m->taken[ntaken][2] = MAP_W;
    m->taken[ntaken][3] = 9;
    ntaken++;

    m->taken[ntaken][0] = 0;
    m->taken[ntaken][1] = MAP_H - 13;
    m->taken[ntaken][2] = 52;
    m->taken[ntaken][3] = MAP_H;
    ntaken++;

    for(uint8_t i = 0; i < app->roster.count; i++) {
        NodeDraw nd;
        if(!node_layout(canvas, m, &app->roster.nodes[i], &nd)) continue;
        if(ntaken >= MAP_MAX_LABELS + 2 + ROSTER_MAX_NODES) break;

        int x0 = nd.px - nd.halo, x1 = nd.px + nd.halo;
        int y0 = nd.py - nd.halo, y1 = nd.py + nd.halo;
        if(nd.tx - 1 < x0) x0 = nd.tx - 1;
        if(nd.tx + nd.tw + 1 > x1) x1 = nd.tx + nd.tw + 1;
        if(nd.ty - 7 < y0) y0 = nd.ty - 7;
        if(nd.ty + 1 > y1) y1 = nd.ty + 1;

        m->taken[ntaken][0] = (int16_t)x0;
        m->taken[ntaken][1] = (int16_t)y0;
        m->taken[ntaken][2] = (int16_t)x1;
        m->taken[ntaken][3] = (int16_t)y1;
        ntaken++;
    }

    for(uint8_t i = 0; i < m->label_count; i++) {
        MapLabel* l = &m->labels[i];
        int px = l->sx;
        int py = l->sy;

        /* A name wider than the screen cannot be placed anywhere, so trim
           it to fit rather than letting it run off both edges. */
        char text[28];
        strncpy(text, l->text, sizeof(text) - 1);
        text[sizeof(text) - 1] = 0;
        int w = canvas_string_width(canvas, text);
        size_t tl = strlen(text);
        while(w > MAP_W - 4 && tl > 3) {
            text[--tl] = 0;
            w = canvas_string_width(canvas, text);
        }

        if(px + w > MAP_W) px = MAP_W - w;
        if(px < 0) px = 0;

        int x0 = px - 1, y0 = py - 7, x1 = px + w + 1, y1 = py + 1;

        bool clash = false;
        for(int t = 0; t < ntaken; t++) {
            if(x0 < m->taken[t][2] && x1 > m->taken[t][0] && y0 < m->taken[t][3] &&
               y1 > m->taken[t][1]) {
                clash = true;
                break;
            }
        }
        if(clash) continue;

        if(ntaken < MAP_MAX_LABELS + 2 + ROSTER_MAX_NODES) {
            m->taken[ntaken][0] = (int16_t)x0;
            m->taken[ntaken][1] = (int16_t)y0;
            m->taken[ntaken][2] = (int16_t)x1;
            m->taken[ntaken][3] = (int16_t)y1;
            ntaken++;
        }

        canvas_set_color(canvas, ColorWhite);
        map_box(canvas, x0, y0, w + 2, 8);
        canvas_set_color(canvas, ColorBlack);
        if(px >= 0 && py >= 0 && px < MAP_W && py < MAP_H) {
            canvas_draw_str(canvas, px, py, text);
        }
    }
}

static void map_draw_nodes(Canvas* canvas, ZeroMeshApp* app) {
    MapState* m = app->map;
    if(!m) return;

    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < app->roster.count; i++) {
        NodeEntry* n = &app->roster.nodes[i];
        NodeDraw nd;
        if(!node_layout(canvas, m, n, &nd)) continue;

        bool focus = (n->node_id == m->focus_id);

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, nd.px, nd.py, nd.halo);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_circle(canvas, nd.px, nd.py, 4);
        if(focus) {
            canvas_draw_disc(canvas, nd.px, nd.py, 2);
            if(m->blink & 1) canvas_draw_circle(canvas, nd.px, nd.py, 6);
        } else {
            canvas_draw_dot(canvas, nd.px, nd.py);
        }

        canvas_set_color(canvas, ColorWhite);
        map_box(canvas, nd.tx - 1, nd.ty - 7, nd.tw + 2, 8);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, nd.tx, nd.ty, nd.tag);
    }
}

static void map_draw_scale(Canvas* canvas, MapState* m) {
    float mpp = 156543.034f * zm_cosf(m->lat * ZM_PI / 180.0f) / (float)(1 << m->z);
    if(mpp <= 0.0f) return;

    static const struct {
        int metres;
        const char* label;
    } steps[] = {
        {100, "100m"},
        {200, "200m"},
        {500, "500m"},
        {1000, "1km"},
        {2000, "2km"},
        {5000, "5km"},
        {10000, "10km"},
        {20000, "20km"},
    };

    int best = 0;
    for(size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        if((float)steps[i].metres / mpp <= 44.0f) best = (int)i;
    }
    int px = (int)((float)steps[best].metres / mpp);
    if(px < 8) px = 8;
    if(px > 44) px = 44;

    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorWhite);
    map_box(canvas, 1, MAP_H - 13, px + 6, 13);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 3, MAP_H - 6, steps[best].label);
    canvas_draw_line(canvas, 3, MAP_H - 3, 3 + px, MAP_H - 3);
    canvas_draw_line(canvas, 3, MAP_H - 5, 3, MAP_H - 3);
    canvas_draw_line(canvas, 3 + px, MAP_H - 5, 3 + px, MAP_H - 3);
}

static void map_draw_crosshair(Canvas* canvas) {
    const int cx = MAP_W / 2, cy = MAP_H / 2;
    canvas_set_color(canvas, ColorWhite);
    map_box(canvas, cx - 3, cy - 3, 7, 7);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, cx - 3, cy, cx + 3, cy);
    canvas_draw_line(canvas, cx, cy - 3, cx, cy + 3);
}

static void map_draw_offscreen_focus(Canvas* canvas, ZeroMeshApp* app) {
    MapState* m = app->map;
    if(!m->focus_id) return;

    for(uint8_t i = 0; i < app->roster.count; i++) {
        NodeEntry* n = &app->roster.nodes[i];
        if(n->node_id != m->focus_id || !n->has_position) continue;

        float gxf, gyf;
        latlon_global(n->latitude_i / 1e7f, n->longitude_i / 1e7f, m->z, &gxf, &gyf);
        int px = (int)(gxf * (float)m->tile_px) - m->gx;
        int py = (int)(gyf * (float)m->tile_px) - m->gy;
        if(px >= 7 && py >= 15 && px < MAP_W - 7 && py < MAP_H - 7) return;

        int dx = px - MAP_W / 2;
        int dy = py - MAP_H / 2;
        int ex = px < 6 ? 6 : (px > MAP_W - 7 ? MAP_W - 7 : px);
        int ey = py < 15 ? 15 : (py > MAP_H - 7 ? MAP_H - 7 : py);

        CanvasDirection dir;
        int ax = dx < 0 ? -dx : dx;
        int ay = dy < 0 ? -dy : dy;
        if(ax >= ay)
            dir = dx < 0 ? CanvasDirectionRightToLeft : CanvasDirectionLeftToRight;
        else
            dir = dy < 0 ? CanvasDirectionBottomToTop : CanvasDirectionTopToBottom;

        canvas_set_color(canvas, ColorWhite);
        map_box(canvas, ex - 5, ey - 5, 11, 11);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_triangle(canvas, ex, ey, 7, 5, dir);
        return;
    }
}

static void icon_towns(Canvas* c, int x, int y) {
    for(int i = 0; i < 3; i++) canvas_draw_line(c, x - 4, y - 2 + i * 2, x + 4, y - 2 + i * 2);
}

static void icon_labels(Canvas* c, int x, int y) {
    canvas_draw_line(c, x - 3, y + 3, x, y - 4);
    canvas_draw_line(c, x, y - 4, x + 3, y + 3);
    canvas_draw_line(c, x - 2, y + 1, x + 2, y + 1);
}

static void icon_home(Canvas* c, int x, int y) {
    canvas_draw_line(c, x - 4, y, x, y - 4);
    canvas_draw_line(c, x, y - 4, x + 4, y);
    canvas_draw_line(c, x - 3, y, x - 3, y + 3);
    canvas_draw_line(c, x + 3, y, x + 3, y + 3);
    canvas_draw_line(c, x - 3, y + 3, x + 3, y + 3);
}

static void icon_node(Canvas* c, int x, int y) {
    canvas_draw_circle(c, x, y, 4);
    canvas_draw_dot(c, x, y);
}

static void icon_zoom(Canvas* c, int x, int y) {
    canvas_draw_circle(c, x - 1, y - 1, 3);
    canvas_draw_line(c, x + 1, y + 1, x + 4, y + 4);
}

static void icon_gps(Canvas* c, int x, int y) {
    canvas_draw_dot(c, x - 3, y + 3);
    canvas_draw_line(c, x - 2, y + 2, x - 1, y + 3);
    canvas_draw_line(c, x - 1, y, x + 2, y + 3);
    canvas_draw_line(c, x, y - 2, x + 3, y + 1);
}

static const char* const TOOLBAR_NAMES[MAP_TOOLBAR_N] = {
    "Towns",
    "Names",
    "Home",
    "Node",
    "Zoom",
    "GPS",
};

static void map_draw_edges(Canvas* canvas, MapState* m) {
    canvas_set_color(canvas, ColorBlack);
    if(m->edge_w)
        for(int y = 10; y < MAP_H - 2; y += 4) map_box(canvas, 0, y, 2, 2);
    if(m->edge_e)
        for(int y = 10; y < MAP_H - 2; y += 4) map_box(canvas, MAP_W - 2, y, 2, 2);
    if(m->edge_n)
        for(int x = 2; x < MAP_W - 2; x += 4) map_box(canvas, x, 9, 2, 2);
    if(m->edge_s)
        for(int x = 2; x < MAP_W - 2; x += 4) map_box(canvas, x, MAP_H - 2, 2, 2);
}

static const char* bearing8(int dx, int dy) {
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    if(adx > ady * 2) return dx > 0 ? "E" : "W";
    if(ady > adx * 2) return dy > 0 ? "S" : "N";
    if(dx > 0) return dy > 0 ? "SE" : "NE";
    return dy > 0 ? "SW" : "NW";
}

static void map_draw_focus_range(Canvas* canvas, ZeroMeshApp* app) {
    MapState* m = app->map;
    if(!m->focus_id) return;

    for(uint8_t i = 0; i < app->roster.count; i++) {
        NodeEntry* n = &app->roster.nodes[i];
        if(n->node_id != m->focus_id || !n->has_position) continue;

        float gxf, gyf;
        latlon_global(n->latitude_i / 1e7f, n->longitude_i / 1e7f, m->z, &gxf, &gyf);
        int64_t dx = (int64_t)(gxf * (float)m->tile_px) - (m->gx + MAP_W / 2);
        int64_t dy = (int64_t)(gyf * (float)m->tile_px) - (m->gy + MAP_H / 2);

        uint64_t d2 = (uint64_t)(dx * dx + dy * dy);
        if(d2 > 0xFFFFFFFFull) d2 = 0xFFFFFFFFull;
        uint32_t px = zm_isqrt((uint32_t)d2);
        if(px < 4) return;

        float mpp = 156543.034f * zm_cosf(m->lat * ZM_PI / 180.0f) / (float)(1 << m->z);
        float metres = (float)px * mpp;

        char buf[24];
        const char* dir = (px < 2) ? "" : bearing8((int)dx, (int)dy);
        if(metres < 1000.0f) {
            snprintf(buf, sizeof(buf), "%d m %s", (int)metres, dir);
        } else {
            int km = (int)(metres / 1000.0f);
            int frac = ((int)(metres / 100.0f)) % 10;
            snprintf(buf, sizeof(buf), "%d.%d km %s", km, frac, dir);
        }

        canvas_set_font(canvas, FontSecondary);
        int w = canvas_string_width(canvas, buf);
        canvas_set_color(canvas, ColorWhite);
        map_box(canvas, MAP_W - w - 4, MAP_H - 11, w + 4, 11);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, MAP_W - w - 2, MAP_H - 3, buf);
        return;
    }
}

static void map_draw_gps(Canvas* canvas, ZeroMeshApp* app) {
    char buf[24];
    if(app->my_sats_seen) {
        snprintf(
            buf,
            sizeof(buf),
            "GPS %u sat%s",
            app->my_sats,
            app->my_has_fix ? " fix" : "");
    } else {
        snprintf(buf, sizeof(buf), "GPS no data");
    }

    canvas_set_font(canvas, FontSecondary);
    int w = canvas_string_width(canvas, buf) + 4;
    canvas_set_color(canvas, ColorWhite);
    map_box(canvas, 1, 1, w, 11);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 1, 1, w, 11);
    canvas_draw_str(canvas, 3, 9, buf);
}

static void map_draw_toolbar(Canvas* canvas, MapState* m) {
    const int h = 15;
    const int top = MAP_H - h;
    const int slot = MAP_W / MAP_TOOLBAR_N;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, top, MAP_W, h);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, top, MAP_W - 1, top);

    for(int i = 0; i < MAP_TOOLBAR_N; i++) {
        int cx = i * slot + slot / 2;
        int cy = top + h / 2 + 1;
        bool sel = (i == m->toolbar_sel);

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, i * slot, top + 1, slot, h - 1);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        switch(i) {
        case 0: icon_towns(canvas, cx, cy); break;
        case 1: icon_labels(canvas, cx, cy); break;
        case 2: icon_home(canvas, cx, cy); break;
        case 3: icon_node(canvas, cx, cy); break;
        case 4: icon_zoom(canvas, cx, cy); break;
        default: icon_gps(canvas, cx, cy); break;
        }
    }

    const char* label = TOOLBAR_NAMES[m->toolbar_sel];
    canvas_set_font(canvas, FontSecondary);
    int w = canvas_string_width(canvas, label);
    canvas_set_color(canvas, ColorWhite);
    map_box(canvas, 1, top - 10, w + 4, 10);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 3, top - 2, label);
}

static void map_draw_towns(Canvas* canvas, MapState* m) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, MAP_W, MAP_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(m->town_count == 0) {
        canvas_draw_str(canvas, 4, 30, "No place names here");
        canvas_draw_str(canvas, 4, 44, "Try zooming out");
        return;
    }

    const int rows = 5;
    int first = m->town_sel - rows / 2;
    if(first > m->town_count - rows) first = m->town_count - rows;
    if(first < 0) first = 0;

    for(int r = 0; r < rows; r++) {
        int i = first + r;
        if(i >= m->town_count) break;
        int y = 2 + r * 12;
        if(i == m->town_sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y, MAP_W, 12);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, 3, y + 9, m->towns[i].name);
    }
}

void render_map(Canvas* canvas, ZeroMeshApp* app) {
    MapState* m = app->map;
    if(!m) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 32, "Map: out of memory");
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, MAP_W, MAP_H, m->fb_pixels);

    map_draw_labels(canvas, app);
    map_draw_nodes(canvas, app);
    map_draw_offscreen_focus(canvas, app);
    map_draw_edges(canvas, m);
    map_draw_scale(canvas, m);
    if(!m->ui_toolbar) map_draw_focus_range(canvas, app);
    if(m->pan_active) map_draw_crosshair(canvas);

    canvas_set_font(canvas, FontSecondary);
    int w = canvas_string_width(canvas, m->status);
    canvas_set_color(canvas, ColorWhite);
    map_box(canvas, 0, 0, w + 4, 9);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 2, 7, m->status);

    if(m->ui_towns) {
        map_draw_towns(canvas, m);
        return;
    }
    if(m->ui_gps) map_draw_gps(canvas, app);
    if(m->ui_toolbar) map_draw_toolbar(canvas, m);

    if(m->pan_active) {
        const char* tag = "PAN";
        int pw = canvas_string_width(canvas, tag);
        canvas_set_color(canvas, ColorWhite);
        map_box(canvas, MAP_W - pw - 4, 0, pw + 4, 9);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, MAP_W - pw - 2, 7, tag);
    }
}

static void map_focus_index(ZeroMeshApp* app, int idx) {
    MapState* m = app->map;

    if(idx < 0 || idx >= (int)app->roster.count || !app->roster.nodes[idx].has_position) {
        m->lat = MAP_HOME_LAT;
        m->lon = MAP_HOME_LON;
        m->focus_id = 0;
        m->focus_idx = -1;
    } else {
        NodeEntry* nd = &app->roster.nodes[idx];
        m->lat = nd->latitude_i / 1e7f;
        m->lon = nd->longitude_i / 1e7f;
        m->focus_id = nd->node_id;
        m->focus_idx = idx;
    }

    m->reload = true;
    m->dirty = true;
}

static void map_cycle_focus(ZeroMeshApp* app, int dir) {
    MapState* m = app->map;
    int n = (int)app->roster.count;
    if(n <= 0) {
        map_focus_index(app, -1);
        return;
    }

    for(int step = 1; step <= n; step++) {
        int i = m->focus_idx + dir * step;
        while(i < 0)
            i += n;
        i %= n;
        if(app->roster.nodes[i].has_position) {
            map_focus_index(app, i);
            return;
        }
    }
    map_focus_index(app, -1);
}

bool map_focus_node(ZeroMeshApp* app, uint32_t node_id) {
    if(!app || !app->map || !node_id) return false;
    for(uint8_t i = 0; i < app->roster.count; i++) {
        if(app->roster.nodes[i].node_id != node_id) continue;
        map_focus_index(app, i);
        /* map_focus_index falls back to the home coordinate for a node
           with no fix, so report that rather than claiming success. */
        return app->roster.nodes[i].has_position;
    }
    return false;
}

/* zm_mercator_y has no closed-form inverse we can link against, but it is
   monotonic, so bisect it rather than pulling in sinh and atan. */
bool map_view_center(ZeroMeshApp* app, int32_t* lat_i, int32_t* lon_i) {
    if(!app || !app->map || !lat_i || !lon_i) return false;
    MapState* m = app->map;

    float n = (float)(1 << m->z);
    float gx = (float)(m->gx + MAP_W / 2) / (float)m->tile_px;
    float gy = (float)(m->gy + MAP_H / 2) / (float)m->tile_px;

    float target = gy / n;
    float lo = -85.0f, hi = 85.0f;
    for(int i = 0; i < 32; i++) {
        float mid = (lo + hi) * 0.5f;
        if(zm_mercator_y(mid) < target)
            hi = mid;
        else
            lo = mid;
    }

    *lat_i = (int32_t)((lo + hi) * 0.5f * 1e7f);
    *lon_i = (int32_t)((gx / n * 360.0f - 180.0f) * 1e7f);
    return true;
}

bool map_wants_key(ZeroMeshApp* app, InputKey key) {
    if(!app || !app->map) return false;
    MapState* m = app->map;

    if(m->ui_towns)
        return key == InputKeyUp || key == InputKeyDown || key == InputKeyOk ||
               key == InputKeyBack;

    if(m->ui_toolbar)
        return key == InputKeyLeft || key == InputKeyRight || key == InputKeyOk ||
               key == InputKeyBack || key == InputKeyDown;

    if(m->pan_active) return true;

    return key == InputKeyUp || key == InputKeyDown || key == InputKeyOk;
}

static void map_set_zoom(MapState* m, int nz) {
    int cx = m->gx + MAP_W / 2;
    int cy = m->gy + MAP_H / 2;
    while(nz > m->z) {
        cx *= 2;
        cy *= 2;
        m->z++;
    }
    while(nz < m->z) {
        cx /= 2;
        cy /= 2;
        m->z--;
    }
    m->gx = cx - MAP_W / 2;
    m->gy = cy - MAP_H / 2;
    m->have_good = false;
    map_clamp(m);
    m->dirty = true;
}

static void toolbar_activate(ZeroMeshApp* app) {
    MapState* m = app->map;
    switch(m->toolbar_sel) {
    case 0:
        m->want_towns = true;
        m->ui_toolbar = false;
        break;
    case 1:
        m->show_labels = !m->show_labels;
        m->dirty = true;
        break;
    case 2:
        if(!map_focus_node(app, app->my_node_num)) {
            map_focus_index(app, -1);
            set_status(app, "No position for this node");
        }
        m->ui_toolbar = false;
        m->pan_active = false;
        break;
    case 3:
        map_cycle_focus(app, 1);
        break;
    case 4:
        map_set_zoom(m, m->z + 1 > m->zoom_max ? m->zoom_min : m->z + 1);
        break;
    default:
        m->ui_gps = !m->ui_gps;
        break;
    }
}

void input_map(InputEvent* e, ZeroMeshApp* app) {
    MapState* m = app->map;
    if(!m || !e) return;

    if(m->ui_towns) {
        if(e->type != InputTypeShort && e->type != InputTypeRepeat) return;
        if(e->key == InputKeyUp && m->town_sel > 0) m->town_sel--;
        if(e->key == InputKeyDown && m->town_sel + 1 < (int)m->town_count) m->town_sel++;
        if(e->key == InputKeyBack) m->ui_towns = false;
        if(e->key == InputKeyOk && m->town_count) {
            m->gx = m->towns[m->town_sel].gx - MAP_W / 2;
            m->gy = m->towns[m->town_sel].gy - MAP_H / 2;
            m->have_good = false;
            map_clamp(m);
            m->ui_towns = false;
            m->dirty = true;
        }
        return;
    }

    if(m->ui_toolbar) {
        if(e->type != InputTypeShort && e->type != InputTypeRepeat) return;
        if(e->key == InputKeyLeft && m->toolbar_sel > 0) m->toolbar_sel--;
        if(e->key == InputKeyRight && m->toolbar_sel + 1 < MAP_TOOLBAR_N) m->toolbar_sel++;
        if(e->key == InputKeyBack || e->key == InputKeyDown) m->ui_toolbar = false;
        if(e->key == InputKeyOk) toolbar_activate(app);
        return;
    }

    if(!m->pan_active && e->key == InputKeyDown && e->type == InputTypeShort) {
        m->ui_toolbar = true;
        return;
    }

    if(e->key == InputKeyOk) {
        if(e->type == InputTypeLong) {
            m->pan_active = !m->pan_active;
        } else if(e->type == InputTypeShort) {
            int nz = m->z + 1;
            if(nz > m->zoom_max) nz = m->zoom_min;
            map_set_zoom(m, nz);
        }
        return;
    }

    if(e->key == InputKeyBack) {
        if(m->pan_active && e->type == InputTypeShort) m->pan_active = false;
        return;
    }

    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return;

    if(!m->pan_active) {
        if(e->key == InputKeyUp) map_cycle_focus(app, -1);
        return;
    }

    switch(e->key) {
    case InputKeyUp:
        m->gy -= MAP_PAN_STEP;
        break;
    case InputKeyDown:
        m->gy += MAP_PAN_STEP;
        break;
    case InputKeyLeft:
        m->gx -= MAP_PAN_STEP;
        break;
    case InputKeyRight:
        m->gx += MAP_PAN_STEP;
        break;
    default:
        return;
    }
    m->dirty = true;
}

bool map_alloc(ZeroMeshApp* app) {
    if(!app || app->map) return app && app->map;

    MapState* m = malloc(sizeof(MapState));
    if(!m) return false;
    memset(m, 0, sizeof(MapState));

    m->fb_pixels = malloc((size_t)((MAP_W + 7) / 8) * MAP_H);
    m->scratch = malloc(sizeof(carto_ipt) * MAP_SCRATCH_PT);
    if(!m->fb_pixels || !m->scratch) {
        free(m->fb_pixels);
        free(m->scratch);
        free(m);
        return false;
    }
    m->scratch_cap = MAP_SCRATCH_PT;

    m->pm = pmtiles_open(MAP_PMTILES);
    if(!m->pm) m->pm = pmtiles_open(MAP_PMTILES_OLD);
    size_t tile_buf = MAP_TILE_MAX;
    if(m->pm) {
        uint32_t biggest = pmtiles_max_tile_len(m->pm);
        if(biggest && biggest < tile_buf) {
            tile_buf = biggest;
        } else if(biggest > MAP_TILE_MAX) {
            FURI_LOG_W(
                TAG,
                "archive holds %lu byte tiles, capped at %u",
                (unsigned long)biggest,
                (unsigned)MAP_TILE_MAX);
        }
    }

    m->tile = malloc(tile_buf);
    m->tile_cap = m->tile ? tile_buf : 0;
    if(!m->tile) {
        FURI_LOG_E(
            TAG,
            "tile buffer %u failed, free heap %u",
            (unsigned)tile_buf,
            (unsigned)memmgr_get_free_heap());
    }

    carto_fb_init(&m->fb, MAP_W, MAP_H, CARTO_FMT_MONO1, m->fb_pixels);
    map_mono_style(&m->style);

    m->tile_px = 256;
    m->extent = 4096;
    m->show_labels = true;

    if(m->pm) {
        m->zoom_min = pmtiles_min_zoom(m->pm);
        m->zoom_max = pmtiles_max_zoom(m->pm);
    }
    if(m->zoom_min < MAP_MIN_Z || m->zoom_max > MAP_MAX_Z || m->zoom_min > m->zoom_max) {
        m->zoom_min = 12;
        m->zoom_max = 12;
    }
    m->z = m->zoom_max;

    app->map = m;

    map_focus_index(app, -1);
    map_recenter(m);
    m->reload = false;

    return true;
}

void map_free(ZeroMeshApp* app) {
    if(!app || !app->map) return;

    /* render_map reads app->map on the GUI thread, so detach under the
       lock before releasing anything it might still be drawing from. */
    furi_mutex_acquire(app->lock, FuriWaitForever);
    MapState* m = app->map;
    app->map = NULL;
    furi_mutex_release(app->lock);

    pmtiles_close(m->pm);
    free(m->tile);
    free(m->scratch);
    free(m->fb_pixels);
    free(m);
}
