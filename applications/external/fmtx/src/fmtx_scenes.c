#include "fmtx_scenes.h"
#include "fmtx_anim.h"

#include <furi_hal.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>
#include <storage/storage.h>

enum {
    AboutPageLogo,
    AboutPageText,
};

enum {
    AboutSocialWebsite,
    AboutSocialInstagram,
    AboutSocialTiktok,
    AboutSocialGithub,
    AboutSocialYoutube,
};

typedef struct {
    uint8_t top;
    uint8_t bottom;
} AboutSocial;

typedef struct {
    uint8_t sequence_index;
} AboutModel;

enum {
    AboutTextWebsite,
    AboutTextAtYo3gnd = AboutTextWebsite + sizeof("www.yo3gnd.ro"),
    AboutTextYo3gnd = AboutTextAtYo3gnd + sizeof("@yo3gnd"),
    AboutTextWebsiteLabel = AboutTextYo3gnd + sizeof("yo3gnd"),
    AboutTextInstagram = AboutTextWebsiteLabel + sizeof("website"),
    AboutTextTiktok = AboutTextInstagram + sizeof("instagram"),
    AboutTextGithub = AboutTextTiktok + sizeof("tiktok"),
    AboutTextYoutube = AboutTextGithub + sizeof("github.com"),
};

static const uint8_t about_logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0xf8, 0x1f, 0x00,
    0x00, 0xfe, 0x7f, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00,
    0x00, 0xf8, 0x1f, 0x00, 0x00, 0x38, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xc0, 0x03, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x46, 0xc4, 0x23, 0x62, 0x46, 0x8c, 0x31, 0x62,
    0xc6, 0x0c, 0x30, 0x63, 0xc6, 0x18, 0x18, 0x63, 0xc6, 0x31, 0x8c, 0x63, 0x8c, 0x01, 0x80, 0x31,
    0x0c, 0x03, 0xc0, 0x30, 0x1e, 0x06, 0x60, 0x78, 0x3f, 0x04, 0x20, 0xfc, 0x31, 0x00, 0x00, 0x8c,
    0xe0, 0x00, 0x00, 0x07, 0xc0, 0x01, 0x80, 0x03, 0x80, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t about_callsign[] = {
    0x03, 0xc3, 0x0f, 0xfc, 0xc0, 0x0f, 0x03, 0xf3, 0x03, 0x03, 0xc3, 0x0f, 0xfc, 0xc0, 0x0f, 0x03,
    0xf3, 0x03, 0x03, 0x33, 0x30, 0x03, 0x33, 0x30, 0x03, 0x33, 0x0c, 0x03, 0x33, 0x30, 0x03, 0x33,
    0x30, 0x03, 0x33, 0x0c, 0xcc, 0x30, 0x30, 0x00, 0x33, 0x00, 0x0f, 0x33, 0x30, 0xcc, 0x30, 0x30,
    0x00, 0x33, 0x00, 0x0f, 0x33, 0x30, 0xcc, 0x30, 0x30, 0xf0, 0x30, 0x00, 0x33, 0x33, 0x30, 0xcc,
    0x30, 0x30, 0xf0, 0x30, 0x00, 0x33, 0x33, 0x30, 0x30, 0x30, 0x30, 0x00, 0x33, 0x3f, 0xc3, 0x33,
    0x30, 0x30, 0x30, 0x30, 0x00, 0x33, 0x3f, 0xc3, 0x33, 0x30, 0x30, 0x30, 0x30, 0x00, 0x33, 0x30,
    0x03, 0x33, 0x30, 0x30, 0x30, 0x30, 0x00, 0x33, 0x30, 0x03, 0x33, 0x30, 0x30, 0x30, 0x30, 0x03,
    0x33, 0x30, 0x03, 0x33, 0x0c, 0x30, 0x30, 0x30, 0x03, 0x33, 0x30, 0x03, 0x33, 0x0c, 0x30, 0xc0,
    0x0f, 0xfc, 0xc0, 0x0f, 0x03, 0xf3, 0x03, 0x30, 0xc0, 0x0f, 0xfc, 0xc0, 0x0f, 0x03, 0xf3, 0x03,
};

static const char about_social_text[] = "www.yo3gnd.ro\0"
                                        "@yo3gnd\0"
                                        "yo3gnd\0"
                                        "website\0"
                                        "instagram\0"
                                        "tiktok\0"
                                        "github.com\0"
                                        "youtube";

static const AboutSocial about_socials[] = {
    [AboutSocialWebsite] = {AboutTextWebsite, AboutTextWebsiteLabel},
    [AboutSocialInstagram] = {AboutTextAtYo3gnd, AboutTextInstagram},
    [AboutSocialTiktok] = {AboutTextAtYo3gnd, AboutTextTiktok},
    [AboutSocialGithub] = {AboutTextYo3gnd, AboutTextGithub},
    [AboutSocialYoutube] = {AboutTextAtYo3gnd, AboutTextYoutube},
};

static const uint8_t about_social_sequence[] = {
    AboutSocialWebsite,
    AboutSocialInstagram,
    AboutSocialTiktok,
    AboutSocialWebsite,
    AboutSocialInstagram,
    AboutSocialTiktok,
    AboutSocialGithub,
    AboutSocialYoutube,
};

const char abttext[] =
    "Built by Richard, YO3GND, a ham radio operator who enjoys embedded engineering and DSP.\n\n"
    "This began as a Morse Flipper spike: send audio to a Baofeng without an audio lead. It worked, so it became its own thing.\n\n"
    "A first-order sigma-delta modulator turns PCM into one-bit PDM. Each bit selects a CC1101 FSK deviation, approximating narrowband FM audio.\n\n"
    "The CC1101 is not an audio transmitter. Its carrier can wander during long transmissions; receiver AFC and bandwidth tolerate some drift. FM capture does not fix it.";

static void about_draw(Canvas* canvas, void* model) {
    AboutModel* about = model;
    const AboutSocial* social = &about_socials[about_social_sequence[about->sequence_index]];

    canvas_clear(canvas);
    canvas_draw_xbm(canvas, 4, 16, 32, 32, about_logo);
    canvas_draw_xbm(canvas, 45, 8, 70, 16, about_callsign);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 82, 37, AlignCenter, AlignBottom, about_social_text + social->top);
    canvas_draw_str_aligned(
        canvas, 82, 50, AlignCenter, AlignBottom, about_social_text + social->bottom);
    elements_button_right(canvas, "Next");
}

static bool about_input(InputEvent* event, void* ctx) {
    App* app = ctx;
    if(!event || event->type != InputTypeShort || event->key != InputKeyRight) return false;
    view_dispatcher_send_custom_event(app->dispatcher, FmtxAboutNext);

    return true;
}

static void about_begin(App* app) {
    DateTime now;
    AboutModel* model = view_get_model(app->playback_view);

    furi_hal_rtc_get_datetime(&now);
    model->sequence_index = now.day % COUNT_OF(about_social_sequence);
    view_commit_model(app->playback_view, false);
}

static void about_rotate(App* app) {
    AboutModel* model = view_get_model(app->playback_view);

    model->sequence_index++;
    if(model->sequence_index >= COUNT_OF(about_social_sequence)) model->sequence_index = 0;
    view_commit_model(app->playback_view, true);
}

static void about_show_logo(App* app) {
    scene_manager_set_scene_state(app->scene_manager, ScAbout, AboutPageLogo);
    app->screen_started = furi_get_tick();
    view_dispatcher_switch_to_view(app->dispatcher, VPlay);
}

static void draw_filename(Canvas* canvas, const PlayModel* model, const char* title) {
    uint16_t width = canvas_string_width(canvas, title);
    if(width <= 124U) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, title);
        return;
    }

    uint32_t scroll = model->scroll > 10U ? model->scroll - 10U : 0U;
    scroll %= width + 16U;
    int32_t x = 2 - (int32_t)scroll;
    canvas_draw_str_aligned(canvas, x, 32, AlignLeft, AlignCenter, title);
    canvas_draw_str_aligned(canvas, x + width + 16, 32, AlignLeft, AlignCenter, title);
}

void playdraw(Canvas* canvas, void* model) {
    PlayModel* m = model;
    char elapsed[12];
    char frequency[20];
    char g[16];
    char f[20];
    const char* title = m->filename;
    uint32_t secs = m->elapsed_ms / 1000U;
    uint32_t khz;
    if(m->tx && txdraw(canvas, m->elapsed_ms)) return;
    if(m->paused) {
        uint32_t phase = m->pause_ms % 1200U;
        title = phase < 500U ? m->filename : phase < 700U ? "" : phase < 1000U ? "pause" : "";
    }
    snprintf(
        elapsed,
        sizeof(elapsed),
        "%02lu:%02lu",
        (unsigned long)(secs / 60U),
        (unsigned long)(secs % 60U));
    khz = (m->frequency_hz + 500U) / 1000U;
    snprintf(
        frequency,
        sizeof(frequency),
        "%lu.%03lu MHz",
        (unsigned long)(khz / 1000U),
        (unsigned long)(khz % 1000U));
    snprintf(g, sizeof(g), "Gain: %u%s", m->gain / 2, m->gain & 1 ? ".5x" : "x");
    snprintf(f, sizeof(f), "Down: filt %s", m->filter ? "on" : "off");
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignBottom, "github.com/yo3gnd/fmtx");
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, elapsed);
    canvas_set_font(canvas, FontPrimary);
    draw_filename(canvas, m, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, frequency);
    canvas_draw_str(canvas, 2, 62, g);
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, f);
}

static void spdraw(Canvas* c, void* x) {
    PlayModel* m = x;
    txpic(c, m->elapsed_ms / furi_ms_to_ticks(500U));
}

static bool ismp3(const char* x) {
    size_t n = strlen(x);
    if(n < 4U || x[n - 4U] != '.') return false;

    return (x[n - 3U] | 0x20) == 'm' && (x[n - 2U] | 0x20) == 'p' && x[n - 1U] == '3';
}

static bool startsong(App* app, bool paused) {
    PlayReq req;
    PlayModel* m = view_get_model(app->playback_view);
    const char* path = furi_string_get_cstr(app->path);
    const char* slash = strrchr(path, '/');
    bool ok;
    txrand();
    fmtx_playback_request_init(&req, path, app->frequency_hz);
    ok = paused ? fmtx_playback_start_paused(app->playback, &req) :
                  fmtx_playback_start(app->playback, &req);
    m->elapsed_ms = fmtx_playback_position_ms(app->playback);
    m->pause_ms = 0;
    m->frequency_hz = app->frequency_hz;
    m->scroll = 0;
    m->gain = fmtx_playback_gain(app->playback);
    m->filter = fmtx_playback_filter_enabled(app->playback);
    m->tx = fmtx_playback_is_transmitting(app->playback);
    m->paused = ok && fmtx_playback_is_paused(app->playback);
    strlcpy(m->filename, slash ? slash + 1 : path, sizeof(m->filename));
    if(m->paused) app->pause_started = furi_get_tick();
    view_commit_model(app->playback_view, true);

    return ok;
}

static bool movesong(App* app, int move) {
    char folder[256];
    char current[256];
    char name[256];
    char first[256] = "";
    char last[256] = "";
    char prev[256] = "";
    char next[256] = "";
    char chosen[256];
    char path[256];
    const char* selected = furi_string_get_cstr(app->path);
    char* slash;
    Storage* storage;
    File* dir;
    bool opened;
    FileInfo info;
    int n;

    if(strlcpy(folder, selected, sizeof(folder)) >= sizeof(folder)) return false;
    slash = strrchr(folder, '/');
    if(!slash || slash == folder) return false;
    strlcpy(current, slash + 1, sizeof(current));
    *slash = 0;
    storage = furi_record_open(RECORD_STORAGE);
    dir = storage ? storage_file_alloc(storage) : NULL;
    opened = dir && storage_dir_open(dir, folder);
    // Find both neighbours and wrap targets without keeping a directory list.
    while(opened && storage_dir_read(dir, &info, name, sizeof(name))) {
        if((info.flags & FSF_DIRECTORY) || !ismp3(name)) continue;
        if(!first[0] || strcmp(name, first) < 0) strlcpy(first, name, sizeof(first));
        if(!last[0] || strcmp(name, last) > 0) strlcpy(last, name, sizeof(last));
        if(strcmp(name, current) < 0 && (!prev[0] || strcmp(name, prev) > 0))
            strlcpy(prev, name, sizeof(prev));
        if(strcmp(name, current) > 0 && (!next[0] || strcmp(name, next) < 0))
            strlcpy(next, name, sizeof(next));
    }
    if(dir) {
        storage_dir_close(dir);
        storage_file_free(dir);
    }
    if(storage) furi_record_close(RECORD_STORAGE);
    if(!first[0]) return false;
    strlcpy(chosen, move < 0 ? prev[0] ? prev : last : next[0] ? next : first, sizeof(chosen));
    n = snprintf(path, sizeof(path), "%s/%s", folder, chosen);
    if(n < 0 || (size_t)n >= sizeof(path)) return false;
    fmtx_playback_stop(app->playback);
    furi_string_set_str(app->path, path);

    return startsong(app, true);
}

static void checkhold(App* app) {
    if(!app->holding || app->hold_handled) return;
    if(furi_get_tick() - app->hold_started < furi_ms_to_ticks(2000U)) return;
    // Suppress the short seek event which follows a handled hold.
    app->hold_handled = true;
    (void)movesong(app, app->held_key == InputKeyLeft ? -1 : 1);
}

bool playinput(InputEvent* ev, void* ctx) {
    App* app = ctx;
    PlayModel* m;
    if(!ev) return false;
    txcancel(fmtx_playback_position_ms(app->playback));
    if(ev->key == InputKeyLeft || ev->key == InputKeyRight) {
        if(ev->type == InputTypePress) {
            app->held_key = ev->key;
            app->hold_started = furi_get_tick();
            app->holding = true;
            app->hold_handled = false;
        } else if(ev->type == InputTypeLong || ev->type == InputTypeRepeat) {
            checkhold(app);
        } else if(ev->type == InputTypeRelease) {
            checkhold(app);
            app->holding = false;
        } else if(ev->type == InputTypeShort) {
            if(!app->hold_handled) {
                if(ev->key == InputKeyLeft && fmtx_playback_is_paused(app->playback) &&
                   fmtx_playback_position_ms(app->playback) == 0)
                    (void)movesong(app, -1);
                else
                    (void)fmtx_playback_seek_frames(
                        app->playback, ev->key == InputKeyLeft ? -1 : 128);
            }
            m = view_get_model(app->playback_view);
            m->elapsed_ms = fmtx_playback_position_ms(app->playback);
            m->tx = fmtx_playback_is_transmitting(app->playback);
            m->paused = fmtx_playback_is_paused(app->playback);
            view_commit_model(app->playback_view, true);
        } else {
            return false;
        }
        return true;
    }
    if(ev->type != InputTypeShort) return false;
    m = view_get_model(app->playback_view);
    if(ev->key == InputKeyUp)
        m->gain = fmtx_playback_cycle_gain(app->playback);
    else if(ev->key == InputKeyDown)
        m->filter = fmtx_playback_toggle_filter(app->playback);
    else if(ev->key == InputKeyOk) {
        if(!fmtx_playback_toggle_pause(app->playback)) {
            view_commit_model(app->playback_view, false);
            return false;
        }
        m->paused = fmtx_playback_is_paused(app->playback);
        m->pause_ms = 0;
        if(m->paused) app->pause_started = furi_get_tick();
        m->elapsed_ms = fmtx_playback_position_ms(app->playback);
    } else {
        view_commit_model(app->playback_view, false);
        return false;
    }
    m->tx = fmtx_playback_is_transmitting(app->playback);
    view_commit_model(app->playback_view, true);
    return true;
}

void vfodraw(Canvas* canvas, void* model) {
    FmtxVfoViewModel* m = model;
    fmtx_vfo_draw(m->vfo, canvas);
}

bool vfoinput(InputEvent* ev, void* ctx) {
    App* app = ctx;
    FmtxVfoViewModel* m = view_get_model(app->vfo_view);
    bool ok = false;
    bool h = fmtx_vfo_input(m->vfo, ev, &ok);
    view_commit_model(app->vfo_view, h);
    if(ok) {
        app->frequency_hz = fmtx_vfo_frequency(app->vfo);
        (void)fmtx_config_save_frequency(app->frequency_hz);
        view_dispatcher_send_custom_event(app->dispatcher, FmtxVfoDone);
    }
    return h;
}

void abtback(GuiButtonType b, InputType t, void* ctx) {
    App* a = ctx;
    if(t == InputTypeShort && b == GuiButtonTypeLeft)
        view_dispatcher_send_custom_event(a->dispatcher, FmtxAboutBack);
}

static void menucb(void* ctx, uint32_t id) {
    App* app = ctx;
    view_dispatcher_send_custom_event(app->dispatcher, id);
}

static void pickfile(App* app) {
    DialogsFileBrowserOptions opts;
    FuriString* out = furi_string_alloc();
    FuriString* at = furi_string_alloc_set(EXT_PATH("apps_assets/fmtx"));
    dialog_file_browser_set_basic_options(&opts, ".mp3", NULL);
    opts.base_path = EXT_PATH("");
    opts.skip_assets = false;
    if(out && at && dialog_file_browser_show(app->dialogs, out, at, &opts))
        furi_string_set(app->path, out);
    if(at) furi_string_free(at);
    if(out) furi_string_free(out);
}

static void mainin(void* ctx) {
    App* app = ctx;
    submenu_set_header(app->menu, "FM TX");
    submenu_add_item(app->menu, "Start", MStart, menucb, app);
    submenu_add_item(app->menu, "Choose file", MFile, menucb, app);
    submenu_add_item(app->menu, "Settings", MSet, menucb, app);
    submenu_add_item(app->menu, "About", MAbout, menucb, app);
    submenu_set_selected_item(
        app->menu, scene_manager_get_scene_state(app->scene_manager, ScMain));
    view_dispatcher_switch_to_view(app->dispatcher, VMain);
}

static void spin(void* x) {
    App* a = x;
    PlayModel* m = view_get_model(a->playback_view);
    txrand();
    a->screen_started = furi_get_tick();
    m->elapsed_ms = 0;
    view_set_draw_callback(a->playback_view, spdraw);
    view_set_input_callback(a->playback_view, NULL);
    view_commit_model(a->playback_view, true);
    view_dispatcher_switch_to_view(a->dispatcher, VPlay);
}

static bool spev(void* x, SceneManagerEvent ev) {
    App* a = x;
    PlayModel* m;
    uint32_t t;
    if(ev.type != SceneManagerEventTypeTick) return false;
    t = furi_get_tick() - a->screen_started;

    if(t >= furi_ms_to_ticks(2500U)) {
        scene_manager_next_scene(a->scene_manager, ScMain);
        return true;
    }

    m = view_get_model(a->playback_view);
    m->elapsed_ms = t;
    view_commit_model(a->playback_view, true);

    return true;
}

static void spout(void* x) {
    App* a = x;
    view_set_draw_callback(a->playback_view, playdraw);
    view_set_input_callback(a->playback_view, playinput);
}

static bool mainev(void* ctx, SceneManagerEvent ev) {
    App* app = ctx;
    if(ev.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(ev.type != SceneManagerEventTypeCustom) return false;
    if(ev.event == MStart)
        scene_manager_next_scene(app->scene_manager, ScPlay);
    else if(ev.event == MFile)
        pickfile(app);
    else if(ev.event == MSet)
        scene_manager_next_scene(app->scene_manager, FmtxSceneSettings);
    else if(ev.event == MAbout)
        scene_manager_next_scene(app->scene_manager, ScAbout);
    if(ev.event <= MAbout) return true;
    return false;
}

static void mainout(void* ctx) {
    App* app = ctx;
    scene_manager_set_scene_state(
        app->scene_manager, ScMain, submenu_get_selected_item(app->menu));
    submenu_reset(app->menu);
}

static void playin(void* ctx) {
    App* app = ctx;
    app->playback_visible = true;
    (void)startsong(app, false);
    view_dispatcher_switch_to_view(app->dispatcher, VPlay);
}

static bool playev(void* ctx, SceneManagerEvent ev) {
    App* app = ctx;
    if(ev.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeTick && app->playback_visible) {
        PlayModel* m = view_get_model(app->playback_view);
        bool paused = fmtx_playback_is_paused(app->playback);
        if(paused && !m->paused) app->pause_started = furi_get_tick();
        m->elapsed_ms = fmtx_playback_position_ms(app->playback);
        m->tx = fmtx_playback_is_transmitting(app->playback);
        m->paused = paused;
        m->pause_ms = paused ? furi_get_tick() - app->pause_started : 0;
        m->scroll++;
        view_commit_model(app->playback_view, true);
        return true;
    }
    return false;
}

static void playout(void* ctx) {
    App* app = ctx;
    app->playback_visible = false;
    app->holding = false;
    fmtx_playback_stop(app->playback);
}

static void setin(void* ctx) {
    App* app = ctx;
    submenu_set_header(app->settings_menu, "Settings");
    submenu_add_item(
        app->settings_menu, "Transmit frequency", FmtxSettingsSetFrequency, menucb, app);
    view_dispatcher_switch_to_view(app->dispatcher, FmtxViewSettings);
}

static bool setev(void* ctx, SceneManagerEvent ev) {
    App* app = ctx;
    if(ev.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeCustom && ev.event == FmtxSettingsSetFrequency) {
        scene_manager_next_scene(app->scene_manager, FmtxSceneVfo);
        return true;
    }
    return false;
}

static void setout(void* ctx) {
    App* app = ctx;
    submenu_reset(app->settings_menu);
}

static void vfoin(void* ctx) {
    App* app = ctx;
    FmtxVfoViewModel* m;
    fmtx_vfo_begin(app->vfo, app->frequency_hz);
    m = view_get_model(app->vfo_view);
    m->vfo = app->vfo;
    view_commit_model(app->vfo_view, true);
    view_dispatcher_switch_to_view(app->dispatcher, FmtxViewVfo);
}

static bool vfoev(void* ctx, SceneManagerEvent ev) {
    App* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == FmtxVfoDone) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) {
        app->frequency_hz = fmtx_vfo_accept(app->vfo);
        (void)fmtx_config_save_frequency(app->frequency_hz);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

static void vfoout(void* ctx) {
    UNUSED(ctx);
}

static void abtin(void* ctx) {
    App* a = ctx;

    fmtx_playback_stop(a->playback);
    view_set_draw_callback(a->playback_view, about_draw);
    view_set_input_callback(a->playback_view, about_input);
    about_begin(a);
    about_show_logo(a);
}

static bool abtev(void* ctx, SceneManagerEvent ev) {
    App* a = ctx;

    if(ev.type == SceneManagerEventTypeTick &&
       scene_manager_get_scene_state(a->scene_manager, ScAbout) == AboutPageLogo &&
       furi_get_tick() - a->screen_started >= furi_ms_to_ticks(1500U)) {
        a->screen_started = furi_get_tick();
        about_rotate(a);
        return true;
    }
    if(ev.type == SceneManagerEventTypeCustom && ev.event == FmtxAboutNext) {
        scene_manager_set_scene_state(a->scene_manager, ScAbout, AboutPageText);
        view_dispatcher_switch_to_view(a->dispatcher, VAbout);
        return true;
    }
    if((ev.type == SceneManagerEventTypeBack ||
        (ev.type == SceneManagerEventTypeCustom && ev.event == FmtxAboutBack)) &&
       scene_manager_get_scene_state(a->scene_manager, ScAbout) == AboutPageText) {
        about_show_logo(a);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack ||
       (ev.type == SceneManagerEventTypeCustom && ev.event == FmtxAboutBack)) {
        scene_manager_previous_scene(a->scene_manager);
        return true;
    }
    return false;
}

static void abtout(void* ctx) {
    App* app = ctx;

    view_set_draw_callback(app->playback_view, playdraw);
    view_set_input_callback(app->playback_view, playinput);
}

static const AppSceneOnEnterCallback fmtx_on_enter_handlers[] = {
    [ScBoot] = spin,
    [ScMain] = mainin,
    [ScPlay] = playin,
    [FmtxSceneSettings] = setin,
    [FmtxSceneVfo] = vfoin,
    [ScAbout] = abtin,
};

static const AppSceneOnEventCallback fmtx_on_event_handlers[] = {
    [ScBoot] = spev,
    [ScMain] = mainev,
    [ScPlay] = playev,
    [FmtxSceneSettings] = setev,
    [FmtxSceneVfo] = vfoev,
    [ScAbout] = abtev,
};

static const AppSceneOnExitCallback fmtx_on_exit_handlers[] = {
    [ScBoot] = spout,
    [ScMain] = mainout,
    [ScPlay] = playout,
    [FmtxSceneSettings] = setout,
    [FmtxSceneVfo] = vfoout,
    [ScAbout] = abtout,
};

const SceneManagerHandlers scenes = {
    .on_enter_handlers = fmtx_on_enter_handlers,
    .on_event_handlers = fmtx_on_event_handlers,
    .on_exit_handlers = fmtx_on_exit_handlers,
    .scene_num = ScCount,
};
