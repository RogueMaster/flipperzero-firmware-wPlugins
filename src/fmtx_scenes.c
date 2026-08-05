#include "fmtx_scenes.h"
#include "fmtx_anim.h"

#include <stdio.h>
#include <string.h>
#include <storage/storage.h>

const char abttext[] =
    "Built by Richard, YO3GND, a ham radio operator who enjoys embedded engineering and DSP.\n\n"
    "This began as a Morse Flipper spike: send audio to a Baofeng without an audio lead. It worked, so it became its own thing.\n\n"
    "A first-order sigma-delta modulator turns PCM into one-bit PDM. Each bit selects a CC1101 FSK deviation, approximating narrowband FM audio.\n\n"
    "The CC1101 is not an audio transmitter. Its carrier can wander during long transmissions; receiver AFC and bandwidth tolerate some drift. FM capture does not fix it.\n\n"
    "www.yo3gnd.ro\n"
    "github.com/yo3gnd\n"
    "yo3gnd@gmail.com\n"
    "instagram: @yo3gnd\n"
    "tiktok: @yo3gnd\n"
    "youtube.com/@yo3gnd";

void playdraw(Canvas *canvas, void *model)
{
    PlayModel *m = model;
    char elapsed[12];
    char g[16];
    char f[20];
    const char *title = m->filename;
    uint32_t secs = m->elapsed_ms / 1000U;
    if(m->tx && txdraw(canvas, m->elapsed_ms)) return;
    if(m->paused)
    {
        uint32_t phase = m->pause_ms % 1200U;
        title = phase < 500U ? m->filename : phase < 700U ? "" : phase < 1000U ? "pause" : "";
    }
    snprintf(elapsed, sizeof(elapsed), "%02lu:%02lu", (unsigned long)(secs / 60U), (unsigned long)(secs % 60U));
    snprintf(g, sizeof(g), "Gain: %u%s", m->gain / 2, m->gain & 1 ? ".5x" : "x");
    snprintf(f, sizeof(f), "Down: filt %s", m->filter ? "on" : "off");
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, elapsed);
    canvas_draw_str(canvas, 2, 62, g);
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, f);
}

static void spdraw(Canvas *c, void *x)
{
    PlayModel *m = x;
    txpic(c, m->elapsed_ms / furi_ms_to_ticks(500U));
}

static bool ismp3(const char *x)
{
    size_t n = strlen(x);
    if(n < 4U || x[n - 4U] != '.') return false;

    return (x[n - 3U] | 0x20) == 'm' && (x[n - 2U] | 0x20) == 'p' && x[n - 1U] == '3';
}

static bool startsong(App *app, bool paused)
{
    PlayReq req;
    PlayModel *m = view_get_model(app->pv);
    const char *path = furi_string_get_cstr(app->path);
    const char *slash = strrchr(path, '/');
    bool ok;
    playreq(&req, path, app->hz);
    ok = paused ? playpaused(app->play, &req) : playstart(app->play, &req);
    m->elapsed_ms = playms(app->play);
    m->pause_ms = 0;
    m->gain = playgain(app->play);
    m->filter = playfilter(app->play);
    m->tx = playtx(app->play);
    m->paused = ok && ispaused(app->play);
    strlcpy(m->filename, slash ? slash + 1 : path, sizeof(m->filename));
    if(m->paused) app->pauseat = furi_get_tick();
    view_commit_model(app->pv, true);

    return ok;
}

static bool movesong(App *app, int move)
{
    char folder[256];
    char current[256];
    char name[256];
    char first[256] = "";
    char x1[256] = "";
    char prev[256] = "";
    char next[256] = "";
    char pick[256];
    char path[256];
    const char *selected = furi_string_get_cstr(app->path);
    char *slash;
    Storage *storage;
    File *dir;
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
    while(opened && storage_dir_read(dir, &info, name, sizeof(name)))
    {
        if((info.flags & FSF_DIRECTORY) || !ismp3(name)) continue;
        if(!first[0] || strcmp(name, first) < 0) strlcpy(first, name, sizeof(first));
        if(!x1[0] || strcmp(name, x1) > 0) strlcpy(x1, name, sizeof(x1));
        if(strcmp(name, current) < 0 && (!prev[0] || strcmp(name, prev) > 0)) strlcpy(prev, name, sizeof(prev));
        if(strcmp(name, current) > 0 && (!next[0] || strcmp(name, next) < 0)) strlcpy(next, name, sizeof(next));
    }
    if(dir)
    {
        storage_dir_close(dir);
        storage_file_free(dir);
    }
    if(storage) furi_record_close(RECORD_STORAGE);
    if(!first[0]) return false;
    strlcpy(pick, move < 0 ? prev[0] ? prev : x1 : next[0] ? next : first, sizeof(pick));
    n = snprintf(path, sizeof(path), "%s/%s", folder, pick);
    if(n < 0 || (size_t)n >= sizeof(path)) return false;
    playstop(app->play);
    furi_string_set_str(app->path, path);

    return startsong(app, true);
}

static void checkhold(App *app)
{
    if(!app->holding || app->heldskip) return;
    if(furi_get_tick() - app->holdat < furi_ms_to_ticks(2000U)) return;
    app->heldskip = true;
    (void)movesong(app, app->holdkey == InputKeyLeft ? -1 : 1);
}

bool playinput(InputEvent *ev, void *ctx)
{
    App *app = ctx;
    PlayModel *m;
    if(!ev) return false;
    if(ev->key == InputKeyLeft || ev->key == InputKeyRight)
    {
        if(ev->type == InputTypePress)
        {
            app->holdkey = ev->key;
            app->holdat = furi_get_tick();
            app->holding = true;
            app->heldskip = false;
        }
        else if(ev->type == InputTypeLong || ev->type == InputTypeRepeat)
        {
            checkhold(app);
        }
        else if(ev->type == InputTypeRelease)
        {
            checkhold(app);
            app->holding = false;
        }
        else if(ev->type == InputTypeShort)
        {
            if(!app->heldskip)
            {
                if(ev->key == InputKeyLeft && ispaused(app->play) && playms(app->play) == 0) (void)movesong(app, -1);
                else (void)playseek(app->play, ev->key == InputKeyLeft ? -1 : 128);
            }
            m = view_get_model(app->pv);
            m->elapsed_ms = playms(app->play);
            m->tx = playtx(app->play);
            m->paused = ispaused(app->play);
            view_commit_model(app->pv, true);
        }
        else
        {
            return false;
        }
        return true;
    }
    if(ev->type != InputTypeShort) return false;
    m = view_get_model(app->pv);
    if(ev->key == InputKeyUp) m->gain = gainup(app->play);
    else if(ev->key == InputKeyDown) m->filter = filtertoggle(app->play);
    else if(ev->key == InputKeyOk)
    {
        if(!playenter(app->play))
        {
            view_commit_model(app->pv, false);
            return false;
        }
        m->paused = ispaused(app->play);
        m->pause_ms = 0;
        if(m->paused) app->pauseat = furi_get_tick();
        m->elapsed_ms = playms(app->play);
    }
    else
    {
        view_commit_model(app->pv, false);
        return false;
    }
    m->tx = playtx(app->play);
    view_commit_model(app->pv, true);
    return true;
}

void vfodraw(Canvas *canvas, void *model)
{
    FmtxVfoViewModel *m = model;
    fmtx_vfo_draw(m->vfo, canvas);
}

bool vfoinput(InputEvent *ev, void *ctx)
{
    App *app = ctx;
    FmtxVfoViewModel *m = view_get_model(app->vv);
    bool ok = false;
    bool h = fmtx_vfo_input(m->vfo, ev, &ok);
    view_commit_model(app->vv, h);
    if(ok)
    {
        app->hz = fmtx_vfo_frequency(app->vfo);
        (void)cfgsave(app->hz);
        view_dispatcher_send_custom_event(app->vd, FmtxVfoDone);
    }
    return h;
}

void abtback(GuiButtonType b, InputType t, void *ctx)
{
    App *a = ctx;
    if(t == InputTypeShort && b == GuiButtonTypeLeft) view_dispatcher_send_custom_event(a->vd, MAbout);
}

static void menucb(void *ctx, uint32_t id)
{
    App *app = ctx;
    view_dispatcher_send_custom_event(app->vd, id);
}

static void pickfile(App *app)
{
    DialogsFileBrowserOptions opts;
    FuriString *out = furi_string_alloc();
    FuriString *at = furi_string_alloc_set(EXT_PATH("yo3gnd_audio"));
    dialog_file_browser_set_basic_options(&opts, ".mp3", NULL);
    opts.base_path = EXT_PATH("");
    opts.skip_assets = false;
    if(out && at && dialog_file_browser_show(app->dlg, out, at, &opts)) furi_string_set(app->path, out);
    if(at) furi_string_free(at);
    if(out) furi_string_free(out);
}

static void mainin(void *ctx)
{
    App *app = ctx;
    submenu_set_header(app->menu, "FM TX");
    submenu_add_item(app->menu, "Start", MStart, menucb, app);
    submenu_add_item(app->menu, "Choose file", MFile, menucb, app);
    submenu_add_item(app->menu, "Settings", MSet, menucb, app);
    submenu_add_item(app->menu, "About", MAbout, menucb, app);
    submenu_set_selected_item(app->menu, scene_manager_get_scene_state(app->sm, ScMain));
    view_dispatcher_switch_to_view(app->vd, VMain);
}

static void spin(void *x)
{
    App *a = x;
    PlayModel *m = view_get_model(a->pv);
    a->spat = furi_get_tick();
    m->elapsed_ms = 0;
    view_set_draw_callback(a->pv, spdraw);
    view_set_input_callback(a->pv, NULL);
    view_commit_model(a->pv, true);
    view_dispatcher_switch_to_view(a->vd, VPlay);
}

static bool spev(void *x, SceneManagerEvent ev)
{
    App *a = x;
    PlayModel *m;
    uint32_t t;
    if(ev.type != SceneManagerEventTypeTick) return false;
    t = furi_get_tick() - a->spat;

    if(t >= furi_ms_to_ticks(2500U))
    {
        scene_manager_next_scene(a->sm, ScMain);
        return true;
    }

    m = view_get_model(a->pv);
    m->elapsed_ms = t;
    view_commit_model(a->pv, true);


    return true;
}

static void spout(void *x)
{
    App *a = x;
    view_set_draw_callback(a->pv, playdraw);
    view_set_input_callback(a->pv, playinput);
}

static bool mainev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeBack)
    {
        view_dispatcher_stop(app->vd);
        return true;
    }
    if(ev.type != SceneManagerEventTypeCustom) return false;
    if(ev.event == MStart) scene_manager_next_scene(app->sm, ScPlay);
    else if(ev.event == MFile) pickfile(app);
    else if(ev.event == MSet) scene_manager_next_scene(app->sm, FmtxSceneSettings);
    else if(ev.event == MAbout) scene_manager_next_scene(app->sm, ScAbout);
    if(ev.event <= MAbout) return true;
    return false;
}

static void mainout(void *ctx)
{
    App *app = ctx;
    scene_manager_set_scene_state(app->sm, ScMain, submenu_get_selected_item(app->menu));
    submenu_reset(app->menu);
}

static void playin(void *ctx)
{
    App *app = ctx;
    app->playing = true;
    (void)startsong(app, false);
    view_dispatcher_switch_to_view(app->vd, VPlay);
}

static bool playev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeBack)
    {
        scene_manager_previous_scene(app->sm);
        return true;
    }
    if(ev.type == SceneManagerEventTypeTick && app->playing)
    {
        PlayModel *m = view_get_model(app->pv);
        bool paused = ispaused(app->play);
        if(paused && !m->paused) app->pauseat = furi_get_tick();
        m->elapsed_ms = playms(app->play);
        m->tx = playtx(app->play);
        m->paused = paused;
        m->pause_ms = paused ? furi_get_tick() - app->pauseat : 0;
        view_commit_model(app->pv, true);
        return true;
    }
    return false;
}

static void playout(void *ctx)
{
    App *app = ctx;
    app->playing = false;
    app->holding = false;
    playstop(app->play);
}

static void setin(void *ctx)
{
    App *app = ctx;
    submenu_set_header(app->setmenu, "Settings");
    submenu_add_item(app->setmenu, "Transmit frequency", FmtxSettingsSetFrequency, menucb, app);
    view_dispatcher_switch_to_view(app->vd, FmtxViewSettings);
}

static bool setev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeBack)
    {
        scene_manager_previous_scene(app->sm);
        return true;
    }
    if(ev.type == SceneManagerEventTypeCustom && ev.event == FmtxSettingsSetFrequency)
    {
        scene_manager_next_scene(app->sm, FmtxSceneVfo);
        return true;
    }
    return false;
}

static void setout(void *ctx)
{
    App *app = ctx;
    submenu_reset(app->setmenu);
}

static void vfoin(void *ctx)
{
    App *app = ctx;
    FmtxVfoViewModel *m;
    fmtx_vfo_begin(app->vfo, app->hz);
    m = view_get_model(app->vv);
    m->vfo = app->vfo;
    view_commit_model(app->vv, true);
    view_dispatcher_switch_to_view(app->vd, FmtxViewVfo);
}

static bool vfoev(void *ctx, SceneManagerEvent ev)
{
    App *app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == FmtxVfoDone)
    {
        scene_manager_previous_scene(app->sm);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack)
    {
        app->hz = fmtx_vfo_accept(app->vfo);
        (void)cfgsave(app->hz);
        scene_manager_previous_scene(app->sm);
        return true;
    }
    return false;
}

static void vfoout(void *ctx)
{
    UNUSED(ctx);
}

static void abtin(void *ctx)
{
    App *a = ctx;
    view_dispatcher_switch_to_view(a->vd, VAbout);
}

static bool abtev(void *ctx, SceneManagerEvent ev)
{
    App *a = ctx;
    if(ev.type == SceneManagerEventTypeBack || (ev.type == SceneManagerEventTypeCustom && ev.event == MAbout))
    {
        scene_manager_previous_scene(a->sm);
        return true;
    }
    return false;
}

static void abtout(void *ctx)
{
    UNUSED(ctx);
}

static const AppSceneOnEnterCallback fmtx_on_enter_handlers[] =
{
    [ScBoot] = spin,
    [ScMain] = mainin,
    [ScPlay] = playin,
    [FmtxSceneSettings] = setin,
    [FmtxSceneVfo] = vfoin,
    [ScAbout] = abtin,
};

static const AppSceneOnEventCallback fmtx_on_event_handlers[] =
{
    [ScBoot] = spev,
    [ScMain] = mainev,
    [ScPlay] = playev,
    [FmtxSceneSettings] = setev,
    [FmtxSceneVfo] = vfoev,
    [ScAbout] = abtev,
};

static const AppSceneOnExitCallback fmtx_on_exit_handlers[] =
{
    [ScBoot] = spout,
    [ScMain] = mainout,
    [ScPlay] = playout,
    [FmtxSceneSettings] = setout,
    [FmtxSceneVfo] = vfoout,
    [ScAbout] = abtout,
};

const SceneManagerHandlers scenes =
{
    .on_enter_handlers = fmtx_on_enter_handlers,
    .on_event_handlers = fmtx_on_event_handlers,
    .on_exit_handlers = fmtx_on_exit_handlers,
    .scene_num = ScCount,
};
