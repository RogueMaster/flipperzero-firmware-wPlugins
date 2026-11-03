/* Hosts list, the host page (Connect, its settings, Delete), saved commands, trusted keys.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"

#define HOSTS_ADD 999u

static void start_connect(SshRunnerApp* app, uint32_t return_scene) {
    app->return_scene = return_scene;
    app->offered_save = false;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneConnect, 0);
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneConnect);
}

void hosts_connect_profile(SshRunnerApp* app, uint32_t return_scene) {
    app->quick_connect = !app->profile_saved;
    start_connect(app, return_scene);
}

/* ---------------- Hosts ---------------- */

void ssh_runner_scene_hosts_on_enter(void* context) {
    SshRunnerApp* app = context;
    store_load_hosts(app);
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Hosts");
    /* the alias alone: user and address are one level in, on the host page */
    HostProfile* tmp = malloc(sizeof(HostProfile));
    for(uint8_t i = 0; i < app->host_count; i++) {
        char label[PROFILE_ALIAS_MAX + 16];
        if(store_load_profile(app, app->host_aliases[i], tmp))
            snprintf(label, sizeof(label), "%s", tmp->alias);
        else
            snprintf(label, sizeof(label), "%s (unreadable)", app->host_aliases[i]);
        util_submenu_add(sm, label, i, util_submenu_cb, app);
    }
    free(tmp);
    if(app->host_count == 0) {
        Widget* w = app->widget;
        widget_reset(w);
        widget_add_string_element(w, 64, 3, AlignCenter, AlignTop, FontPrimary, "Hosts");
        widget_add_string_multiline_element(
            w,
            64,
            30,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "No hosts yet.\nAdd one here, or save\none after Quick Connect.");
        widget_add_button_element(w, GuiButtonTypeCenter, "Add", util_widget_button_cb, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return;
    }
    util_submenu_add(sm, "+ Add Host", HOSTS_ADD, util_submenu_cb, app);
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneHosts));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

static bool load_selected(SshRunnerApp* app, uint32_t idx) {
    if(idx >= app->host_count) return false;
    if(!store_load_profile(app, app->host_aliases[idx], &app->profile)) {
        app_notify_error(app);
        char why[PROFILE_ALIAS_MAX + 48]; /* copied by the message scene */
        snprintf(
            why,
            sizeof(why),
            "%s\n%s.\nEdit it on a computer.",
            app->host_aliases[idx],
            profile_error());
        app_show_error(app, "Cannot Read Host", why, 0);
        return false;
    }
    app->profile_saved = true;
    app->profile_dirty = false;
    snprintf(app->profile_orig_alias, sizeof(app->profile_orig_alias), "%s", app->profile.alias);
    return true;
}

bool ssh_runner_scene_hosts_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EVT_DIALOG_CENTER)
        event.event = HOSTS_ADD; /* empty state: the Add button */
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    uint32_t idx = event.event;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneHosts, idx);
    if(idx == HOSTS_ADD) {
        profile_init(&app->profile);
        app->profile_saved = false;
        app->profile_dirty = false;
        app->profile_orig_alias[0] = '\0';
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneHostEdit, 0);
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneHostEdit);
        return true;
    }
    if(!load_selected(app, idx)) return true;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneHostEdit, HOST_EDIT_CONNECT);
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneHostEdit);
    return true;
}

void ssh_runner_scene_hosts_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
    widget_reset(app->widget);
}

/* ---------------- Host page ----------------
 * Connect on top, the settings, Save, Delete at the bottom (a saved host; a new one has only
 * the settings and Save). */

enum {
    EditAlias,
    EditUser,
    EditHost,
    EditPort,
    EditAuth,
    EditRemember,
    EditTransport,
    EditPolicy,
    EditCommands,
    EditKeys,
    EditSave,
    EditConnect,
    EditDelete,
};
_Static_assert(EditAuth == HOST_EDIT_AUTH_ROW && EditConnect == HOST_EDIT_CONNECT, "host page ids");

/* list values show at most 37 px: "Password" (40 px) would be cut */
static const char* const yesno[] = {"No", "Yes"};
static const char* const transport_names[] = {"Auto", "LAN", "WiFi"};
static const char* const policy_names[] = {"Prompt", "Strict", "Off"};

/* text rows ("label   value" flush right, util_label_flush): Alias, User, Host, Port, Commands */
#define edit_vals ((char(*)[64])util_scratch) /* 5 rows of 64: the whole scratch */
static char edit_auth[KEY_NAME_MAX + 12];
/* list row -> Edit* item: "Save Password" is only there for password hosts */
static uint8_t edit_rows[EditDelete + 1];
static uint8_t edit_row_count;
#define EVT_EDIT_REBUILD (EvtBase + 300)

/* Auth values: Password, Default key, every key in keys/, and the host's own key when
 * its file is gone ("<name> (missing)"), so opening the editor never changes the host. */
static bool auth_missing(SshRunnerApp* app) {
    const HostProfile* p = &app->profile;
    return p->auth == ProfileAuthKey && (!app->keys || keylist_find(app->keys, p->key) < 0);
}

static uint8_t auth_count(SshRunnerApp* app) {
    return (uint8_t)(2 + (app->keys ? app->keys->count : 0) + (auth_missing(app) ? 1 : 0));
}

static uint8_t auth_index(SshRunnerApp* app) {
    const HostProfile* p = &app->profile;
    if(p->auth == ProfileAuthPassword) return 0;
    if(p->auth == ProfileAuthDefault) return 1;
    int i = app->keys ? keylist_find(app->keys, p->key) : -1;
    return (uint8_t)(i >= 0 ? 2 + i : 2 + (app->keys ? app->keys->count : 0));
}

static void auth_show(SshRunnerApp* app, VariableItem* it) {
    const HostProfile* p = &app->profile;
    if(p->auth == ProfileAuthPassword)
        snprintf(edit_auth, sizeof(edit_auth), "Passwd");
    else if(p->auth == ProfileAuthDefault)
        snprintf(edit_auth, sizeof(edit_auth), "Default");
    else
        snprintf(
            edit_auth, sizeof(edit_auth), "%s%s", p->key, auth_missing(app) ? " (missing)" : "");
    util_fit_value(edit_auth, sizeof(edit_auth));
    variable_item_set_current_value_text(it, edit_auth);
}

/* A row edited by typing: its value flush right like every text value on the page (the rows
 * with < > choices keep the stock value column). */
static void text_row(
    VariableItemList* vl,
    char* buf,
    const char* label,
    const char* value,
    const char* empty,
    void* ctx) {
    util_label_value(vl, buf, sizeof(edit_vals[0]), label, value[0] ? value : empty, ctx);
}

static void edit_auth_changed(VariableItem* it) {
    SshRunnerApp* app = variable_item_get_context(it);
    HostProfile* p = &app->profile;
    bool was_password = p->auth == ProfileAuthPassword;
    uint8_t i = variable_item_get_current_value_index(it);
    uint8_t keys = app->keys ? app->keys->count : 0;
    if(i == 0) {
        p->auth = ProfileAuthPassword;
    } else if(i == 1) {
        p->auth = ProfileAuthDefault;
    } else if(i < 2 + keys) {
        p->auth = ProfileAuthKey;
        snprintf(p->key, sizeof(p->key), "%s", app->keys->e[i - 2].name);
    } else {
        p->auth = ProfileAuthKey; /* back on the missing key, still named in p->key */
    }
    auth_show(app, it);
    app->profile_dirty = true;
    /* "Save Password" comes or goes */
    if(was_password != (p->auth == ProfileAuthPassword))
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_EDIT_REBUILD);
}

static void edit_remember_changed(VariableItem* it) {
    SshRunnerApp* app = variable_item_get_context(it);
    app->profile.remember = variable_item_get_current_value_index(it) == 1;
    variable_item_set_current_value_text(it, yesno[app->profile.remember]);
    app->profile_dirty = true;
}

static void edit_transport_changed(VariableItem* it) {
    SshRunnerApp* app = variable_item_get_context(it);
    app->profile.transport = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, transport_names[app->profile.transport]);
    app->profile_dirty = true;
}

static void edit_policy_changed(VariableItem* it) {
    SshRunnerApp* app = variable_item_get_context(it);
    app->profile.policy = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, policy_names[app->profile.policy]);
    app->profile_dirty = true;
}

void ssh_runner_scene_host_edit_on_enter(void* context) {
    SshRunnerApp* app = context;
    HostProfile* p = &app->profile;
    VariableItemList* vl = app->var_list;
    variable_item_list_reset(vl);
    variable_item_list_set_enter_callback(vl, util_var_list_enter_cb, app);
    keys_load(app);
    uint8_t row = 0;
    if(app->profile_saved) {
        variable_item_list_add(vl, "Connect", 1, NULL, app);
        edit_rows[row++] = EditConnect;
    }
    for(uint8_t e = EditAlias; e <= EditAuth; e++)
        edit_rows[row++] = e;

    text_row(vl, edit_vals[0], "Alias", p->alias, "(set)", app);
    text_row(vl, edit_vals[1], "User", p->target.user, "(set)", app);
    text_row(vl, edit_vals[2], "Host", p->target.host, "(set)", app); /* whole: no marquee */
    char port[8];
    snprintf(port, sizeof(port), "%u", (unsigned)p->target.port);
    text_row(vl, edit_vals[3], "Port", port, "22", app);
    VariableItem* it;

    it = variable_item_list_add(vl, "Auth", auth_count(app), edit_auth_changed, app);
    variable_item_set_current_value_index(it, auth_index(app));
    auth_show(app, it);
    if(p->auth == ProfileAuthPassword) {
        it = variable_item_list_add(vl, "Save Password", 2, edit_remember_changed, app);
        variable_item_set_current_value_index(it, p->remember);
        variable_item_set_current_value_text(it, yesno[p->remember]);
        edit_rows[row++] = EditRemember;
    }
    for(uint8_t e = EditTransport; e <= EditSave; e++)
        edit_rows[row++] = e;
    it = variable_item_list_add(vl, "Transport", 3, edit_transport_changed, app);
    variable_item_set_current_value_index(it, p->transport);
    variable_item_set_current_value_text(it, transport_names[p->transport]);
    it = variable_item_list_add(vl, "Host Key", 3, edit_policy_changed, app);
    variable_item_set_current_value_index(it, p->policy);
    variable_item_set_current_value_text(it, policy_names[p->policy]);

    char cmds[8]; /* opens a list, like Scripts */
    snprintf(cmds, sizeof(cmds), "%u", p->cmd_count);
    text_row(vl, edit_vals[4], "Commands", cmds, "0", app);
    it = variable_item_list_add(vl, "Trusted Keys", 1, NULL, app);
    variable_item_set_current_value_text(it, ">");
    variable_item_list_add(vl, "Save", 1, NULL, app);
    if(app->profile_saved) {
        variable_item_list_add(vl, "Delete", 1, NULL, app);
        edit_rows[row++] = EditDelete;
    }
    edit_row_count = row;

    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneHostEdit);
    for(uint8_t r = 0; r < row; r++)
        if(edit_rows[r] == sel) variable_item_list_set_selected_item(vl, r);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
}

/* Validates and writes the host; `quiet`: no "Saved" screen (Connect saves first). */
static bool edit_save(SshRunnerApp* app, bool quiet) {
    HostProfile* p = &app->profile;
    const char* err = NULL;
    if(!alias_valid(p->alias))
        err = "Set an alias first";
    else if(!user_valid(p->target.user))
        err = "Set a user first";
    else if(!host_valid(p->target.host))
        err = "Set a host first";
    else if(strcmp(p->alias, app->profile_orig_alias) != 0 && store_profile_exists(app, p->alias))
        err = "Alias already exists";
    if(err) {
        app_notify_error(app);
        app_show_error(app, "Cannot Save", err, 0);
        return false;
    }
    bool renamed = app->profile_saved && strcmp(p->alias, app->profile_orig_alias) != 0;
    if(!store_save_profile(app, p)) {
        app_notify_error(app);
        app_show_error(app, "Cannot Save", "SD card write failed", 0);
        return false;
    }
    if(renamed) {
        /* Host keys and secrets are keyed by alias on the ESP32. */
        store_delete_profile(app, app->profile_orig_alias);
        app->backend.ops->secret_forget(
            app->backend.self, ProtoForgetProfile, app->profile_orig_alias);
        recent_forget_alias(&app->recent, app->profile_orig_alias);
        store_save_recent(app);
    }
    app->profile_saved = true;
    app->profile_dirty = false;
    snprintf(app->profile_orig_alias, sizeof(app->profile_orig_alias), "%s", p->alias);
    if(quiet) return true;
    app_notify_success(app);
    app_show_message_mono(app, "Saved", "Host settings saved.", p->alias, SshRunnerSceneHosts);
    return true;
}

bool ssh_runner_scene_host_edit_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        if(app->profile_dirty) {
            app->confirm_action = ConfirmDiscardEdit;
            scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
            return true;
        }
        return false;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == EVT_EDIT_REBUILD) {
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneHostEdit, EditAuth);
        ssh_runner_scene_host_edit_on_enter(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom || event.event >= edit_row_count) return false;
    uint32_t item = edit_rows[event.event];
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneHostEdit, item);
    HostProfile* p = &app->profile;
    char port[8];
    switch(item) {
    case EditAlias:
        util_start_input(app, InputAlias, p->alias);
        break;
    case EditUser:
        util_start_input(app, InputUser, p->target.user);
        break;
    case EditHost:
        util_start_input(app, InputHost, p->target.host);
        break;
    case EditPort:
        snprintf(port, sizeof(port), "%u", (unsigned)p->target.port);
        util_start_input(app, InputPort, port);
        break;
    case EditCommands:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneHostCmds);
        break;
    case EditKeys:
        if(!app->profile_saved) {
            app_show_message(app, "Save First", "Trusted keys belong\nto a saved alias", 0);
        } else {
            scene_manager_next_scene(app->scene_manager, SshRunnerSceneHostKeys);
        }
        break;
    case EditSave:
        edit_save(app, false);
        break;
    case EditConnect:
        /* unsaved changes are saved first: the run, its trusted keys and Recent go by alias */
        if(!app->profile_dirty || edit_save(app, true))
            hosts_connect_profile(app, SshRunnerSceneHostEdit);
        break;
    case EditDelete:
        app->confirm_action = ConfirmDeleteHost;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
        break;
    default:
        break;
    }
    return true;
}

void ssh_runner_scene_host_edit_on_exit(void* context) {
    SshRunnerApp* app = context;
    variable_item_list_reset(app->var_list);
    keys_unload(app); /* 1.2 KB the run after a save does not need; on_enter scans again */
}

/* ---------------- Saved commands of a host ---------------- */

#define CMDS_ADD 999u

void ssh_runner_scene_host_cmds_on_enter(void* context) {
    SshRunnerApp* app = context;
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Saved Commands");
    for(uint8_t i = 0; i < app->profile.cmd_count; i++) {
        char label[PROFILE_CMD_MAX + 4];
        snprintf(label, sizeof(label), "$ %s", app->profile.cmds[i]);
        util_submenu_add_ex(sm, label, i, util_submenu_ex_cb, app);
    }
    if(app->profile.cmd_count == 0) {
        /* empty state: text and an Add button, not a list with one highlighted row */
        Widget* w = app->widget;
        widget_reset(w);
        widget_add_string_element(w, 64, 3, AlignCenter, AlignTop, FontPrimary, "Saved Commands");
        widget_add_string_multiline_element(
            w,
            64,
            30,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "No commands yet.\nThey show first in the\nrun menu of this host.");
        widget_add_button_element(w, GuiButtonTypeCenter, "Add", util_widget_button_cb, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return;
    }
    if(app->profile.cmd_count < PROFILE_MAX_CMDS)
        util_submenu_add(sm, "+ Add Command", CMDS_ADD, util_submenu_cb, app);
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneHostCmds));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_host_cmds_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EVT_DIALOG_CENTER)
        event.event = CMDS_ADD; /* empty state: the Add button */
    if(event.type != SceneManagerEventTypeCustom || (event.event & ~EVT_LONG) >= EvtBase)
        return false;
    uint32_t idx = event.event & ~EVT_LONG;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneHostCmds, idx);
    if(idx == CMDS_ADD) {
        app->cmd_edit_index = -1;
        util_start_input(app, InputHostCmd, NULL);
    } else if(idx < app->profile.cmd_count) {
        /* OK and long-press: the whole command, with Edit and Delete */
        app->cmd_index = (uint8_t)idx;
        app->cmd_global = false;
        app->cmd_in_session = false;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneCmdDetail);
    }
    return true;
}

void ssh_runner_scene_host_cmds_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
    widget_reset(app->widget);
}

/* ---------------- Trusted host keys ---------------- */

static void keys_show(SshRunnerApp* app) {
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Trusted Keys");
    if(app->hostkeys.count == 0) {
        util_submenu_add(sm, "No trusted keys yet", 900, util_submenu_cb, app);
    }
    for(uint8_t i = 0; i < app->hostkeys.count; i++) {
        char label[PROTO_STR_FP + PROTO_STR_LABEL + 4];
        const ProtoHostkey* k = &app->hostkeys.keys[i];
        /* drop the "SHA256:" prefix; the row scrolls when selected */
        const char* fp = strncmp(k->fp, "SHA256:", 7) == 0 ? k->fp + 7 : k->fp;
        if(k->label[0])
            snprintf(label, sizeof(label), "%s %s", k->label, fp);
        else
            snprintf(label, sizeof(label), "%s", fp);
        util_submenu_add(sm, label, i, util_submenu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

void ssh_runner_scene_host_keys_on_enter(void* context) {
    SshRunnerApp* app = context;
    app->hostkeys_loaded = false;
    if(app->backend.ops->hostkey_list(app->backend.self, app->profile.alias)) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading);
    } else {
        app_notify_error(app);
        app_show_error(app, "ESP32 Busy", "Try again in a moment", 0);
    }
}

bool ssh_runner_scene_host_keys_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EvtChanged) {
        uint32_t chg = app_take_changes(app);
        if(chg & APP_CHG_HOSTKEYS) keys_show(app);
        if(chg & APP_CHG_OPDONE)
            app->backend.ops->hostkey_list(app->backend.self, app->profile.alias);
        if((chg & APP_CHG_ERROR) && !app->hostkeys_loaded) {
            app_notify_error(app);
            app_show_error(app, "No Answer", "ESP32 not responding", SshRunnerSceneHostEdit);
        }
        return true;
    }
    if(event.event < PROTO_MAX_HOSTKEYS && event.event < app->hostkeys.count) {
        app->confirm_action = ConfirmDeleteHostkey;
        app->confirm_index = event.event;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
    }
    return true;
}

void ssh_runner_scene_host_keys_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}
