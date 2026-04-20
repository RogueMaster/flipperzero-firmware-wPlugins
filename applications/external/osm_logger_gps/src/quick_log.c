#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_rtc.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "quick_log.h"
#include "presets.h"
#include "notes_cache.h"
#include "storage_helpers.h"

// Récupère le seuil HDOP depuis les settings (0 = gate désactivé)
// Retourne un float exploitable par la comparaison (ou 99999 si off).
static float quick_log_hdop_max(const App* app) {
    if(app->settings.hdop_max_x10 == 0) return 99999.0f;
    return (float)app->settings.hdop_max_x10 / 10.0f;
}

typedef struct {
    bool has_fix;
    float lat;
    float lon;
    float hdop;
    float altitude;
    uint8_t sats;
    uint8_t preset_idx;
    uint8_t variant_idx;
    uint8_t variant_count;
    uint16_t session_count;
    uint32_t total_count;
    uint32_t fix_age_s;
    bool fix_ever;
    bool preview_pending;
    bool duplicate_warning;
    float duplicate_dist_m;
    char last_error[64];
    char note[64];
    bool recent_save;
    char last_saved_preset[24];
    char last_saved_time[8];

    // Averaging
    bool averaging;
    uint32_t avg_elapsed_s;
    uint32_t avg_total_s;
    uint32_t avg_samples;
    float avg_min_hdop;
    float avg_cur_lat;
    float avg_cur_lon;

    // Overlay "Saving..." (affiché entre OK et l'exécution du save au tick)
    bool saving;
} QuickLogModel;

static void quick_log_draw_callback(Canvas* canvas, void* ctx) {
    QuickLogModel* m = (QuickLogModel*)ctx;
    const Preset* p = presets_get(m->preset_idx);
    if(!p) p = presets_get(0);
    if(!p) return;

    canvas_clear(canvas);

    // Overlay "Saving..." : affiché entre OK et l'exécution du save au tick
    if(m->saving) {
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Saving...");
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(
            canvas, 64, 40, AlignCenter, AlignTop, "writing to SD");
        return;
    }

    // Overlay averaging : collecte active de samples GPS
    if(m->averaging) {
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Averaging...");
        canvas_set_font(canvas, FontSecondary);

        char line[48];
        snprintf(line, sizeof(line), "%lu / %lu s",
                 (unsigned long)m->avg_elapsed_s, (unsigned long)m->avg_total_s);
        elements_multiline_text_aligned(canvas, 64, 16, AlignCenter, AlignTop, line);

        snprintf(line, sizeof(line), "%lu samples  HDOP=%.1f",
                 (unsigned long)m->avg_samples, (double)m->avg_min_hdop);
        elements_multiline_text_aligned(canvas, 64, 28, AlignCenter, AlignTop, line);

        if(m->avg_samples > 0) {
            snprintf(line, sizeof(line), "%.6f", (double)m->avg_cur_lat);
            elements_multiline_text_aligned(canvas, 64, 40, AlignCenter, AlignTop, line);
            snprintf(line, sizeof(line), "%.6f", (double)m->avg_cur_lon);
            elements_multiline_text_aligned(canvas, 64, 48, AlignCenter, AlignTop, line);
        } else {
            elements_multiline_text_aligned(
                canvas, 64, 44, AlignCenter, AlignTop, "(waiting for fix)");
        }

        elements_multiline_text_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "Back to cancel");
        return;
    }

    // Overlay erreur (SD absente/pleine) : prioritaire sur tout le reste
    if(m->last_error[0]) {
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 8, AlignCenter, AlignTop, "SAVE FAILED");
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(
            canvas, 64, 28, AlignCenter, AlignTop, m->last_error);
        elements_multiline_text_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "Press any key");
        return;
    }

    // Overlay doublon détecté
    if(m->duplicate_warning) {
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Duplicate?");
        canvas_set_font(canvas, FontSecondary);

        char msg[64];
        snprintf(
            msg, sizeof(msg), "Same tag %.0fm away", (double)m->duplicate_dist_m);
        elements_multiline_text_aligned(canvas, 64, 18, AlignCenter, AlignTop, msg);

        char tag[64];
        preset_build_tag(p, m->variant_idx, tag, sizeof(tag));
        char* sc = strchr(tag, ';');
        if(sc) {
            *sc = '\0';
            elements_multiline_text_aligned(canvas, 64, 30, AlignCenter, AlignTop, tag);
            elements_multiline_text_aligned(canvas, 64, 38, AlignCenter, AlignTop, sc + 1);
        } else {
            elements_multiline_text_aligned(canvas, 64, 34, AlignCenter, AlignTop, tag);
        }

        elements_multiline_text_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "OK save  Back cancel");
        return;
    }

    // Mode preview (confirmation) : écran distinct et pédagogique.
    if(m->preview_pending) {
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(
            canvas, 64, 2, AlignCenter, AlignTop, "Save this point?");

        canvas_set_font(canvas, FontSecondary);
        char tag[64];
        preset_build_tag(p, m->variant_idx, tag, sizeof(tag));
        // Multi-tag : split sur 2 lignes
        char* sc = strchr(tag, ';');
        if(sc) {
            *sc = '\0';
            elements_multiline_text_aligned(canvas, 64, 16, AlignCenter, AlignTop, tag);
            elements_multiline_text_aligned(canvas, 64, 24, AlignCenter, AlignTop, sc + 1);
        } else {
            elements_multiline_text_aligned(canvas, 64, 18, AlignCenter, AlignTop, tag);
        }

        char line[80];
        if(m->has_fix) {
            snprintf(line, sizeof(line), "%.6f, %.6f", (double)m->lat, (double)m->lon);
        } else {
            snprintf(line, sizeof(line), "NO FIX (will save 0,0)");
        }
        elements_multiline_text_aligned(canvas, 64, 30, AlignCenter, AlignTop, line);

        snprintf(line, sizeof(line), "HDOP=%.1f sats=%u", (double)m->hdop, m->sats);
        elements_multiline_text_aligned(canvas, 64, 40, AlignCenter, AlignTop, line);

        if(m->note[0]) {
            snprintf(line, sizeof(line), "note: %s", m->note);
            elements_multiline_text_aligned(canvas, 64, 50, AlignCenter, AlignTop, line);
        }

        elements_multiline_text_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "OK=save  Back=cancel");
        return;
    }

    // Badge batterie en haut à droite
    char bat[8];
    snprintf(bat, sizeof(bat), "%u%%", (unsigned)furi_hal_power_get_pct());
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 2, AlignRight, AlignTop, bat);

    // Header : label du preset (avec marqueur de variante si plusieurs)
    canvas_set_font(canvas, FontPrimary);
    if(m->variant_count > 1) {
        char header[48];
        snprintf(
            header,
            sizeof(header),
            "< %s %u/%u >",
            p->label,
            (unsigned)(m->variant_idx + 1),
            (unsigned)m->variant_count);
        elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, header);
    } else {
        elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, p->label);
    }

    // Tag OSM effectif — split sur 2 lignes si multi-tag (contient ';'),
    // pour éviter le wrap automatique qui superpose avec les lignes en dessous.
    char tag[64];
    preset_build_tag(p, m->variant_idx, tag, sizeof(tag));
    canvas_set_font(canvas, FontSecondary);
    char* semicolon = strchr(tag, ';');
    if(semicolon) {
        *semicolon = '\0';
        elements_multiline_text_aligned(canvas, 64, 14, AlignCenter, AlignTop, tag);
        elements_multiline_text_aligned(canvas, 64, 22, AlignCenter, AlignTop, semicolon + 1);
    } else {
        elements_multiline_text_aligned(canvas, 64, 16, AlignCenter, AlignTop, tag);
    }

    // Coords + sats
    char line1[48];
    char line2[48];
    if(m->has_fix) {
        snprintf(line1, sizeof(line1), "%.5f, %.5f", (double)m->lat, (double)m->lon);
        snprintf(
            line2,
            sizeof(line2),
            "HDOP=%.1f sats=%u  #%u",
            (double)m->hdop,
            m->sats,
            m->session_count);
    } else {
        snprintf(line1, sizeof(line1), "No GPS fix");
        snprintf(line2, sizeof(line2), "sats=%u  #%u", m->sats, m->session_count);
    }
    // Décale coords/hdop d'une ligne vers le bas si on a 2 lignes de tag
    uint8_t y_coords = semicolon ? 30 : 28;
    uint8_t y_hdop = semicolon ? 40 : 38;
    uint8_t y_alt = semicolon ? 50 : 48;

    elements_multiline_text_aligned(canvas, 64, y_coords, AlignCenter, AlignTop, line1);
    elements_multiline_text_aligned(canvas, 64, y_hdop, AlignCenter, AlignTop, line2);

    // Ligne altitude + précision estimée + total
    // prec_m ≈ HDOP * 3 : approximation courante pour les modules ublox
    // (User Equivalent Range Error ~3 m en conditions normales, multiplié par HDOP).
    char line3[56];
    if(m->fix_ever && m->has_fix) {
        float prec_m = m->hdop * 3.0f;
        snprintf(
            line3, sizeof(line3),
            "alt=%.0fm prec=%.0fm t=%lu",
            (double)m->altitude, (double)prec_m, (unsigned long)m->total_count);
    } else if(m->fix_ever) {
        // Fix stale (hystérésis 5s) : précision probablement fausse, affiche fix age
        snprintf(
            line3, sizeof(line3),
            "alt=%.0fm fix=%lus  t=%lu",
            (double)m->altitude, (unsigned long)m->fix_age_s,
            (unsigned long)m->total_count);
    } else {
        snprintf(line3, sizeof(line3), "alt=-- prec=-- t=%lu", (unsigned long)m->total_count);
    }
    elements_multiline_text_aligned(canvas, 64, y_alt, AlignCenter, AlignTop, line3);

    // Footer : toast SAVED > note > rappel touches
    if(m->recent_save && m->last_saved_preset[0]) {
        char toast[80];
        snprintf(
            toast, sizeof(toast), "> saved %s @%s",
            m->last_saved_preset, m->last_saved_time);
        elements_multiline_text_aligned(canvas, 64, 62, AlignCenter, AlignBottom, toast);
    } else if(m->note[0]) {
        char footer[80];
        snprintf(footer, sizeof(footer), "note: %s", m->note);
        elements_multiline_text_aligned(canvas, 64, 62, AlignCenter, AlignBottom, footer);
    } else {
        elements_multiline_text_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "OK save  Up:note  <>:var");
    }
}

// Fonction de save bas niveau : écrit un point avec les coords et HDOP fournis.
// Utilisée à la fois pour les saves instantanés et pour le final d'un averaging.
// Valeurs "placeholder" : on ne peut pas les utiliser ni comme icône OsmAnd,
// ni comme vraie valeur de tag OSM. Doivent être remplacées par la note utilisateur.
static bool is_placeholder_value(const char* v) {
    if(!v || !v[0]) return true;
    return strcmp(v, "TBD") == 0 || strcmp(v, "?") == 0 ||
           strcmp(v, "tbd") == 0 || strcmp(v, "fill_me") == 0;
}

// Retourne true si la note est "safe" comme valeur de tag OSM : pas de '=' ni ';'
// qui casseraient notre format interne, non vide après trim.
static bool note_is_safe_as_value(const char* note) {
    if(!note || !note[0]) return false;
    for(const char* p = note; *p; p++) {
        if(*p == '=' || *p == ';') return false;
    }
    return true;
}

static bool quick_log_write_point(
    App* app, float lat, float lon, float hdop, uint8_t sats, float altitude,
    const Preset* p, bool forced_note_avg) {
    if(!p) return false;

    // Refuser les saves à lat=0/lon=0 (GPS non initialisé, trame RMC/GGA sans coords).
    // Ces points polluent les fichiers de sortie et n'ont aucune valeur OSM.
    if(lat == 0.0f && lon == 0.0f) {
        snprintf(app->last_error, sizeof(app->last_error), "GPS not initialized (0,0)");
        FURI_LOG_E("OSM", "write_point: refused lat=lon=0");
        notification_message(app->notification, &sequence_error);
        return false;
    }

    FURI_LOG_D("OSM", "write_point: pre_save_check");
    if(!storage_pre_save_check(app->last_error, sizeof(app->last_error))) {
        FURI_LOG_E("OSM", "write_point: pre_save_check FAILED: %s", app->last_error);
        notification_message(app->notification, &sequence_error);
        return false;
    }
    FURI_LOG_D("OSM", "write_point: storage_write_all_formats");

    char tag[128];
    preset_build_tag(p, app->current_variant, tag, sizeof(tag));

    // Note-as-value : si la valeur primaire du preset est un placeholder (ex. "TBD"
    // pour House number) et que l'user a saisi une note qui tient comme valeur OSM,
    // on remplace dans le tag. Ex. preset "addr:housenumber=TBD" + note "42" →
    // tag final "addr:housenumber=42". La note est alors vidée pour éviter la duplication.
    bool note_consumed_as_value = false;
    if(is_placeholder_value(p->variants[0]) && note_is_safe_as_value(app->quick_note)) {
        char* placeholder = strstr(tag, "=");
        if(placeholder) {
            // Préserve ce qui suit le placeholder (ex. ";source=survey")
            char* sep = strchr(placeholder + 1, ';');
            char after[96] = "";
            if(sep) {
                strncpy(after, sep, sizeof(after) - 1);
                after[sizeof(after) - 1] = '\0';
            }
            snprintf(placeholder + 1, sizeof(tag) - (size_t)(placeholder + 1 - tag),
                     "%s%s", app->quick_note, after);
            note_consumed_as_value = true;
        }
    }

    // Survey mode : append les tags OSM standards "source=survey" + "survey:date=YYYY-MM-DD"
    // qui signalent à la community que la contribution a été vérifiée sur place.
    // Séparés par ';' → storage.c les éclate en <tag/> distincts dans l'OSM XML.
    if(app->settings.survey_mode) {
        DateTime dt_s;
        furi_hal_rtc_get_datetime(&dt_s);
        size_t tlen = strlen(tag);
        snprintf(
            tag + tlen,
            sizeof(tag) - tlen,
            "%ssource=survey;survey:date=%04u-%02u-%02u",
            tlen > 0 ? ";" : "",
            dt_s.year, dt_s.month, dt_s.day);
    }

    // Note finale (base + auto_photo_id + suffixe avg éventuel).
    // Si la note a été consommée comme valeur de tag ci-dessus, on la retire
    // du champ note pour éviter la redondance (elle est déjà dans le tag).
    char final_note[160];
    final_note[0] = '\0';
    const char* base_note = (!note_consumed_as_value && app->quick_note[0]) ? app->quick_note : NULL;

    char photo_suffix[24] = "";
    if(app->settings.auto_photo_id) {
        snprintf(
            photo_suffix, sizeof(photo_suffix), "photo:%lu",
            (unsigned long)(app->total_count + 1));
    }

    // Assemble les morceaux avec espaces
    int w = 0;
    if(base_note) {
        w += snprintf(final_note + w, sizeof(final_note) - w, "%s", base_note);
    }
    if(photo_suffix[0]) {
        w += snprintf(
            final_note + w, sizeof(final_note) - w, "%s%s", w > 0 ? " " : "", photo_suffix);
    }
    if(forced_note_avg) {
        w += snprintf(
            final_note + w, sizeof(final_note) - w, "%s%s",
            w > 0 ? " " : "", "avg");
    }
    const char* note = final_note[0] ? final_note : NULL;

    // Infos supplémentaires pour enrichir le GPX (format OsmAnd Favorites)
    const char* cat_label =
        (p->category < PresetCatCount) ? PRESET_CATEGORY_LABELS[p->category] : "Other";
    const char* cat_color =
        (p->category < PresetCatCount) ? PRESET_CATEGORY_COLORS[p->category] : "#c0c0c0";
    // L'icône OsmAnd est dérivée de la valeur primaire OSM (ex. "bench" → icône banc).
    // Si la valeur est un placeholder (TBD, ?, ...), fallback sur l'icône générique :
    // sinon OsmAnd affiche un icon "TBD" cassé.
    const char* icon_hint = (p->variants[0] && !is_placeholder_value(p->variants[0]))
                                ? p->variants[0]
                                : "special_marker";

    // ID OSM séquentiel : total_count est le nb de points existants,
    // le nouveau sera donc le (total_count + 1)-ième. Utilisé comme ID négatif
    // dans le .osm (convention OSM API pour nouveaux nodes non-uploadés).
    uint32_t node_seq = app->total_count + 1;

    storage_write_all_formats(
        lat, lon, altitude, hdop, sats, tag, note,
        p->label, cat_label, icon_hint, cat_color,
        node_seq);
    FURI_LOG_D("OSM", "write_point: formats written, saving note cache");

    // Cache de note (on persiste juste la note utilisateur, pas photo/avg)
    char primary[48];
    preset_build_primary_tag(p, primary, sizeof(primary));
    notes_cache_save(primary, app->quick_note);
    FURI_LOG_D("OSM", "write_point: done");

    // Tracker du dernier point
    app->last_saved_tick = furi_get_tick();
    strncpy(app->last_saved_preset, p->label, sizeof(app->last_saved_preset) - 1);
    app->last_saved_preset[sizeof(app->last_saved_preset) - 1] = '\0';
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(app->last_saved_time, sizeof(app->last_saved_time), "%02u:%02u", dt.hour, dt.minute);

    app->session_count++;
    app->total_count++;
    notification_message(app->notification, &sequence_success);
    return true;
}

static bool quick_log_do_save(App* app, bool force) {
    FURI_LOG_D("OSM", "save: start preset=%u force=%d", app->current_preset, force);
    const Preset* p = presets_get(app->current_preset);
    if(!p) p = presets_get(0);
    if(!p) {
        FURI_LOG_E("OSM", "save: no preset available");
        return false;
    }
    FURI_LOG_D("OSM", "save: preset=%s", p->label);

    // "Fix effectif" = on considère qu'on a un fix si un fix a été reçu dans
    // les 5 dernières secondes (même logique que l'affichage). Évite de
    // refuser un save juste parce que la trame RMC en cours dit V alors que
    // les GGA d'avant/après sont valides.
    bool recent_fix = false;
    if(app->last_fix_tick != 0) {
        uint32_t freq = furi_kernel_get_tick_frequency();
        if(freq == 0) freq = 1;
        uint32_t age_ticks = furi_get_tick() - app->last_fix_tick;
        recent_fix = (age_ticks < 5 * freq);
    }
    bool effective_fix = app->has_fix || recent_fix;
    float hdop_max = quick_log_hdop_max(app);
    bool quality_ok = effective_fix && (app->hdop <= hdop_max);
    FURI_LOG_D(
        "OSM", "save: fix=%d recent=%d hdop=%.2f max=%.2f ok=%d",
        app->has_fix, recent_fix, (double)app->hdop, (double)hdop_max, quality_ok);
    if(!quality_ok && !force) {
        if(!effective_fix) {
            snprintf(app->last_error, sizeof(app->last_error),
                     "No GPS fix\nHold OK to force");
        } else {
            snprintf(app->last_error, sizeof(app->last_error),
                     "HDOP %.1f > %.1f\nHold OK to force",
                     (double)app->hdop, (double)hdop_max);
        }
        FURI_LOG_I("OSM", "save: refused quality (%s)", app->last_error);
        notification_message(app->notification, &sequence_error);
        return false;
    }

    // Détection de doublon (si setting activé et pas déjà confirmé par user)
    if(!force && app->settings.duplicate_check_m > 0) {
        char tag_for_dup[64];
        preset_build_tag(p, app->current_variant, tag_for_dup, sizeof(tag_for_dup));
        FURI_LOG_D("OSM", "save: dup check starting (%um)", app->settings.duplicate_check_m);
        float dist = 0;
        bool found = storage_find_duplicate_nearby(
            app->lat, app->lon, tag_for_dup,
            app->settings.duplicate_check_m, &dist);
        FURI_LOG_D("OSM", "save: dup check done (found=%d dist=%.1f)", found, (double)dist);
        if(found) {
            app->duplicate_warning = true;
            app->duplicate_dist_m = dist;
            notification_message(app->notification, &sequence_error);
            return false;
        }
    }

    FURI_LOG_D("OSM", "save: calling write_point");
    bool ok = quick_log_write_point(
        app, app->lat, app->lon, app->hdop, app->sats, app->altitude, p, false);
    FURI_LOG_I("OSM", "save: result=%d", ok);
    return ok;
}

// --- Averaging : démarrage / tick / finalisation ---
static void averaging_start(App* app) {
    uint8_t secs = app->settings.avg_seconds;
    if(secs == 0) return;
    app->averaging = true;
    app->averaging_start_tick = furi_get_tick();
    uint32_t freq = furi_kernel_get_tick_frequency();
    if(freq == 0) freq = 1;
    app->averaging_end_tick = app->averaging_start_tick + (uint32_t)secs * freq;
    app->avg_lat_sum = 0.0;
    app->avg_lon_sum = 0.0;
    app->avg_sample_count = 0;
    app->avg_min_hdop = 99.9f;
    app->avg_last_sampled_fix_tick = 0;
}

static void averaging_cancel(App* app) {
    app->averaging = false;
    app->avg_sample_count = 0;
}

// Collecte un sample si un nouveau fix est arrivé depuis le dernier tick.
// On se base uniquement sur le changement de last_fix_tick (qui n'avance QUE
// quand has_fix passe à true) — pas besoin de vérifier has_fix maintenant,
// c'est déjà implicite. Ça évite de manquer des samples à cause du flip-flop
// RMC(V)/GGA(q>0) qui fait osciller has_fix à 2 Hz.
static void averaging_tick(App* app) {
    if(!app->averaging) return;
    if(app->last_fix_tick != 0 && app->last_fix_tick != app->avg_last_sampled_fix_tick) {
        app->avg_lat_sum += (double)app->lat;
        app->avg_lon_sum += (double)app->lon;
        if(app->hdop < app->avg_min_hdop) app->avg_min_hdop = app->hdop;
        app->avg_sample_count++;
        app->avg_last_sampled_fix_tick = app->last_fix_tick;
    }

    if(furi_get_tick() >= app->averaging_end_tick) {
        if(app->avg_sample_count > 0) {
            float avg_lat = (float)(app->avg_lat_sum / (double)app->avg_sample_count);
            float avg_lon = (float)(app->avg_lon_sum / (double)app->avg_sample_count);
            const Preset* p = presets_get(app->current_preset);
            if(!p) p = presets_get(0);
            if(p) {
                quick_log_write_point(
                    app, avg_lat, avg_lon, app->avg_min_hdop, app->sats,
                    app->altitude, p, true);
            }
        } else {
            snprintf(app->last_error, sizeof(app->last_error),
                     "Averaging: no fix\nduring capture");
            notification_message(app->notification, &sequence_error);
        }
        app->averaging = false;
    }
}

static bool quick_log_input_callback(InputEvent* event, void* ctx) {
    App* app = (App*)ctx;

    bool ok_short = (event->type == InputTypeShort && event->key == InputKeyOk);
    bool ok_long = (event->type == InputTypeLong && event->key == InputKeyOk);
    bool is_press = (event->type == InputTypeShort || event->type == InputTypeRepeat);

    // Bloque toutes les touches pendant un save différé : évite que l'utilisateur
    // re-déclenche save_deferred avant que le tick n'ait consommé le précédent.
    if(app->save_deferred) return true;

    // Overlay erreur : n'importe quelle touche efface, on reste sur Quick Log
    if(app->last_error[0]) {
        if(event->type == InputTypeShort) {
            app->last_error[0] = '\0';
            quick_log_refresh(app);
        }
        return true;
    }

    // Averaging en cours : Back annule, les autres touches sont bloquées
    if(app->averaging) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            averaging_cancel(app);
            quick_log_refresh(app);
        }
        return true;
    }

    // Overlay doublon : OK = save anyway (force, différé), Back = annule
    if(app->duplicate_warning) {
        if(ok_short || ok_long) {
            app->duplicate_warning = false;
            app->preview_pending = false;
            // Différer : la frame suivante affiche "Saving..." avant le blocage I/O
            app->save_deferred = true;
            app->save_deferred_force = true;
            quick_log_refresh(app);
            return true;
        }
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->duplicate_warning = false;
            app->preview_pending = false;
            quick_log_refresh(app);
            return true;
        }
        return true;
    }

    // Mode preview (confirmation) : gestion spéciale des touches
    if(app->preview_pending) {
        if(ok_short) {
            app->preview_pending = false;
            app->save_deferred = true;
            app->save_deferred_force = false;
            quick_log_refresh(app);
            return true;
        }
        if(ok_long) {
            app->preview_pending = false;
            app->save_deferred = true;
            app->save_deferred_force = true;
            quick_log_refresh(app);
            return true;
        }
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->preview_pending = false;
            quick_log_refresh(app);
            return true;
        }
        return true;
    }

    // ←/→ : cycle sur les variantes du preset courant
    if(is_press && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        const Preset* p = presets_get(app->current_preset);
        if(p && p->variant_count > 1) {
            if(event->key == InputKeyRight) {
                app->current_variant = (uint8_t)((app->current_variant + 1) % p->variant_count);
            } else {
                app->current_variant = app->current_variant == 0
                                           ? (uint8_t)(p->variant_count - 1)
                                           : (uint8_t)(app->current_variant - 1);
            }
            quick_log_refresh(app);
        }
        return true;
    }

    // UP : ouvre l'éditeur de note
    if(event->type == InputTypeShort && event->key == InputKeyUp) {
        view_dispatcher_switch_to_view(app->dispatcher, AppViewNote);
        return true;
    }

    // DOWN : efface la note courante
    if(event->type == InputTypeShort && event->key == InputKeyDown) {
        app->quick_note[0] = '\0';
        quick_log_refresh(app);
        return true;
    }

    if(ok_short || ok_long) {
        // OK long : force save (différé), quelle que soit la config
        if(ok_long) {
            app->save_deferred = true;
            app->save_deferred_force = true;
            quick_log_refresh(app);
            return true;
        }
        // OK court + averaging activé -> démarre la collecte
        if(app->settings.avg_seconds > 0 && !app->averaging) {
            averaging_start(app);
            quick_log_refresh(app);
            return true;
        }
        // OK court + preview activé -> écran de confirmation
        if(app->settings.preview_before_save) {
            app->preview_pending = true;
            quick_log_refresh(app);
            return true;
        }
        // Sinon : save (différé), respecte gate HDOP
        app->save_deferred = true;
        app->save_deferred_force = false;
        quick_log_refresh(app);
        return true;
    }

    return false;
}

static uint32_t quick_log_previous_callback(void* ctx) {
    (void)ctx;
    return AppViewPresets;
}

static void quick_log_enter(void* ctx) {
    App* app = (App*)ctx;

    // Recharge la note du cache associée au preset courant (si existe)
    const Preset* p = presets_get(app->current_preset);
    if(p) {
        char primary[48];
        preset_build_primary_tag(p, primary, sizeof(primary));
        notes_cache_load(primary, app->quick_note, sizeof(app->quick_note));
    }

    quick_log_refresh(app);
}
static void quick_log_exit(void* ctx) {
    (void)ctx;
}

void quick_log_tick_deferred(App* app) {
    if(!app->save_deferred) return;
    bool force = app->save_deferred_force;
    app->save_deferred = false;
    app->save_deferred_force = false;
    quick_log_do_save(app, force);
}

void quick_log_refresh(App* app) {
    if(!app->quick_view) return;

    // Tick de collecte d'averaging (si actif). Peut terminer le save automatiquement.
    averaging_tick(app);

    uint32_t age_s = 0;
    bool fix_ever = (app->last_fix_tick != 0);
    if(fix_ever) {
        uint32_t freq = furi_kernel_get_tick_frequency();
        if(freq == 0) freq = 1;
        age_s = (furi_get_tick() - app->last_fix_tick) / freq;
    }
    // Hystérésis d'affichage : on reste en "fix OK" tant qu'on a eu un fix
    // dans les 5 dernières secondes, même si la trame courante dit "no fix".
    // Évite le clignotement RMC(V) <-> GGA(q>0) à 2 Hz.
    bool display_has_fix = app->has_fix || (fix_ever && age_s < 5);

    // Toast "SAVED": actif pendant 10s après le dernier save
    bool recent_save = false;
    if(app->last_saved_tick != 0) {
        uint32_t freq = furi_kernel_get_tick_frequency();
        if(freq == 0) freq = 1;
        uint32_t save_age = (furi_get_tick() - app->last_saved_tick) / freq;
        recent_save = (save_age < 10);
    }

    with_view_model(
        app->quick_view,
        QuickLogModel * m,
        {
            m->has_fix = display_has_fix;
            m->lat = app->lat;
            m->lon = app->lon;
            m->hdop = app->hdop;
            m->altitude = app->altitude;
            m->sats = app->sats;
            m->preset_idx = app->current_preset;
            m->variant_idx = app->current_variant;
            const Preset* p = presets_get(app->current_preset);
            m->variant_count = p ? p->variant_count : 1;
            m->session_count = app->session_count;
            m->total_count = app->total_count;
            m->fix_age_s = age_s;
            m->fix_ever = fix_ever;
            m->preview_pending = app->preview_pending;
            m->duplicate_warning = app->duplicate_warning;
            m->duplicate_dist_m = app->duplicate_dist_m;
            strncpy(m->last_error, app->last_error, sizeof(m->last_error) - 1);
            m->last_error[sizeof(m->last_error) - 1] = '\0';
            strncpy(m->note, app->quick_note, sizeof(m->note) - 1);
            m->note[sizeof(m->note) - 1] = '\0';
            m->recent_save = recent_save;
            strncpy(m->last_saved_preset, app->last_saved_preset,
                    sizeof(m->last_saved_preset) - 1);
            m->last_saved_preset[sizeof(m->last_saved_preset) - 1] = '\0';
            strncpy(m->last_saved_time, app->last_saved_time,
                    sizeof(m->last_saved_time) - 1);
            m->last_saved_time[sizeof(m->last_saved_time) - 1] = '\0';

            m->saving = app->save_deferred;
            // Averaging — snapshot de l'état courant pour l'overlay
            m->averaging = app->averaging;
            m->avg_total_s = app->settings.avg_seconds;
            m->avg_samples = app->avg_sample_count;
            m->avg_min_hdop = app->avg_min_hdop;
            if(app->averaging) {
                uint32_t freq = furi_kernel_get_tick_frequency();
                if(freq == 0) freq = 1;
                m->avg_elapsed_s = (furi_get_tick() - app->averaging_start_tick) / freq;
                if(app->avg_sample_count > 0) {
                    m->avg_cur_lat =
                        (float)(app->avg_lat_sum / (double)app->avg_sample_count);
                    m->avg_cur_lon =
                        (float)(app->avg_lon_sum / (double)app->avg_sample_count);
                }
            }
        },
        true);
}

void app_start_quick_log(App* app) {
    if(!app->quick_view) {
        View* view = view_alloc();
        view_allocate_model(view, ViewModelTypeLockFree, sizeof(QuickLogModel));
        view_set_context(view, app);
        view_set_draw_callback(view, quick_log_draw_callback);
        view_set_input_callback(view, quick_log_input_callback);
        view_set_enter_callback(view, quick_log_enter);
        view_set_exit_callback(view, quick_log_exit);
        view_set_previous_callback(view, quick_log_previous_callback);

        app->quick_view = view;
        view_dispatcher_add_view(app->dispatcher, AppViewQuickLog, view);
    }
    view_dispatcher_switch_to_view(app->dispatcher, AppViewQuickLog);
}

void app_stop_quick_log(App* app) {
    if(app->quick_view) {
        view_dispatcher_remove_view(app->dispatcher, AppViewQuickLog);
        view_free(app->quick_view);
        app->quick_view = NULL;
    }
}
