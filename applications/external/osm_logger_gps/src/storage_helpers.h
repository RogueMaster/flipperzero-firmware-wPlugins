#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Écrit un point dans les 5 fichiers (JSONL, CSV, GPX, GeoJSON, OSM XML).
// Le GPX est enrichi avec des extensions OsmAnd (icon/color/background) +
// type (catégorie) + nom humain lisible. Les autres formats n'utilisent que
// les champs standard.
//
//   display_name   : label humain (ex. "Bench") pour <name> GPX
//   category_label : catégorie (ex. "Street furniture") pour <type> GPX
//   osmand_icon    : valeur primaire OSM (ex. "bench") — OsmAnd mappe l'icône
//   osmand_color   : couleur hex "#RRGGBB" pour la pastille OsmAnd (peut être NULL)
//   node_seq       : compteur séquentiel (1-based) utilisé comme ID OSM négatif
//                    dans le fichier .osm (convention OSM API pour nouveaux nodes)
void storage_write_all_formats(
    float lat,
    float lon,
    float altitude,
    float hdop,
    uint8_t sats,
    const char* tag,
    const char* note,
    const char* display_name,
    const char* category_label,
    const char* osmand_icon,
    const char* osmand_color,
    uint32_t node_seq);

// Compte le nombre de points déjà présents dans points.jsonl (0 si pas de fichier).
uint32_t storage_count_saved_points(void);

// Ajoute un trkpt GPX à track.gpx. Si new_segment=true, insère </trkseg><trkseg>
// avant le point pour délimiter une nouvelle session de tracking.
void storage_append_trkpt(float lat, float lon, float altitude, bool new_segment);

// Supprime le dernier point sauvegardé de TOUS les fichiers (JSONL, CSV, GPX, GeoJSON).
// Retourne true si au moins un fichier a été modifié.
bool storage_delete_last_point(void);

// Supprime tous les fichiers de points (points.jsonl, notes.csv, points.gpx,
// points.geojson). track.gpx n'est pas touché (mode séparé).
void storage_clear_all_points(void);

// Archive la session courante : crée /ext/apps_data/osm_logger/session_<ts>/
// et y déplace les 5 fichiers de sortie. Retourne true si au moins un fichier
// a été déplacé. Fills err si une erreur bloquante survient.
bool storage_archive_session(char* err, size_t err_size);

// Lit les N dernières lignes de points.jsonl, extrait les champs "time" et "tag"
// pour chacune, et les écrit sous la forme "HH:MM  tag" dans out_lines (buffer
// d'au moins N * line_size octets). Retourne le nombre de lignes lues (0..N).
uint8_t storage_read_last_points(char* out_lines, uint8_t max_lines, size_t line_size);

// Lit le N-ième point le plus récent (0 = dernier) en brut (ligne JSONL complète).
// Retourne true si trouvé, false sinon.
bool storage_get_point_raw(uint8_t idx_from_end, char* out, size_t out_size);

// Vérifie que la SD est prête pour une écriture (carte présente, dossier OK,
// espace suffisant). Retourne true si OK. Sinon remplit `err` avec un message
// court et retourne false.
bool storage_pre_save_check(char* err, size_t err_size);

// Cherche dans points.jsonl un point avec le même tag à moins de max_dist_m
// mètres. Retourne true si trouvé et remplit out_dist_m (distance au plus proche).
bool storage_find_duplicate_nearby(
    float lat,
    float lon,
    const char* tag,
    uint8_t max_dist_m,
    float* out_dist_m);
