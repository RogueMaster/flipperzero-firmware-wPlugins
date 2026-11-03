/* Recent connections and their context menu. SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"

#define RECENT_EMPTY 900u

void hosts_connect_profile(SshRunnerApp* app, uint32_t return_scene);

/* Load the profile behind a recent entry: the saved alias if it still exists,
 * otherwise an unsaved Quick Connect target with the remembered transport. */
static void recent_select(SshRunnerApp* app, uint8_t idx) {
    const RecentEntry* e = &app->recent.items[idx];
    if(e->alias[0] && store_load_profile(app, e->alias, &app->profile)) {
        app->profile_saved = true;
        snprintf(app->profile_orig_alias, sizeof(app->profile_orig_alias), "%s", e->alias);
    } else {
        profile_init(&app->profile);
        app->profile.target = e->target;
        app->profile.transport = e->transport;
        if(app->settings.default_key[0] && store_key_exists(app, app->settings.default_key))
            app->profile.auth = ProfileAuthDefault;
        app->profile_saved = false;
        app->profile_orig_alias[0] = '\0';
    }
    app->profile_dirty = false;
}

void ssh_runner_scene_recent_on_enter(void* context) {
    SshRunnerApp* app = context;
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Recent");
    if(app->recent.count == 0) {
        /* an empty state is text, not a selectable row */
        Widget* w = app->widget;
        widget_reset(w);
        widget_add_string_element(w, 64, 3, AlignCenter, AlignTop, FontPrimary, "Recent");
        widget_add_string_multiline_element(
            w,
            64,
            30,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "No recent connections.\nQuick Connect adds\nthem here.");
        widget_add_button_element(w, GuiButtonTypeCenter, "Connect", util_widget_button_cb, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return;
    }
    for(uint8_t i = 0; i < app->recent.count; i++) {
        char label[PROFILE_USER_MAX + PROFILE_HOST_MAX + 24];
        char t[PROFILE_USER_MAX + PROFILE_HOST_MAX + 12];
        const RecentEntry* e = &app->recent.items[i];
        if(e->alias[0]) {
            target_format(&e->target, t, sizeof(t));
            snprintf(label, sizeof(label), "%s: %s", e->alias, t);
        } else {
            recent_label(e, label, sizeof(label));
        }
        util_submenu_add_ex(sm, label, i, util_submenu_ex_cb, app);
    }
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneRecent));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_recent_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EVT_DIALOG_CENTER) {
        util_start_input(app, InputQuickConnect, NULL); /* empty state: the Connect button */
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom || (event.event & ~EVT_LONG) >= EvtBase)
        return false;
    uint32_t idx = event.event & ~EVT_LONG;
    if(idx >= app->recent.count) return true;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneRecent, idx);
    app->confirm_index = idx;
    recent_select(app, (uint8_t)idx);
    if(event.event & EVT_LONG)
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneRecentMenu);
    else
        hosts_connect_profile(app, SshRunnerSceneRecent);
    return true;
}

void ssh_runner_scene_recent_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
    widget_reset(app->widget);
}

enum {
    RecentMenuConnect,
    RecentMenuSave,
    RecentMenuRemove,
};

void ssh_runner_scene_recent_menu_on_enter(void* context) {
    SshRunnerApp* app = context;
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    /* stock header: the alias, or a plain word (no user@host in a bold proportional font) */
    static char header[PROFILE_ALIAS_MAX + 1];
    const RecentEntry* e = &app->recent.items[app->confirm_index];
    snprintf(header, sizeof(header), "%s", e->alias[0] ? e->alias : "Recent");
    submenu_set_header(sm, header);
    util_submenu_add(sm, "Connect", RecentMenuConnect, util_submenu_cb, app);
    if(!app->profile_saved)
        util_submenu_add(sm, "Save as Alias", RecentMenuSave, util_submenu_cb, app);
    util_submenu_add(sm, "Delete", RecentMenuRemove, util_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_recent_menu_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    switch(event.event) {
    case RecentMenuConnect:
        hosts_connect_profile(app, SshRunnerSceneRecent);
        break;
    case RecentMenuSave: {
        char suggestion[PROFILE_ALIAS_MAX + 1];
        alias_suggest(app->profile.target.host, suggestion, sizeof(suggestion));
        app->return_scene = SshRunnerSceneRecent;
        util_start_input(app, InputSaveAlias, suggestion);
        break;
    }
    case RecentMenuRemove:
        app->confirm_action = ConfirmRemoveRecent;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
        break;
    }
    return true;
}

void ssh_runner_scene_recent_menu_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}
