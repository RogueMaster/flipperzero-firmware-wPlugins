#include "../animation_switcher.h"
#include "fas_scene.h"

/* ── Read the playlist .txt and build a display string ─────────────────── */
static void build_preview_text(FasApp* app, char* out, int out_size) {
    char path[FAS_PATH_LEN];
    snprintf(
        path,
        sizeof(path),
        "%s/%s.txt",
        FAS_PLAYLISTS_PATH,
        app->playlists[app->current_playlist_index].name);

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        snprintf(out, out_size, "Could not read\nplaylist file.");
        return;
    }

    /* Reserve tail space for a "...and N more" line so we never truncate
     * mid-name. Each name takes up to ~70 bytes when rendered. */
    const int tail_reserve = 32;
    int pos = 0;
    int total = 0; /* total Name: entries seen */
    int rendered = 0; /* entries written to out */
    char line[128];
    int lp = 0;
    char c;

    while(storage_file_read(f, &c, 1) == 1) {
        if(c == '\n' || c == '\r') {
            line[lp] = '\0';
            if(strncmp(line, "Name: ", 6) == 0) {
                total++;
                if(pos < out_size - tail_reserve - 80) {
                    int written = snprintf(out + pos, out_size - pos, "- %s\n", line + 6);
                    if(written > 0) {
                        pos += written;
                        rendered++;
                    }
                }
            }
            lp = 0;
        } else if(lp < (int)sizeof(line) - 1) {
            line[lp++] = c;
        }
    }
    /* Flush last line (no trailing newline) */
    if(lp > 0) {
        line[lp] = '\0';
        if(strncmp(line, "Name: ", 6) == 0) {
            total++;
            if(pos < out_size - tail_reserve - 80) {
                int written = snprintf(out + pos, out_size - pos, "- %s\n", line + 6);
                if(written > 0) {
                    pos += written;
                    rendered++;
                }
            }
        }
    }

    if(rendered < total) {
        snprintf(out + pos, out_size - pos, "...and %d more\n", total - rendered);
    }

    if(total == 0) snprintf(out, out_size, "(empty playlist)");

    storage_file_close(f);
    storage_file_free(f);
}

/* ── Scene handlers ───────────────────────────────────────────────────── */
void fas_scene_playlist_preview_on_enter(void* context) {
    FasApp* app = context;
    widget_reset(app->widget);

    /* Title */
    char title[FAS_PLAYLIST_NAME_LEN + 16];
    snprintf(
        title, sizeof(title), "Playlist: %s", app->playlists[app->current_playlist_index].name);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, title);

    /* Animation list as scrollable text.  Sized to hold all 128 possible
     * animations (rough upper bound: 128 * ~70 chars per "- name\n"). */
    static char preview_buf[4096];
    preview_buf[0] = '\0';
    build_preview_text(app, preview_buf, sizeof(preview_buf));
    widget_add_text_scroll_element(app->widget, 0, 14, 128, 50, preview_buf);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewWidget);
}

bool fas_scene_playlist_preview_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fas_scene_playlist_preview_on_exit(void* context) {
    FasApp* app = context;
    widget_reset(app->widget);
}
