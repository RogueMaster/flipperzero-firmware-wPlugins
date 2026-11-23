// The editor: opening a file, its input loop and the Save and Cut actions.

#include "app.h"

// Save A..B as RAW, or with dc/d the decoded entry d as a key file.
void prompt_and_save(
    Gui *gui, ViewPort *vp, FuriMessageQueue *queue, Storage *st, App *a, DecodeCtx *dc, const Decoded *d)
{
    char namebuf[64];
    if (d)
    {
        char base[48];
        decoded_base_name(dc, d, base, sizeof(base));
        propose_name(st, base, "_", true, namebuf, sizeof(namebuf));
    }
    else
        propose_edit_name(st, a, namebuf, sizeof(namebuf));

    FuriString *path = furi_string_alloc();
    bool write = false;
    snprintf(a->status, sizeof(a->status), "Cancelled");
    while (prompt_filename(gui, namebuf, sizeof(namebuf)) && namebuf[0] != '\0')
    {
        furi_string_printf(path, "%s/%s.sub", SUBGHZ_DIR, namebuf);
        if (!storage_file_exists(st, furi_string_get_cstr(path)))
            write = true;
        else if (a->src_in_dir && strcasecmp(namebuf, a->basename) == 0)
            snprintf(a->status, sizeof(a->status), "Can't overwrite open file");
        else if (!confirm(gui, "Name exists,\noverwrite?"))
            continue;
        else
            write = true;

        break;
    }

    if (write)
    {
        a->busy = "Saving...";
        view_port_update(vp);

        const char *p = furi_string_get_cstr(path);
        if (d ? write_decoded(st, a, dc, d, p) : write_selection(st, a, p))
            snprintf(a->status, sizeof(a->status), "Saved %s.sub", namebuf);

        a->busy = NULL;
    }

    furi_string_free(path);

    a->status_until = furi_get_tick() + 2000;
    furi_message_queue_reset(queue);
    view_port_update(vp);
}

static void do_cut_action(Gui *gui, FuriMessageQueue *queue, DialogsApp *dialogs, App *a)
{
    furi_mutex_acquire(a->mutex, FuriWaitForever);

    int32_t lo = a->marker_a, hi = a->marker_b;
    if (lo > hi)
    {
        int32_t t = lo;
        lo = hi;
        hi = t;
    }

    if (hi - lo < 1)
    {
        snprintf(a->status, sizeof(a->status), "Range too small");
        a->status_until = furi_get_tick() + 1500;
        furi_mutex_release(a->mutex);
        return;
    }

    if (cut_run(a, false) < 2)
    {
        furi_mutex_release(a->mutex);

        show_message(dialogs, "Cannot cut", "That would leave the\nsignal empty.", 30);
        furi_message_queue_reset(queue);
        return;
    }

    bool undo = snapshot_take(a);
    if (!undo)
    {
        furi_mutex_release(a->mutex);
        bool go = confirm(gui, "No RAM for Undo.\nCut anyway?");
        furi_message_queue_reset(queue);
        furi_mutex_acquire(a->mutex, FuriWaitForever);

        if (!go)
        {
            snprintf(a->status, sizeof(a->status), "Cut cancelled");
            a->status_until = furi_get_tick() + 1500;
            furi_mutex_release(a->mutex);
            return;
        }

        // An older snapshot would undo two cuts at once; dropping it may also free enough
        // for this one.
        free(a->undo_data);
        a->undo_data = NULL;
        a->has_undo = false;
        undo = snapshot_take(a);
    }

    cut_compact(a);
    snprintf(a->status, sizeof(a->status), undo ? "Cut" : "Cut, no Undo");
    a->status_until = furi_get_tick() + 1200;
    furi_mutex_release(a->mutex);
}

void run_editor_session(
    Gui *gui,
    ViewPort *vp,
    FuriMessageQueue *queue,
    Storage *storage,
    DialogsApp *dialogs,
    App *app,
    bool auto_select)
{
    app->busy = NULL;
    app->opened_tick = furi_get_tick();
    if (auto_select)
    {
        auto_detect(app);
    }
    else
    {
        app->marker_a = 0;
        app->marker_b = app->sd.total_us;
        app->view_start = 0;
        app->view_end = app->sd.total_us > 0 ? app->sd.total_us : 1;
        clamp_view(app);
    }
    recompute_overview(app);
    recompute_activity(app);
    furi_message_queue_reset(queue);
    view_port_update(vp);

    bool running = true;
    InputEvent e;
    while (running)
    {
        if (furi_message_queue_get(queue, &e, 100) == FuriStatusOk)
        {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool changed = false;
            bool want_save = false;
            bool want_decode = false;
            bool want_cut = false;

            if (app->mode == EditModeMenu)
            {
                if (e.type == InputTypeShort)
                {
                    if (e.key == InputKeyDown)
                    {
                        app->menu_sel = (app->menu_sel + 1) % MENU_ITEMS;
                    }
                    else if (e.key == InputKeyUp)
                    {
                        app->menu_sel = (app->menu_sel + MENU_ITEMS - 1) % MENU_ITEMS;
                    }
                    else if (e.key == InputKeyOk)
                    {
                        app->mode = EditModeEdit;
                        if (app->menu_sel == MENU_SEL_SAVE)
                        {
                            want_save = true;
                        }
                        else if (app->menu_sel == MENU_SEL_DECODE)
                        {
                            want_decode = true;
                        }
                        else if (app->menu_sel == MENU_SEL_CUT)
                        {
                            want_cut = true;
                        }
                        else if (app->menu_sel == MENU_SEL_UNDO)
                        {
                            if (app->has_undo)
                            {
                                snapshot_restore(app);
                                snprintf(app->status, sizeof(app->status), "Undone");
                            }
                            else
                            {
                                snprintf(app->status, sizeof(app->status), "Nothing to undo");
                            }

                            app->status_until = furi_get_tick() + 1500;
                        }
                    }
                    else if (e.key == InputKeyBack)
                    {
                        app->mode = EditModeEdit;
                    }
                }
            }
            else if (e.type == InputTypePress || e.type == InputTypeRepeat)
            {
                int32_t span = app->view_end - app->view_start;
                int32_t step = span / SCREEN_W_PX;
                if (step < 1)
                    step = 1;

                if (e.type == InputTypeRepeat)
                {
                    int32_t fast = span / 24;
                    if (fast > step)
                        step = fast;
                }

                int32_t *m = app->active ? &app->marker_b : &app->marker_a;
                switch (e.key)
                {
                case InputKeyLeft:
                    *m -= step;
                    clamp_marker(app, app->active);
                    ensure_visible(app, *m);
                    changed = true;
                    break;
                case InputKeyRight:
                    *m += step;
                    clamp_marker(app, app->active);
                    ensure_visible(app, *m);
                    changed = true;
                    break;
                case InputKeyUp:
                    zoom(app, true);
                    changed = true;
                    break;
                case InputKeyDown:
                    zoom(app, false);
                    changed = true;
                    break;
                default:
                    break;
                }
            }
            else if (e.type == InputTypeShort)
            {
                if (e.key == InputKeyOk)
                {
                    app->active ^= 1;
                    ensure_visible(app, app->active ? app->marker_b : app->marker_a);
                    changed = true;
                }
                else if (e.key == InputKeyBack)
                {
                    running = false;
                }
            }
            else if (e.type == InputTypeLong)
            {
                if (e.key == InputKeyOk)
                {
                    app->mode = EditModeMenu;
                    app->menu_sel = 0;
                    selection_info(app);
                }
                else if (e.key == InputKeyBack)
                {
                    running = false;
                }
            }

            if (changed)
                recompute_activity(app);

            furi_mutex_release(app->mutex);

            if (want_save)
            {
                prompt_and_save(gui, vp, queue, storage, app, NULL, NULL);
            }
            else if (want_decode)
            {
                do_decode_action(gui, vp, queue, storage, app);
            }
            else if (want_cut)
            {
                do_cut_action(gui, queue, dialogs, app);
            }

            view_port_update(vp);
        }
        else
        {
            view_port_update(vp);
        }
    }
}


// Edit `file`, or a file picked in the browser when NULL.
void run_editor(Storage *storage, DialogsApp *dialogs, const char *file)
{
    App *app = safe_malloc(sizeof(App));
    if (!app)
    {
        show_message(dialogs, "Out of memory", "Not enough RAM to\nopen the editor.\nReboot Flipper.", 32);
        return;
    }

    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    FuriString *path = furi_string_alloc_set(file ? file : SUBGHZ_DIR);
    if (!file)
    {
        DialogsFileBrowserOptions br;
        dialog_file_browser_set_basic_options(&br, ".sub", &I_sub1_10px);
        br.base_path = SUBGHZ_DIR;
        if (!dialog_file_browser_show(dialogs, path, path, &br))
            goto cleanup;
    }

    {
        const char *full = furi_string_get_cstr(path);
        const char *slash = strrchr(full, '/');
        const char *nm = slash ? slash + 1 : full;
        strncpy(app->basename, nm, sizeof(app->basename) - 1);
        char *dot = strrchr(app->basename, '.');
        if (dot)
            *dot = '\0';

        size_t dl = strlen(SUBGHZ_DIR);
        app->src_in_dir = slash && (size_t)(slash - full) == dl && strncmp(full, SUBGHZ_DIR, dl) == 0 &&
                          strlen(nm) < sizeof(app->basename);
    }

    Gui *gui = furi_record_open(RECORD_GUI);

    // Loading screen first: probing the buttons takes a moment and the menu would show meanwhile
    ViewPort *vp = view_port_alloc();
    FuriMessageQueue *queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);
    app->busy = busy_loading;
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    view_port_update(vp);

    if (!btn_select_for_file(gui, storage, furi_string_get_cstr(path)))
    {
        gui_remove_view_port(gui, vp);
        view_port_free(vp);
        furi_message_queue_free(queue);
        furi_record_close(RECORD_GUI);
        goto cleanup;
    }
    furi_message_queue_reset(queue);

    bool loaded = (g_merge_repeat > 1)
                      ? load_sub_repeated(storage, furi_string_get_cstr(path), &app->sd)
                      : load_sub(storage, furi_string_get_cstr(path), &app->sd);

    if (app->sd.out_of_memory || !loaded)
    {
        gui_remove_view_port(gui, vp);
        view_port_free(vp);
        furi_message_queue_free(queue);
        furi_record_close(RECORD_GUI);

        if (app->sd.out_of_memory)
            show_message(dialogs, "Out of memory",
                         "Not enough free RAM to\nload this capture.\nReboot Flipper, then\nopen the app first.", 34);
        else
            show_message(dialogs, APP_NAME, "Unsupported protocol or file is empty", 32);

        goto cleanup;
    }

    if (app->sd.truncated)
    {
        char msg[96];
        snprintf(
            msg,
            sizeof(msg),
            "Not enough RAM for all of\nit. Loaded the first\n%u samples (%ld.%ld s).",
            (unsigned)app->sd.count,
            (long)(app->sd.total_us / 1000000),
            (long)(app->sd.total_us / 100000 % 10));

        show_message(dialogs, "Signal truncated", msg, 34);
        furi_message_queue_reset(queue);
    }

    run_editor_session(gui, vp, queue, storage, dialogs, app, true);

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_record_close(RECORD_GUI);

cleanup:
    furi_string_free(path);

    if (app->sd.data)
        free(app->sd.data);

    if (app->undo_data)
        free(app->undo_data);

    furi_mutex_free(app->mutex);
    free(app);
}
