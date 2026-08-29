#include "zeromesh_map.h"
#include "zeromesh_history.h"

#include <storage/storage.h>
#include <furi_hal_memory.h>
#include <core/memmgr.h>
#include <math.h>

#include "zeromesh_mvtlabel.h"
#include "zeromesh_pmtiles.h"
#include "mvt.h"
#include "carto/raster.h"
#include "carto/style.h"
#include "carto/framebuffer.h"

#define TAG "zeromesh_map"

#define MAP_DIR        "/ext/zeromesh/map"
#define MAP_PMTILES    "/ext/zeromesh/map.pmtiles"
#define MAP_TILE_MAX   65536
#define MAP_SCRATCH_PT 512
#define MAP_MAX_LABELS 10

typedef struct {
    int16_t ex, ey;
    char text[24];
} MapLabel;

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
    bool loaded;

    int z, tx, ty;
    int tile_px;
    int pan_x, pan_y;

    bool dirty;
    bool pan_mode;
    bool want_reload;
    int want_z;

    MapLabel labels[MAP_MAX_LABELS];
    uint8_t label_count;
    uint32_t extent;
    bool show_labels;

    char status[32];
};

#define MAP_HOME_LAT 43.4443f
#define MAP_HOME_LON (-71.6478f)

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

static void map_collect_labels(MapState* m);

static void latlon_to_tile(float lat, float lon, int z, int* tx, int* ty) {
    float n = (float)(1 << z);
    *tx = (int)((lon + 180.0f) / 360.0f * n);
    float r = lat * (float)M_PI / 180.0f;
    *ty = (int)((1.0f - asinhf(tanf(r)) / (float)M_PI) / 2.0f * n);
}

static void latlon_in_tile(float lat, float lon, int z, float* fx, float* fy) {
    float n = (float)(1 << z);
    float x = (lon + 180.0f) / 360.0f * n;
    float r = lat * (float)M_PI / 180.0f;
    float y = (1.0f - asinhf(tanf(r)) / (float)M_PI) / 2.0f * n;
    *fx = x - floorf(x);
    *fy = y - floorf(y);
}

static bool map_load_tile(ZeroMeshApp* app, int z, int tx, int ty) {
    MapState* m = app->map;
    if(!m) return false;

    m->loaded = false;
    m->tile_len = 0;
    bool ok = false;

    if(m->pm) {
        size_t got = 0;
        if(m->tile &&
           pmtiles_get_tile(m->pm, (uint8_t)z, (uint32_t)tx, (uint32_t)ty, m->tile, m->tile_cap, &got)) {
            m->tile_len = got;
            m->loaded = true;
            ok = true;
        }
    } else {

        char path[96];
        snprintf(path, sizeof(path), MAP_DIR "/%d/%d/%d.mvt", z, tx, ty);

        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* f = storage_file_alloc(storage);

        if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t sz = storage_file_size(f);
            if(sz > 0 && sz <= m->tile_cap && m->tile) {
                uint16_t got = storage_file_read(f, m->tile, (uint16_t)sz);
                if(got == sz) {
                    m->tile_len = (size_t)sz;
                    m->loaded = true;
                    ok = true;
                }
            }
            storage_file_close(f);
        }
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
    }

    m->z = z;
    m->tx = tx;
    m->ty = ty;
    m->dirty = true;

    snprintf(
        m->status, sizeof(m->status), ok ? "z%d %d/%d" : "z%d %d/%d missing", z, tx, ty);
    return ok;
}

void map_tick(ZeroMeshApp* app) {
    if(!app || !app->map) return;
    MapState* m = app->map;
    if(!m->fb_pixels || !m->dirty) return;

    if(m->want_reload) {
        m->want_reload = false;
        int ntx, nty;
        latlon_to_tile(MAP_HOME_LAT, MAP_HOME_LON, m->want_z, &ntx, &nty);
        map_load_tile(app, m->want_z, ntx, nty);
        float fx, fy;
        latlon_in_tile(MAP_HOME_LAT, MAP_HOME_LON, m->want_z, &fx, &fy);
        m->pan_x = (int)(fx * m->tile_px) - MAP_W / 2;
        m->pan_y = (int)(fy * m->tile_px) - MAP_H / 2;
    }

    const int margin = 32;
    if(m->pan_x < -margin) m->pan_x = -margin;
    if(m->pan_y < -margin) m->pan_y = -margin;
    if(m->pan_x > m->tile_px - MAP_W + margin) m->pan_x = m->tile_px - MAP_W + margin;
    if(m->pan_y > m->tile_px - MAP_H + margin) m->pan_y = m->tile_px - MAP_H + margin;

    memset(m->fb_pixels, 0, (size_t)m->fb.stride * MAP_H);

    if(m->loaded) {
        float ox = (float)(-m->pan_x);
        float oy = (float)(-m->pan_y);

        static const carto_layer_kind order[] = {
            CARTO_LAYER_WATER,
            CARTO_LAYER_ROAD,
        };
        for(size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
            carto_mvt_render_category(
                &m->fb,
                &m->style,
                m->tile,
                m->tile_len,
                order[i],
                ox,
                oy,
                (float)m->tile_px,
                m->z,
                m->scratch,
                m->scratch_cap);
        }

        map_collect_labels(m);
    } else {
        m->label_count = 0;
    }

    m->dirty = false;
}

static void label_cb(const char* name, int32_t ex, int32_t ey, void* ctx) {
    MapState* m = ctx;
    if(m->label_count >= MAP_MAX_LABELS) return;
    MapLabel* l = &m->labels[m->label_count];
    l->ex = (int16_t)ex;
    l->ey = (int16_t)ey;
    strncpy(l->text, name, sizeof(l->text) - 1);
    l->text[sizeof(l->text) - 1] = '\0';
    m->label_count++;
}

static void map_collect_labels(MapState* m) {
    m->label_count = 0;
    if(!m->loaded || !m->show_labels) return;

    static const char* const label_layers[] = {
        "place_labels",
        "street_labels",
        "water_polygons_labels",
        "water_lines_labels",
    };
    mvt_scan_labels(
        m->tile,
        m->tile_len,
        label_layers,
        sizeof(label_layers) / sizeof(label_layers[0]),
        label_cb,
        m,
        &m->extent);
}

static void map_draw_labels(Canvas* canvas, MapState* m) {
    if(!m->show_labels) return;
    canvas_set_font(canvas, FontSecondary);

    struct {
        int16_t x0, y0, x1, y1;
    } taken[MAP_MAX_LABELS];
    int ntaken = 0;

    float per = (float)m->tile_px / (float)(m->extent ? m->extent : 4096);

    for(uint8_t i = 0; i < m->label_count; i++) {
        MapLabel* l = &m->labels[i];
        int px = (int)(l->ex * per) - m->pan_x;
        int py = (int)(l->ey * per) - m->pan_y;
        if(px < -20 || py < 8 || px >= MAP_W || py >= MAP_H) continue;

        int w = canvas_string_width(canvas, l->text);
        if(px + w > MAP_W) px = MAP_W - w;
        if(px < 0) px = 0;

        int x0 = px - 1, y0 = py - 7, x1 = px + w + 1, y1 = py + 1;

        bool clash = false;
        for(int t = 0; t < ntaken; t++) {
            if(x0 < taken[t].x1 && x1 > taken[t].x0 && y0 < taken[t].y1 && y1 > taken[t].y0) {
                clash = true;
                break;
            }
        }
        if(clash) continue;

        if(ntaken < MAP_MAX_LABELS) {
            taken[ntaken].x0 = (int16_t)x0;
            taken[ntaken].y0 = (int16_t)y0;
            taken[ntaken].x1 = (int16_t)x1;
            taken[ntaken].y1 = (int16_t)y1;
            ntaken++;
        }

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, x0, y0, w + 2, 8);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, px, py, l->text);
    }
}

static void map_draw_nodes(Canvas* canvas, ZeroMeshApp* app) {
    MapState* m = app->map;
    if(!m) return;

    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < app->roster.count; i++) {
        NodeEntry* n = &app->roster.nodes[i];
        if(!n->has_position) continue;

        float lat = n->latitude_i / 1e7f;
        float lon = n->longitude_i / 1e7f;

        int ntx, nty;
        latlon_to_tile(lat, lon, m->z, &ntx, &nty);
        if(ntx != m->tx || nty != m->ty) continue;

        float fx, fy;
        latlon_in_tile(lat, lon, m->z, &fx, &fy);
        int px = (int)(fx * m->tile_px) - m->pan_x;
        int py = (int)(fy * m->tile_px) - m->pan_y;
        if(px < 0 || py < 0 || px >= MAP_W || py >= MAP_H) continue;

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, px, py, 4);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, px, py, 2);
        canvas_draw_circle(canvas, px, py, 4);

        const char* tag = n->has_name && n->short_name[0] ? n->short_name : NULL;
        char idbuf[8];
        if(!tag) {
            snprintf(idbuf, sizeof(idbuf), "%04lx", (unsigned long)(n->node_id & 0xFFFF));
            tag = idbuf;
        }
        int tw = canvas_string_width(canvas, tag);
        int tx = px + 6;
        if(tx + tw > MAP_W) tx = px - 6 - tw;
        if(tx < 0) tx = 0;
        int ty = py + 3;
        if(ty < 8) ty = 8;
        if(ty > MAP_H - 1) ty = MAP_H - 1;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, tx - 1, ty - 7, tw + 2, 8);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, tx, ty, tag);
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

    map_draw_labels(canvas, m);
    map_draw_nodes(canvas, app);

    canvas_set_font(canvas, FontSecondary);
    int w = canvas_string_width(canvas, m->status);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, w + 4, 9);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 2, 7, m->status);

    if(m->pan_mode) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, MAP_W - 26, 0, 26, 9);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, MAP_W - 24, 7, "PAN");
    }
}

bool map_wants_key(ZeroMeshApp* app, InputKey key) {

    if(!app || !app->map) return false;
    if(key == InputKeyUp || key == InputKeyDown || key == InputKeyOk) return true;
    if(key == InputKeyLeft || key == InputKeyRight) return app->map->pan_mode;
    return false;
}

void input_map(InputEvent* e, ZeroMeshApp* app) {
    MapState* m = app->map;
    if(!m || !e) return;

    const int step = 16;

    if(e->key == InputKeyOk) {
        if(e->type == InputTypeLong) {

            int nz = m->z + 1;
            if(nz > MAP_MAX_Z) nz = MAP_MIN_Z;
            m->want_z = nz;
            m->want_reload = true;
            m->dirty = true;
        } else if(e->type == InputTypeShort) {
            m->pan_mode = !m->pan_mode;
        }
        return;
    }

    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return;

    switch(e->key) {
    case InputKeyUp:
        m->pan_y -= step;
        m->dirty = true;
        break;
    case InputKeyDown:
        m->pan_y += step;
        m->dirty = true;
        break;
    case InputKeyLeft:
        if(m->pan_mode) {
            m->pan_x -= step;
            m->dirty = true;
        }
        break;
    case InputKeyRight:
        if(m->pan_mode) {
            m->pan_x += step;
            m->dirty = true;
        }
        break;
    default:
        break;
    }
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
    size_t tile_buf = 0;
    if(m->pm) {
        tile_buf = pmtiles_max_tile_len(m->pm);
        if(tile_buf > MAP_TILE_MAX) tile_buf = MAP_TILE_MAX;
    }
    if(tile_buf == 0) tile_buf = 32 * 1024;

    m->tile = malloc(tile_buf);
    m->tile_cap = m->tile ? tile_buf : 0;
    if(!m->tile) {
    }

    FURI_LOG_I(
        TAG,
        "map_alloc ok, tile buf %u, source %s, free heap %u",
        (unsigned)tile_buf,
        m->pm ? "pmtiles" : "loose",
        (unsigned)memmgr_get_free_heap());

    carto_fb_init(&m->fb, MAP_W, MAP_H, CARTO_FMT_MONO1, m->fb_pixels);
    map_mono_style(&m->style);

    m->tile_px = 256;
    m->z = 12;
    m->extent = 4096;
    m->show_labels = true;
    app->map = m;

    int tx, ty;
    latlon_to_tile(MAP_HOME_LAT, MAP_HOME_LON, m->z, &tx, &ty);
    map_load_tile(app, m->z, tx, ty);

    float fx, fy;
    latlon_in_tile(MAP_HOME_LAT, MAP_HOME_LON, m->z, &fx, &fy);
    m->pan_x = (int)(fx * m->tile_px) - MAP_W / 2;
    m->pan_y = (int)(fy * m->tile_px) - MAP_H / 2;

    return true;
}

void map_free(ZeroMeshApp* app) {
    if(!app || !app->map) return;
    MapState* m = app->map;
    pmtiles_close(m->pm);
    free(m->tile);
    free(m->scratch);
    free(m->fb_pixels);
    free(m);
    app->map = NULL;
}
