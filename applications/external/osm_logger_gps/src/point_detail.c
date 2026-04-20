#include <furi.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "point_detail.h"

// Buffer de la ligne JSONL à afficher (partagé entre set et draw).
#define RAW_MAX 320
static char g_raw[RAW_MAX];

// Extrait un champ JSON string "key":"value" ou number "key":value.
// Retourne true si trouvé, copie dans out (null-terminé).
static bool extract_field(const char* key, char* out, size_t out_size) {
    if(!out || out_size == 0) return false;
    out[0] = '\0';

    // Cherche "key"
    char needle[24];
    int nlen = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if(nlen <= 0) return false;

    const char* p = strstr(g_raw, needle);
    if(!p) return false;
    p += nlen;
    if(*p != ':') return false;
    p++;

    // Valeur string (quote) ou nombre (jusqu'à ',' ou '}')
    if(*p == '"') {
        p++;
        size_t o = 0;
        while(*p && *p != '"' && o < out_size - 1) out[o++] = *p++;
        out[o] = '\0';
    } else {
        size_t o = 0;
        while(*p && *p != ',' && *p != '}' && o < out_size - 1) out[o++] = *p++;
        out[o] = '\0';
    }
    return true;
}

static void point_detail_draw_callback(Canvas* canvas, void* ctx) {
    (void)ctx;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Point details");

    canvas_set_font(canvas, FontSecondary);

    char time[32] = "--";
    char tag[64] = "--";
    char lat[16] = "--";
    char lon[16] = "--";
    char alt[12] = "--";
    char hdop[8] = "--";
    char sats[6] = "--";
    char note[40] = "";
    extract_field("time", time, sizeof(time));
    extract_field("tag", tag, sizeof(tag));
    extract_field("lat", lat, sizeof(lat));
    extract_field("lon", lon, sizeof(lon));
    extract_field("alt", alt, sizeof(alt));
    extract_field("hdop", hdop, sizeof(hdop));
    extract_field("sats", sats, sizeof(sats));
    extract_field("note", note, sizeof(note));

    // Garde juste HH:MM:SS de l'ISO
    char short_time[10] = "--:--:--";
    if(strlen(time) >= 19) {
        memcpy(short_time, &time[11], 8);
        short_time[8] = '\0';
    }

    char line[96];

    elements_multiline_text_aligned(canvas, 64, 14, AlignCenter, AlignTop, short_time);

    elements_multiline_text_aligned(canvas, 64, 24, AlignCenter, AlignTop, tag);

    snprintf(line, sizeof(line), "%s, %s", lat, lon);
    elements_multiline_text_aligned(canvas, 64, 34, AlignCenter, AlignTop, line);

    snprintf(line, sizeof(line), "alt=%sm HDOP=%s sats=%s", alt, hdop, sats);
    elements_multiline_text_aligned(canvas, 64, 44, AlignCenter, AlignTop, line);

    if(note[0]) {
        snprintf(line, sizeof(line), "note: %s", note);
        elements_multiline_text_aligned(canvas, 64, 54, AlignCenter, AlignTop, line);
    }

    elements_multiline_text_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back");
}

static bool point_detail_input_callback(InputEvent* event, void* ctx) {
    (void)event;
    (void)ctx;
    return false; // Back -> previous_callback
}

static uint32_t point_detail_previous_callback(void* ctx) {
    (void)ctx;
    return AppViewLastPoints;
}

void app_show_point_detail(App* app, const char* raw_jsonl) {
    strncpy(g_raw, raw_jsonl ? raw_jsonl : "", sizeof(g_raw) - 1);
    g_raw[sizeof(g_raw) - 1] = '\0';

    if(!app->point_detail_view) {
        View* view = view_alloc();
        view_set_draw_callback(view, point_detail_draw_callback);
        view_set_input_callback(view, point_detail_input_callback);
        view_set_previous_callback(view, point_detail_previous_callback);

        app->point_detail_view = view;
        view_dispatcher_add_view(app->dispatcher, AppViewPointDetail, view);
    }
    view_dispatcher_switch_to_view(app->dispatcher, AppViewPointDetail);
}

void app_stop_point_detail(App* app) {
    if(app->point_detail_view) {
        view_dispatcher_remove_view(app->dispatcher, AppViewPointDetail);
        view_free(app->point_detail_view);
        app->point_detail_view = NULL;
    }
}
