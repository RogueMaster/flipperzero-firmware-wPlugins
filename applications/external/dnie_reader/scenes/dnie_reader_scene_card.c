#include "../dnie_reader.h"
#include "../dnie_jpeg.h"
#include "../dnie_sounds.h"
#include "dnie_reader_scene.h"

#include <furi_hal_rtc.h>

#define TAG              "DnieCard"
#define CARD_PHOTO_MAX_W 56
#define CARD_PHOTO_MAX_H 64
#define CARD_FILE_MAX    (64 * 1024)
#define CARD_HEAP_MARGIN (6 * 1024) // margen para no dejar al sistema sin memoria

typedef struct {
    DnieApp* app;
} CardModel;

static void card_toast_draw(Canvas* canvas, const char* text);

// ---------------------------------------------------------------------------
// Memoria: cuanto necesita decodificar la foto a una altura dada
// ---------------------------------------------------------------------------

static uint32_t ceil_shift(uint32_t v, uint32_t s) {
    return (v + (1u << s) - 1) >> s;
}

// Estimacion del pico del decodificador (buffer float de la imagen + code-block + prefijo).
// En *largest devuelve el mayor bloque contiguo que pedira (el buffer de la imagen).
static size_t decode_need(const DnieJ2kInfo* j, uint16_t min_h, size_t prefix, size_t* largest) {
    uint32_t w = j->width, h = j->height;
    for(int r = 0; r <= j->levels; r++) {
        uint32_t rh = ceil_shift(j->height, (uint32_t)(j->levels - r));
        w = ceil_shift(j->width, (uint32_t)(j->levels - r));
        h = rh;
        if(rh >= min_h) break;
    }
    *largest = (size_t)w * h * 4;
    if(prefix > *largest) *largest = prefix;
    return (size_t)w * h * 4 + (size_t)((w + 1) / 2) * ((h + 1) / 2) * 4 + prefix + 4096;
}

// ---------------------------------------------------------------------------
// Decodificacion de la foto -> pixel art
// ---------------------------------------------------------------------------

static uint8_t* read_prefix(const char* path, size_t want, size_t* got, size_t* file_size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    uint8_t* buf = NULL;
    *got = 0;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        *file_size = storage_file_size(f);
        size_t n = want < *file_size ? want : *file_size;
        buf = malloc(n ? n : 1);
        *got = storage_file_read(f, buf, n);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return buf;
}

// JPEG (pasaportes): se decodifica a 1/8 leyendo de la SD a trozos
static size_t card_file_read(void* ctx, uint8_t* buf, size_t max) {
    return storage_file_read((File*)ctx, buf, max);
}

static void card_photo_to_pixelart(DnieApp* app, DnieGrayImage* gray) {
    app->photo_decoded_w = gray->width;
    app->photo_decoded_h = gray->height;
    app->photo_reduce = gray->reduce;
    if(!app->pix_src) app->pix_src = malloc(sizeof(DniePixelSource));
    bool ok = app->pix_src &&
              dnie_pixelart_prepare(
                  gray->pixels,
                  gray->width,
                  gray->height,
                  CARD_PHOTO_MAX_W,
                  CARD_PHOTO_MAX_H,
                  app->pix_src) &&
              dnie_pixelart_dither(app->pix_src, app->dither, &app->pix);
    dnie_gray_free(gray);
    if(!ok) app->photo_decode_result = DnieJ2kErrorMemory;
}

static void card_decode_jpeg(DnieApp* app) {
    // ~1.5 KB de estado + la imagen a 1/8 (unos 4 KB en una foto de pasaporte)
    if(memmgr_get_free_heap() < 8192 + CARD_HEAP_MARGIN ||
       memmgr_heap_get_max_free_block() < 6144) {
        app->photo_decode_result = DnieJ2kErrorMemory;
        return;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, app->photo.path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        DnieGrayImage gray;
        uint32_t t0 = furi_get_tick();
        app->photo_decode_result = dnie_jpeg_decode_dc(card_file_read, f, &gray);
        FURI_LOG_I(TAG, "JPEG: %d en %lu ms", app->photo_decode_result, furi_get_tick() - t0);
        storage_file_close(f);
        if(app->photo_decode_result == DnieJ2kOk) card_photo_to_pixelart(app, &gray);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void card_decode_photo_inner(DnieApp* app);

static void card_decode_photo(DnieApp* app) {
    card_decode_photo_inner(app);
    // Privacidad: la foto a resolucion completa no se queda en la SD sin cifrar
    if(app->photo.saved) dnie_photo_wipe();
}

static void card_decode_photo_inner(DnieApp* app) {
    app->photo_decode_tried = true;
    app->photo_decode_result = DnieJ2kErrorFormat;
    if(app->photo.saved && app->photo.dg2.image_type == DnieImageJpeg && app->photo.path) {
        card_decode_jpeg(app);
        return;
    }
    if(!app->photo.saved || app->photo.dg2.image_type != DnieImageJpeg2000 || !app->photo.j2k_ok)
        return;

    // Altura objetivo: 64 (77x95 en un DNI) y, si no hay memoria, 48 (39x48) o 32
    static const uint16_t heights[] = {CARD_PHOTO_MAX_H, 48, 32};
    // En orden LRCP la baja resolucion va al principio: probamos con prefijos crecientes
    static const size_t prefixes[] = {6 * 1024, 12 * 1024, CARD_FILE_MAX};

    for(size_t hi = 0; hi < COUNT_OF(heights); hi++) {
        for(size_t pi = 0; pi < COUNT_OF(prefixes); pi++) {
            size_t max_block = memmgr_heap_get_max_free_block();
            size_t largest;
            size_t need = decode_need(&app->photo.j2k, heights[hi], prefixes[pi], &largest);
            FURI_LOG_I(
                TAG,
                "foto h>=%u, prefijo %u: necesita ~%u, bloque libre %u, heap %u",
                heights[hi],
                prefixes[pi],
                need,
                max_block,
                memmgr_get_free_heap());
            // Total libre suficiente Y un hueco contiguo para el buffer grande
            if(need + CARD_HEAP_MARGIN > memmgr_get_free_heap() ||
               largest + CARD_HEAP_MARGIN > max_block) {
                app->photo_decode_result = DnieJ2kErrorMemory;
                break; // probar con una altura menor
            }

            size_t got = 0, file_size = 0;
            uint8_t* data = read_prefix(app->photo.path, prefixes[pi], &got, &file_size);
            if(!data) return;
            DnieGrayImage gray;
            uint32_t t0 = furi_get_tick();
            DnieJ2kResult r = dnie_j2k_decode_luma(data, got, heights[hi], &gray);
            free(data);
            FURI_LOG_I(TAG, "decode %u B: %d en %lu ms", got, r, furi_get_tick() - t0);
            app->photo_decode_result = r;
            if(r == DnieJ2kErrorTruncated && got < file_size) continue; // pedir mas bytes
            if(r != DnieJ2kOk) return;

            card_photo_to_pixelart(app, &gray);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Fechas
// ---------------------------------------------------------------------------

// "AAAAMMDD" -> "DD/MM/YYYY"
static void format_full_date(char* out, size_t size, const char* yyyymmdd) {
    snprintf(
        out, size, "%c%c/%c%c/%.4s", yyyymmdd[6], yyyymmdd[7], yyyymmdd[4], yyyymmdd[5], yyyymmdd);
}

// Dias hasta el proximo cumpleanos (0 = hoy)
static int days_to_birthday(const char* yyyymmdd) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    int month = (yyyymmdd[4] - '0') * 10 + (yyyymmdd[5] - '0');
    int day = (yyyymmdd[6] - '0') * 10 + (yyyymmdd[7] - '0');
    if(month < 1 || month > 12 || day < 1 || day > 31) return -1;

    DateTime today = {.year = now.year, .month = now.month, .day = now.day};
    DateTime next = {.year = now.year, .month = (uint8_t)month, .day = (uint8_t)day};
    // 29 de febrero en ano no bisiesto: se celebra el 28
    if(month == 2 && day == 29 && !datetime_is_leap_year(next.year)) next.day = 28;
    uint32_t t_today = datetime_datetime_to_timestamp(&today);
    uint32_t t_next = datetime_datetime_to_timestamp(&next);
    if(t_next < t_today) {
        next.year++;
        next.day = (uint8_t)day;
        if(month == 2 && day == 29 && !datetime_is_leap_year(next.year)) next.day = 28;
        t_next = datetime_datetime_to_timestamp(&next);
    }
    return (int)((t_next - t_today) / 86400);
}

// ---------------------------------------------------------------------------
// Dibujo
// ---------------------------------------------------------------------------

static void draw_fit(Canvas* canvas, int x, int y, const char* str, int max_w) {
    char buf[32];
    strlcpy(buf, str, sizeof(buf));
    size_t len = strlen(buf);
    while(len > 0 && canvas_string_width(canvas, buf) > max_w)
        buf[--len] = '\0';
    canvas_draw_str(canvas, x, y, buf);
}

// Nombre en negrita + apellidos en dos lineas
static void draw_names(Canvas* canvas, int x, int w, const char* given, const char* surnames) {
    char line[32];
    canvas_set_font(canvas, FontPrimary);
    strlcpy(line, given, sizeof(line));
    char* sp = strchr(line, ' ');
    if(sp) *sp = '\0';
    draw_fit(canvas, x, 9, line, w);

    canvas_set_font(canvas, FontSecondary);
    strlcpy(line, surnames, sizeof(line));
    sp = strchr(line, ' ');
    if(sp) {
        *sp = '\0';
        draw_fit(canvas, x, 19, line, w);
        draw_fit(canvas, x, 28, sp + 1, w);
    } else {
        draw_fit(canvas, x, 19, line, w);
    }
}

// Lectura renombrada: el nombre puesto sustituye a nombre y apellidos.
// Primera palabra en grande, el resto debajo (como los apellidos).
static void draw_alias(Canvas* canvas, int x, int w, const char* alias) {
    const char* sp = strchr(alias, ' ');
    draw_names(canvas, x, w, alias, sp ? sp + 1 : "");
}

static void card_draw(Canvas* canvas, void* model) {
    DnieApp* app = ((CardModel*)model)->app;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Foto (o un marco vacio)
    int pw = CARD_PHOTO_MAX_W - 4;
    if(app->photo_decode_result == DnieJ2kOk && app->pix.width) {
        pw = app->pix.width;
        int py = (64 - app->pix.height) / 2;
        canvas_draw_xbm(canvas, 0, py, app->pix.width, app->pix.height, app->pix.xbm);
    } else {
        canvas_draw_frame(canvas, 0, 0, pw, 64);
        canvas_set_font(canvas, FontSecondary);
        bool no_mem = app->photo_decode_result == DnieJ2kErrorMemory;
        canvas_draw_str_aligned(canvas, pw / 2, 28, AlignCenter, AlignCenter, "No");
        canvas_draw_str_aligned(
            canvas, pw / 2, 38, AlignCenter, AlignCenter, no_mem ? "memory" : "photo");
    }
    canvas_draw_line(canvas, pw + 1, 0, pw + 1, 63);

    int x = pw + 4, w = 128 - x;
    char line[32];

    if(app->viewing_album) {
        // Album: nombre, cumpleanos y cuenta atras
        if(app->viewing_saved && app->saved_alias[0]) {
            draw_alias(canvas, x, w, app->saved_alias);
        } else {
            draw_names(canvas, x, w, app->album_given, app->album_surnames);
        }
        draw_fit(
            canvas,
            x,
            40,
            app->album_issuer[0] ? dnie_country_name(app->album_issuer) : "Birthday:",
            w);
        format_full_date(line, sizeof(line), app->album_birth);
        draw_fit(canvas, x, 50, line, w);
        int days = days_to_birthday(app->album_birth);
        if(days == 0) {
            snprintf(line, sizeof(line), "TODAY!");
        } else if(days > 0) {
            snprintf(line, sizeof(line), "%d days to go", days);
        } else {
            line[0] = '\0';
        }
        draw_fit(canvas, x, 60, line, w);
    } else {
        const DnieMrz* m = &app->mrz;
        if(app->viewing_saved && app->saved_alias[0]) {
            draw_alias(canvas, x, w, app->saved_alias);
        } else {
            draw_names(canvas, x, w, m->given_names, m->surnames);
        }
        draw_fit(canvas, x, 40, m->personal_number, w);
        char full[9];
        dnie_birth_full(m->birth, full);
        format_full_date(line, sizeof(line), full);
        draw_fit(canvas, x, 50, line, w);
        snprintf(line, sizeof(line), "%s  %c", dnie_country_name(m->issuer), m->sex);
        draw_fit(canvas, x, 60, line, w);

        // Indicador de integridad (solo en lecturas nuevas: las guardadas no conservan el SOD)
        if(!app->viewing_saved) {
            bool bad = dnie_auth_integrity_failed(&app->auth) ||
                       app->auth.ca.result == DnieCaFailed ||
                       app->auth.pa.result == DniePaBadSignature ||
                       app->auth.csca.result == DnieCscaBadSignature;
            const char* badge = app->demo                                     ? "DEMO" :
                                dnie_auth_integrity_failed(&app->auth)        ? "TAMPER" :
                                app->auth.pa.result == DniePaBadSignature     ? "SIG!" :
                                app->auth.csca.result == DnieCscaBadSignature ? "CSCA!" :
                                app->auth.ca.result == DnieCaFailed           ? "CA FAIL" :
                                dnie_auth_all_ok(&app->auth)                  ? "GENUINE" :
                                dnie_auth_data_ok(&app->auth)                 ? "DATA OK" :
                                dnie_auth_integrity_ok(&app->auth)            ? "SOD OK" :
                                                                                NULL;
            if(badge) {
                // Sello sobre la parte baja de la foto, con fondo propio para leerse sobre el tramado
                canvas_set_font(canvas, FontSecondary);
                int bw = canvas_string_width(canvas, badge) + 6;
                int bx = (pw - bw) / 2;
                if(bx < 0) bx = 0;
                if(bad) {
                    canvas_draw_box(canvas, bx, 52, bw, 11);
                    canvas_set_color(canvas, ColorWhite);
                    canvas_draw_str(canvas, bx + 3, 61, badge);
                    canvas_set_color(canvas, ColorBlack);
                } else {
                    canvas_set_color(canvas, ColorWhite);
                    canvas_draw_box(canvas, bx, 52, bw, 11);
                    canvas_set_color(canvas, ColorBlack);
                    canvas_draw_rframe(canvas, bx, 52, bw, 11, 2);
                    canvas_draw_str(canvas, bx + 3, 61, badge);
                }
            }
        }
    }

    if(app->toast) card_toast_draw(canvas, app->toast);
}

// ---------------------------------------------------------------------------
// Avisos temporales
// ---------------------------------------------------------------------------

static void card_toast_draw(Canvas* canvas, const char* text) {
    canvas_set_font(canvas, FontPrimary);
    int w = canvas_string_width(canvas, text) + 10;
    int x = (128 - w) / 2;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, 22, w, 18);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, 22, w, 18, 3);
    canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, text);
}

static void card_toast_timer(void* context) {
    DnieApp* app = context;
    app->toast = NULL;
    with_view_model(app->card_view, CardModel * model, { UNUSED(model); }, true);
}

static void card_show_toast(DnieApp* app, const char* text, uint32_t ms) {
    app->toast = text;
    if(!app->toast_timer) {
        app->toast_timer = furi_timer_alloc(card_toast_timer, FuriTimerTypeOnce, app);
    }
    furi_timer_start(app->toast_timer, furi_ms_to_ticks(ms));
    with_view_model(app->card_view, CardModel * model, { UNUSED(model); }, true);
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------

static bool card_input(InputEvent* event, void* context) {
    DnieApp* app = context;

    // Mantener abajo en una lectura guardada: borrarla
    if(event->key == InputKeyDown && event->type == InputTypeLong && app->viewing_saved) {
        dnie_store_delete(app->saved_path);
        notification_message(app->notifications, &sequence_success);
        view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventCardDeleted);
        return true;
    }
    // Mantener OK en una lectura guardada: cambiarle el nombre
    if(event->key == InputKeyOk && event->type == InputTypeLong && app->viewing_saved) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventCardRename);
        return true;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyUp) {
        if(app->demo) {
            card_show_toast(app, "Demo: not saved", 1500);
        } else if(app->viewing_saved) {
            card_show_toast(app, "Already saved", 1500);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventCardSave);
        }
        return true;
    }
    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        if(app->photo_decode_result != DnieJ2kOk || !app->pix_src) return true;
        int d = (int)app->dither + (event->key == InputKeyRight ? 1 : DnieDitherCount - 1);
        app->dither = (DnieDither)(d % DnieDitherCount);
        dnie_pixelart_dither(app->pix_src, app->dither, &app->pix);
        with_view_model(app->card_view, CardModel * model, { UNUSED(model); }, true);
        return true;
    }
    if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventCardDetails);
        return true;
    }
    return false; // Atras lo gestiona el SceneManager
}

// ---------------------------------------------------------------------------
// Escena
// ---------------------------------------------------------------------------

void dnie_scene_card_on_enter(void* context) {
    DnieApp* app = context;
    if(!app->photo_decode_tried) card_decode_photo(app);

    view_set_context(app->card_view, app);
    view_set_draw_callback(app->card_view, card_draw);
    view_set_input_callback(app->card_view, card_input);
    with_view_model(app->card_view, CardModel * model, { model->app = app; }, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewCard);

    // Al venir del menu de guardar: su resultado. Recien leido: recordar como guardar.
    if(app->pending_toast) {
        card_show_toast(app, app->pending_toast, 1800);
        app->pending_toast = NULL;
    } else if(app->card_hint_pending) {
        app->card_hint_pending = false;
        card_show_toast(app, "Up: save", 2200);
    }
}

bool dnie_scene_card_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;
    // Al salir del carnet (Atras) se acaba la musica
    if(event.type == SceneManagerEventTypeBack) dnie_sound_stop();
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DnieCustomEventCardRename) {
            scene_manager_next_scene(app->scene_manager, DnieSceneRename);
            consumed = true;
        } else if(event.event == DnieCustomEventCardDetails) {
            scene_manager_next_scene(app->scene_manager, DnieSceneResult);
            consumed = true;
        } else if(event.event == DnieCustomEventCardSave) {
            scene_manager_next_scene(app->scene_manager, DnieSceneSaveMenu);
            consumed = true;
        } else if(event.event == DnieCustomEventCardDeleted) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DnieSceneSavedList);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Lectura guardada: volver a la lista. Lectura nueva: al menu (no a la de lectura)
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, app->viewing_saved ? DnieSceneSavedList : DnieSceneStart);
        consumed = true;
    }
    return consumed;
}

void dnie_scene_card_on_exit(void* context) {
    DnieApp* app = context;
    if(app->toast_timer) furi_timer_stop(app->toast_timer);
    app->toast = NULL;
}
