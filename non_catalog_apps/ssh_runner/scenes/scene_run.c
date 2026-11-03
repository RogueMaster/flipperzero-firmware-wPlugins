/* Run menu, Scripts, Output, Save-as-alias. SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"
#include <ssh_runner_icons.h>
#include "../core/cstr.h"

#define RUN_GLOBAL      100u
#define RUN_TYPE        200u
#define RUN_SCRIPTS     201u
#define RUN_SAVE_OUTPUT 202u
#define RUN_SAVE_ALIAS  203u
#define RUN_LAST_OUTPUT 204u
#define RUN_BAD_CMDS    205u

static bool session_gone(SshRunnerApp* app) {
    SessionPhase p = app->session->phase;
    return p == SessionFailed || p == SessionClosed || p == SessionIdle;
}

static void connection_lost(SshRunnerApp* app) {
    char why[PROTO_STR_MSG + 1]; /* copied by the message scene */
    snprintf(why, sizeof(why), "%s", session_failure_text(app->session));
    app_notify_error(app);
    app_end_session(app);
    app_show_error(app, "Disconnected", why, 0);
}

void commands_load(SshRunnerApp* app) {
    if(app->commands) return;
    app->commands = malloc(sizeof(CommandList));
    store_load_commands(app);
}

void commands_unload(SshRunnerApp* app) {
    free(app->commands);
    app->commands = NULL;
}

static void run_command(SshRunnerApp* app, const char* cmd) {
    if(!app->backend.ops->exec(app->backend.self, cmd)) {
        app_notify_error(app);
        app_show_error(app, "Cannot Run", "The ESP32 is busy", 0);
        return;
    }
    snprintf(app->run_title, sizeof(app->run_title), "%s", cmd);
    app->run_is_script = false;
    app->have_output = true;
    session_begin_run(app->session);
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneOutput);
}

/* ---------------- Run menu ---------------- */

void ssh_runner_scene_run_menu_on_enter(void* context) {
    SshRunnerApp* app = context;
    commands_load(app);
    if(session_gone(app)) {
        connection_lost(app);
        return;
    }
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    /* a stock header is bold and proportional: the alias, or a plain word (user@host is on
     * our own screens in FontKeyboard) */
    submenu_set_header(sm, app->profile.alias[0] ? app->profile.alias : "Run");
    char label[PROFILE_CMD_MAX + 4];
    /* Saved commands first: a repeat run is Hosts > host > command (3 clicks). */
    for(uint8_t i = 0; i < app->profile.cmd_count; i++) {
        snprintf(label, sizeof(label), "$ %s", app->profile.cmds[i]);
        util_submenu_add_ex(sm, label, i, util_submenu_ex_cb, app); /* long-press: see all of it */
    }
    for(uint8_t i = 0; i < app->commands->count; i++) {
        /* a global command the host already has is listed once (its own entry) */
        bool dup = false;
        for(uint8_t j = 0; j < app->profile.cmd_count && !dup; j++)
            dup = strcmp(app->profile.cmds[j], app->commands->cmds[i]) == 0;
        if(dup) continue;
        snprintf(label, sizeof(label), "$ %s", app->commands->cmds[i]);
        util_submenu_add_ex(sm, label, RUN_GLOBAL + i, util_submenu_ex_cb, app);
    }
    if(app->commands->bad) {
        /* listed, not hidden: commands.conf has lines that cannot run as written */
        snprintf(
            label,
            sizeof(label),
            "! %u command%s not shown",
            app->commands->bad,
            app->commands->bad == 1 ? "" : "s");
        util_submenu_add(sm, label, RUN_BAD_CMDS, util_submenu_cb, app);
    }
    util_submenu_add(sm, "Type Command", RUN_TYPE, util_submenu_cb, app);
    util_submenu_add(sm, "Scripts", RUN_SCRIPTS, util_submenu_cb, app);
    if(app->have_output) {
        util_submenu_add(sm, "Last Output", RUN_LAST_OUTPUT, util_submenu_cb, app);
        util_submenu_add(sm, "Save Last Output", RUN_SAVE_OUTPUT, util_submenu_cb, app);
    }
    if(!app->profile_saved)
        util_submenu_add(sm, "Save as Alias", RUN_SAVE_ALIAS, util_submenu_cb, app);
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneRunMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_run_menu_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        app_end_session(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EvtChanged) {
        if((app_take_changes(app) & SESSION_CHG_PHASE) && session_gone(app)) connection_lost(app);
        return true;
    }
    uint32_t e = event.event & ~EVT_LONG;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneRunMenu, e);
    if(event.event & EVT_LONG) {
        bool global = e >= RUN_GLOBAL && e < RUN_GLOBAL + app->commands->count;
        if(e < app->profile.cmd_count || global) {
            app->cmd_index = (uint8_t)(global ? e - RUN_GLOBAL : e);
            app->cmd_global = global;
            app->cmd_in_session = true;
            scene_manager_next_scene(app->scene_manager, SshRunnerSceneCmdDetail);
        }
        return true;
    }
    if(e < app->profile.cmd_count) {
        run_command(app, app->profile.cmds[e]);
    } else if(e >= RUN_GLOBAL && e < RUN_GLOBAL + app->commands->count) {
        run_command(app, app->commands->cmds[e - RUN_GLOBAL]);
    } else if(e == RUN_BAD_CMDS) {
        app_show_message(
            app,
            "Not Shown",
            "Over 127 characters,\nor past the 16th.\nEdit commands.conf\non a computer.",
            SshRunnerSceneRunMenu);
    } else if(e == RUN_TYPE) {
        util_start_input(app, InputCommand, NULL);
    } else if(e == RUN_SCRIPTS) {
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneScripts);
    } else if(e == RUN_LAST_OUTPUT) {
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneOutput);
    } else if(e == RUN_SAVE_OUTPUT) {
        char path[128];
        if(store_save_output(app, path, sizeof(path))) {
            app_notify_success(app);
            /* folder and name each on one FontKeyboard line (the name middle-shortened if it
             * ever is too long), the sentence in words */
            const char* name = strrchr(path, '/');
            name = name ? name + 1 : path;
            char shown[64]; /* copied by the message scene */
            const char* rel = path + strlen(LOGS_DIR) - strlen("logs");
            int k = snprintf(shown, sizeof(shown), "%.*s\n", (int)(name - rel), rel);
            wrap_fit_middle(
                name, 20 * FONTW_MONO, fontw_mono_glyph, NULL, shown + k, sizeof(shown) - k);
            app_show_message_mono(app, "Output Saved", "On the SD card.", shown, 0);
        } else {
            app_notify_error(app);
            app_show_error(app, "Cannot Save", "SD card write failed", 0);
        }
    } else if(e == RUN_SAVE_ALIAS) {
        char suggestion[PROFILE_ALIAS_MAX + 1];
        alias_suggest(app->profile.target.host, suggestion, sizeof(suggestion));
        app->offered_save = true;
        util_start_input(app, InputSaveAlias, suggestion);
    }
    return true;
}

void ssh_runner_scene_run_menu_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}

/* ---------------- Scripts ---------------- */

#define MAX_SCRIPTS 32
/* Only while the Scripts screen is open: 2.3 KB the FAP would otherwise hold in .bss from
 * the start (the loader needs every byte). */
static char (*script_names)[72];
static uint8_t script_count;

static void scripts_scan(SshRunnerApp* app) {
    script_count = 0;
    if(!script_names) script_names = malloc(MAX_SCRIPTS * sizeof(*script_names));
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, SCRIPTS_DIR)) {
        FileInfo info;
        char name[72];
        while(script_count < MAX_SCRIPTS && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info) || !script_name_valid(name)) continue;
            snprintf(script_names[script_count++], sizeof(script_names[0]), "%s", name);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    cstr_sort(&script_names[0][0], script_count, sizeof(script_names[0]));
}

void ssh_runner_scene_scripts_on_enter(void* context) {
    SshRunnerApp* app = context;
    scripts_scan(app);
    if(script_count == 0) {
        Widget* w = app->widget;
        widget_reset(w);
        widget_add_string_element(w, 64, 3, AlignCenter, AlignTop, FontPrimary, "No scripts yet");
        widget_add_string_multiline_element(
            w,
            64,
            36,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "Copy .sh files with qFlipper to\napps_data/ssh_runner/scripts");
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return;
    }
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Scripts");
    for(uint8_t i = 0; i < script_count; i++)
        util_submenu_add(sm, script_names[i], i, util_submenu_cb, app);
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneScripts));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

/* Read the first line and size; fills app->script_* for the confirm dialog. */
static bool script_inspect(SshRunnerApp* app, const char* name) {
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", SCRIPTS_DIR, name);
    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok) {
        app->script_size = (uint32_t)storage_file_size(f);
        char first[64];
        size_t n = storage_file_read(f, first, sizeof(first) - 1);
        first[n] = '\0';
        char* nl = strchr(first, '\n');
        if(nl) *nl = '\0';
        script_interpreter(first, app->script_interp, sizeof(app->script_interp));
        snprintf(app->script_name, sizeof(app->script_name), "%s", name);
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

bool ssh_runner_scene_scripts_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    if(event.event >= script_count) return true;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneScripts, event.event);
    if(!script_inspect(app, script_names[event.event])) {
        app_notify_error(app);
        app_show_error(app, "Cannot Open", script_names[event.event], 0);
        return true;
    }
    if(app->script_size == 0 || app->script_size > PROTO_SCRIPT_MAX) {
        app_notify_error(app);
        app_show_error(
            app,
            "Cannot Run",
            app->script_size ? "Script larger than 64 KB" : "Script is empty",
            0);
        return true;
    }
    /* Scripts are code: always confirm name, size and interpreter. */
    app->confirm_action = ConfirmRunScript;
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
    return true;
}

void ssh_runner_scene_scripts_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
    widget_reset(app->widget);
    free(script_names);
    script_names = NULL;
}

static size_t script_read_cb(void* ctx, uint32_t offset, uint8_t* buf, size_t len) {
    SshRunnerApp* app = ctx;
    if(!app->script_file) return 0;
    if(storage_file_tell(app->script_file) != offset &&
       !storage_file_seek(app->script_file, offset, true))
        return 0;
    return storage_file_read(app->script_file, buf, len);
}

void run_confirmed_script(SshRunnerApp* app) {
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", SCRIPTS_DIR, app->script_name);
    if(app->script_file) {
        storage_file_close(app->script_file);
        storage_file_free(app->script_file);
    }
    app->script_file = storage_file_alloc(app->storage);
    bool ok = storage_file_open(app->script_file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              app->backend.ops->run_script(
                  app->backend.self, app->script_interp, app->script_size, script_read_cb, app);
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SshRunnerSceneRunMenu);
    if(!ok) {
        storage_file_close(app->script_file);
        storage_file_free(app->script_file);
        app->script_file = NULL;
        app_notify_error(app);
        app_show_error(app, "Cannot Run", "The ESP32 is busy", 0);
        return;
    }
    snprintf(
        app->run_title, sizeof(app->run_title), "%s (%s)", app->script_name, app->script_interp);
    app->run_is_script = true;
    app->have_output = true;
    session_begin_run(app->session);
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneOutput);
}

/* ---------------- Output ---------------- */

/* The last non-empty block's trailing newlines go (the body ends on text); a block always
 * starts on a new line, so the marker's leading one goes too. */
static size_t trim_nl(const char* t, size_t n) {
    while(n && t[n - 1] == '\n')
        n--;
    return n;
}

void output_bind(SshRunnerApp* app) {
    Session* s = app->session;
    TextView* tv = app->text_view;
    OutbufParts* p = &app->output_parts;
    outbuf_parts(&s->out, p);
    const char* marker = p->marker;
    size_t marker_len = p->marker_len;
    if(marker_len && marker[0] == '\n') {
        marker++;
        marker_len--;
    }
    if(p->tail_len)
        p->tail_len = trim_nl(p->tail, p->tail_len);
    else if(marker_len)
        marker_len = trim_nl(marker, marker_len);
    else
        p->head_len = trim_nl(p->head, p->head_len);
    /* a title cut with "..." hides part of the command: then it opens the body, as in a
     * terminal, so the whole command is always on this screen */
    const char* title = app->output_title;
    text_view_set_ref(
        tv, 0, title, strlen(title) > TITLE_MONO_CHARS ? strlen(title) : 0, TextViewFontMono);
    if(!p->head_len && !marker_len && !p->tail_len && s->have_done)
        text_view_set_ref(tv, 1, "(no output)", 11, TextViewFontMono);
    else
        text_view_set_ref(tv, 1, p->head, p->head_len, TextViewFontMono);
    text_view_set_ref(tv, 2, marker, marker_len, TextViewFontMono);
    text_view_set_ref(tv, 3, p->tail, p->tail_len, TextViewFontMono);
}

/* The footer's sign: the busy spinner while the command runs, a warning when it failed. */
static const Icon* output_footer_icon(SshRunnerApp* app, const char* footer) {
    if(app->session->running)
        return util_busy
            [(furi_get_tick() * 1000 / furi_kernel_get_tick_frequency() / UTIL_BUSY_MS) %
             UTIL_BUSY_FRAMES];
    return strncmp(footer, "Failed", 6) ? NULL : &I_Alert_9x8;
}

static void output_show(SshRunnerApp* app) {
    char footer[48];
    snprintf(
        app->output_title,
        sizeof(app->output_title),
        "%s %s",
        app->run_is_script ? "#" : "$",
        app->run_title);
    app_output_footer(app, footer, sizeof(footer));
    text_view_lock(app->text_view);
    output_bind(app);
    text_view_unlock(app->text_view, true);
    text_view_set_header(app->text_view, app->output_title, NULL);
    /* a failed run carries the same warning sign as every other error screen */
    app->output_icon = output_footer_icon(app, footer);
    snprintf(app->output_footer, sizeof(app->output_footer), "%s", footer);
    text_view_set_footer(app->text_view, footer, app->output_icon);
    /* the key that stops it, on the right as a status bar's hint */
    text_view_set_footer_hint(
        app->text_view,
        app->session->running ? &I_Back_8x7 : NULL,
        app->session->running ? "Stop" : NULL);
}

static void close_script(SshRunnerApp* app) {
    if(app->script_file) {
        storage_file_close(app->script_file);
        storage_file_free(app->script_file);
        app->script_file = NULL;
    }
}

void ssh_runner_scene_output_on_enter(void* context) {
    SshRunnerApp* app = context;
    commands_unload(app); /* 2 KB back while the output (the peak) is up; Back reloads it */
    text_view_reset(app->text_view);
    text_view_set_follow_end(app->text_view, true);
    app->output_bound = true;
    app->want_ticks = true; /* the busy spinner turns without output */
    output_show(app);
    text_view_scroll_to_end(app->text_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewText);
}

bool ssh_runner_scene_output_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    Session* s = app->session;
    if(event.type == SceneManagerEventTypeBack) {
        if(s->running) {
            /* first Back stops the command; its exit status still arrives */
            app->backend.ops->cancel(app->backend.self);
            return true;
        }
        if(app->quick_connect && !app->offered_save && !app->profile_saved && s->have_done &&
           s->done.exit_code == 0 && s->done.flags == 0) {
            scene_manager_next_scene(app->scene_manager, SshRunnerSceneSaveAlias);
            return true;
        }
        return false;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == EvtChanged) {
        uint32_t chg = app_take_changes(app);
        if(chg & SESSION_CHG_DONE) {
            close_script(app);
            output_show(app);
            if(s->done.exit_code == 0 && s->done.flags == 0)
                app_notify_success(app);
            else
                app_notify_error(app);
        } else if(app->output_dirty && furi_get_tick() - app->output_rendered_at >= OUTPUT_REDRAW_MS) {
            output_show(app);
        } else if(s->running) {
            const Icon* icon = output_footer_icon(app, app->output_footer);
            if(icon != app->output_icon) {
                app->output_icon = icon;
                text_view_set_footer(app->text_view, app->output_footer, icon);
            }
        }
        return true;
    }
    return false;
}

void ssh_runner_scene_output_on_exit(void* context) {
    SshRunnerApp* app = context;
    app->output_bound = false;
    app->want_ticks = false;
    text_view_reset(app->text_view);
}

/* ---------------- Save as alias? (after the first good Quick Connect run) ---------------- */

void ssh_runner_scene_save_alias_on_enter(void* context) {
    SshRunnerApp* app = context;
    app->offered_save = true;
    char* text = util_scratch;
    target_format(&app->profile.target, text, 96);
    util_text_screen(
        app,
        "Save as alias?",
        false,
        "Keep it in Hosts with its own keys and commands.",
        text,
        true,
        "No",
        NULL,
        "Save");
}

bool ssh_runner_scene_save_alias_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EVT_DIALOG_LEFT) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SshRunnerSceneRunMenu);
        return true;
    }
    if(event.event == EVT_DIALOG_RIGHT) {
        char suggestion[PROFILE_ALIAS_MAX + 1];
        alias_suggest(app->profile.target.host, suggestion, sizeof(suggestion));
        util_start_input(app, InputSaveAlias, suggestion);
        return true;
    }
    return false;
}

void ssh_runner_scene_save_alias_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}

/* Called by the keyboard scene for InputSaveAlias. */
void input_save_alias(SshRunnerApp* app) {
    snprintf(app->profile.alias, sizeof(app->profile.alias), "%s", app->input);
    bool in_session = !session_gone(app);
    if(!store_save_profile(app, &app->profile)) {
        app->profile.alias[0] = '\0';
        app_notify_error(app);
        app_show_error(app, "Cannot Save", "SD card write failed", 0);
        return;
    }
    app->profile_saved = true;
    app->quick_connect = false;
    snprintf(app->profile_orig_alias, sizeof(app->profile_orig_alias), "%s", app->profile.alias);
    /* The recent entry now points at the alias. */
    RecentEntry e;
    memset(&e, 0, sizeof(e));
    e.target = app->profile.target;
    e.transport = app->profile.transport;
    for(uint8_t i = 0; i < app->recent.count; i++)
        if(!app->recent.items[i].alias[0] &&
           target_equal(&app->recent.items[i].target, &e.target)) {
            recent_remove(&app->recent, i);
            break;
        }
    snprintf(e.alias, sizeof(e.alias), "%s", app->profile.alias);
    recent_push(&app->recent, &e);
    store_save_recent(app);
    app_notify_success(app);
    app_show_message_mono(
        app,
        "Saved",
        "Saved as a host alias.",
        app->profile.alias,
        in_session ? SshRunnerSceneRunMenu : app->return_scene);
}

/* ---------------- Command detail (long-press a command) ----------------
 * The stock Submenu fits a long label with "..." and never scrolls it, so the whole command
 * is here: FontKeyboard, word-wrapped and scrollable, with Edit / Run / Delete. From the run
 * menu OK still runs at once; this is the long-press. */

/* A command was added, edited or deleted: from a session the host file is saved at once
 * (there is no editor Save there); in the host editor it is saved with the rest. */
void cmd_changed(SshRunnerApp* app) {
    if(app->cmd_in_session && app->profile_saved) {
        if(!store_save_profile(app, &app->profile)) {
            app_notify_error(app);
            app_show_error(app, "Cannot Save", "SD card write failed", 0);
        }
    } else {
        app->profile_dirty = true;
    }
}

static const char* cmd_detail_text(SshRunnerApp* app) {
    if(app->cmd_global)
        return app->cmd_index < app->commands->count ? app->commands->cmds[app->cmd_index] : NULL;
    return app->cmd_index < app->profile.cmd_count ? app->profile.cmds[app->cmd_index] : NULL;
}

void ssh_runner_scene_cmd_detail_on_enter(void* context) {
    SshRunnerApp* app = context;
    const char* cmd = cmd_detail_text(app);
    if(!cmd) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    /* global commands live in commands.conf (edited with qFlipper): Run only */
    util_text_screen(
        app,
        app->cmd_global ? "Global Command" : "Saved Command",
        false,
        NULL,
        cmd,
        true,
        app->cmd_global ? NULL : "Edit",
        app->cmd_in_session ? "Run" : NULL,
        app->cmd_global ? NULL : "Delete");
}

bool ssh_runner_scene_cmd_detail_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EvtChanged) {
        if((app_take_changes(app) & SESSION_CHG_PHASE) && app->cmd_in_session && session_gone(app))
            connection_lost(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    const char* cmd = cmd_detail_text(app);
    if(!cmd) return false;
    if(event.event == EVT_DIALOG_CENTER && app->cmd_in_session) {
        char run[PROFILE_CMD_MAX + 1];
        if(strlen(cmd) > PROFILE_CMD_MAX) return true; /* never cut: loading refuses longer ones */
        memcpy(run, cmd, strlen(cmd) + 1);
        /* Back from Output returns to the run menu, as for a short press */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SshRunnerSceneRunMenu);
        run_command(app, run);
        return true;
    }
    if(app->cmd_global) return false;
    if(event.event == EVT_DIALOG_LEFT) {
        app->cmd_edit_index = app->cmd_index;
        util_start_input(app, InputHostCmd, cmd);
        return true;
    }
    if(event.event == EVT_DIALOG_RIGHT) {
        app->confirm_action = ConfirmDeleteCmd;
        app->confirm_index = app->cmd_index;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
        return true;
    }
    return false;
}

void ssh_runner_scene_cmd_detail_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}
