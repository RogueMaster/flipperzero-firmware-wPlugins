/* Keyboard scene: every text field in the app goes through here, keyed by
 * app->input_target. SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"

static void input_done_cb(void* context) {
    SshRunnerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EvtInputDone);
}

static bool validate(const char* text, char* error, size_t size, void* context) {
    SshRunnerApp* app = context;
    switch(app->input_target) {
    case InputQuickConnect: {
        SshTarget t;
        TargetError e = target_parse(text, &t);
        if(e != TargetOk) {
            snprintf(
                error, size, "%s\nuser@host[:port]", target_error_str(e)); /* as in the title */
            return false;
        }
        return true;
    }
    case InputAlias:
    case InputSaveAlias:
        if(!alias_valid(text)) {
            snprintf(error, size, "Letters, digits, . _ -\nup to %d chars", PROFILE_ALIAS_MAX);
            return false;
        }
        if(strcmp(text, app->profile_orig_alias) != 0 && store_profile_exists(app, text)) {
            snprintf(error, size, "Alias already exists");
            return false;
        }
        return true;
    case InputUser:
        if(!user_valid(text)) {
            snprintf(error, size, "Invalid user name");
            return false;
        }
        return true;
    case InputHost:
    case InputPingHost:
        if(!host_valid(text)) {
            snprintf(error, size, "Invalid host\nIP or host name");
            return false;
        }
        return true;
    case InputPort: {
        char* end = NULL;
        unsigned long v = strtoul(text, &end, 10);
        if(!text[0] || *end || v == 0 || v > 65535) {
            snprintf(error, size, "Port must be 1-65535");
            return false;
        }
        return true;
    }
    case InputIp:
    case InputMask:
    case InputGateway:
    case InputDns: {
        uint32_t ip;
        if(!ip4_parse(text, &ip)) {
            snprintf(error, size, "Enter an IPv4 address\n192.168.1.50");
            return false;
        }
        return true;
    }
    case InputKeyName:
    case InputKeyRename:
        if(!key_name_valid(text)) {
            snprintf(error, size, "Letters, digits, . _ -\nup to %d chars", KEY_NAME_MAX);
            return false;
        }
        if(store_key_exists(app, text) &&
           !(app->input_target == InputKeyRename && !strcmp(text, app->key_name))) {
            snprintf(error, size, "A key with this\nname exists");
            return false;
        }
        return true;
    case InputSecWord:
        return proto_sec_word_ok(text); /* no bubble: OK does nothing until it matches */
    case InputKeyPass2:
        if(strcmp(text, app->gen_pass) != 0) {
            snprintf(error, size, "Not the same\npassphrase");
            return false;
        }
        return true;
    case InputCommand:
    case InputHostCmd:
        if(strlen(text) > PROFILE_CMD_MAX) {
            snprintf(error, size, "Too long (max %d)", PROFILE_CMD_MAX);
            return false;
        }
        return true;
    default:
        return true;
    }
}

/* Each field's limit is what its destination holds, so nothing typed is ever cut later. */
size_t input_max_length(InputTarget t) {
    switch(t) {
    case InputQuickConnect:
        return PROFILE_USER_MAX + 1 + PROFILE_HOST_MAX + 6; /* user@host:port */
    case InputPassword:
        return PROTO_STR_PASSWORD;
    case InputPassphrase:
    case InputKeyPass1:
    case InputKeyPass2:
        return PROTO_STR_PASSPHRASE;
    case InputWifiPassword:
        return PROTO_STR_WIFI_PASS;
    case InputKeyName:
    case InputKeyRename:
        return KEY_NAME_MAX;
    case InputSecWord:
        return PROTO_STR_SEC_WORD;
    case InputCommand:
    case InputHostCmd:
        return PROFILE_CMD_MAX;
    case InputAlias:
    case InputSaveAlias:
        return PROFILE_ALIAS_MAX;
    case InputUser:
        return PROFILE_USER_MAX;
    case InputHost:
    case InputPingHost:
        return PROFILE_HOST_MAX;
    case InputPort:
        return 5;
    case InputIp:
    case InputMask:
    case InputGateway:
    case InputDns:
        return 15; /* 255.255.255.255 */
    }
    return 0;
}

void ssh_runner_scene_input_on_enter(void* context) {
    SshRunnerApp* app = context;
    SshKbd* k = app->kbd;
    ssh_kbd_reset(k);
    const char* header = "";
    bool clear_default = true;
    size_t min_len = 1;
    const char* hint = NULL;
    char hdr[48]; /* a header built here: the keyboard copies it */
    switch(app->input_target) {
    case InputQuickConnect:
        /* plain words; an example in the empty field (a dimmed format mask reads as garbage) */
        header = "Quick Connect";
        hint = "user@host";
        break;
    case InputPassword: {
        /* say which host the password is for (stock header: one line, FontSecondary) */
        /* plain words only (symbols look odd in a proportional title): the alias, else the user */
        /* the alias names the target; without one, just "Password" (the host is on the
         * screens before and after) */
        const char* word = app->session->auth_retry ? "Retry Password" : "Password";
        if(app->profile.alias[0])
            snprintf(hdr, sizeof(hdr), "%s for %s", word, app->profile.alias);
        else
            snprintf(hdr, sizeof(hdr), "%s", word);
        header = hdr;
        ssh_kbd_set_mask(k, true);
        clear_default = false;
        break;
    }
    case InputPassphrase: {
        /* the key is named in the title, like the host on the password prompt */
        if(app->session->auth_retry)
            snprintf(hdr, sizeof(hdr), "Wrong passphrase. Try again.");
        else
            snprintf(
                hdr, sizeof(hdr), "Passphrase for %s", app->key_name[0] ? app->key_name : "key");
        header = hdr;
        ssh_kbd_set_mask(k, true);
        clear_default = false;
        break;
    }
    case InputKeyName:
        header = "Key Name";
        break;
    case InputSecWord:
        header = "Type \"confirm\" to continue";
        min_len = 0;
        break;
    case InputKeyRename:
        header = "Rename Key";
        break;
    case InputKeyPass1:
    case InputKeyPass2: {
        /* as ssh-keygen: one prompt, empty for no passphrase; a passphrase is typed twice */
        if(app->input_target == InputKeyPass1) {
            snprintf(hdr, sizeof(hdr), "Passphrase, empty for none");
            min_len = 0;
        } else {
            snprintf(hdr, sizeof(hdr), "Again for %s", app->key_name);
        }
        header = hdr;
        ssh_kbd_set_mask(k, true);
        clear_default = false;
        break;
    }
    case InputCommand:
        header = "Command";
        clear_default = false;
        break;
    case InputAlias:
        header = "Alias";
        break;
    case InputUser:
        header = "User";
        break;
    case InputHost:
        header = "Host or IP";
        hint = "192.168.1.10";
        break;
    case InputPort:
        header = "Port";
        break;
    case InputHostCmd:
        header = app->cmd_edit_index < 0 ? "Saved Command" : "Edit Command";
        clear_default = false;
        break;
    case InputSaveAlias:
        header = "Save as Alias";
        break;
    case InputPingHost:
        header = "Ping Host or IP"; /* prefilled (8.8.8.8, a gateway): no example needed */
        break;
    case InputWifiPassword: {
        /* plain words: the SSID is a name and belongs in FontKeyboard, and the network
         * was picked on the screen before */
        header = "WiFi Password";
        ssh_kbd_set_mask(k, true);
        clear_default = false;
        min_len = 8;
        break;
    }
    case InputIp:
        header = "Static IP";
        break;
    case InputMask:
        header = "Netmask";
        break;
    case InputGateway:
        header = "Gateway";
        break;
    case InputDns:
        header = "DNS Server";
        break;
    }
    ssh_kbd_set_header(k, header);
    ssh_kbd_set_hint(k, hint);
    ssh_kbd_set_min_length(k, min_len);
    ssh_kbd_set_validator(k, validate, app);
    ssh_kbd_set_result_callback(
        k, input_done_cb, app, app->input, sizeof(app->input), clear_default);
    ssh_kbd_set_max_length(k, input_max_length(app->input_target));
    /* Ping comes prefilled (8.8.8.8, the gateway), usually sent as it is: OK sends it */
    if(app->input_target == InputPingHost) ssh_kbd_select_save(k);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewKbd);
}

static void set_net_field(SshRunnerApp* app) {
    uint32_t ip = 0;
    ip4_parse(app->input, &ip);
    switch(app->input_target) {
    case InputIp:
        app->net_cfg.ip = ip;
        break;
    case InputMask:
        app->net_cfg.mask = ip;
        break;
    case InputGateway:
        app->net_cfg.gw = ip;
        break;
    default:
        app->net_cfg.dns = ip;
        break;
    }
}

static void start_command(SshRunnerApp* app, const char* cmd) {
    if(!app->backend.ops->exec(app->backend.self, cmd)) {
        app_notify_error(app);
        app_show_error(
            app, "Cannot Run", "The ESP32 is busy or\nthe session ended", SshRunnerSceneRunMenu);
        return;
    }
    snprintf(app->run_title, sizeof(app->run_title), "%s", cmd);
    app->run_is_script = false;
    app->have_output = true;
    session_begin_run(app->session);
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SshRunnerSceneRunMenu);
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneOutput);
}

void input_save_alias(SshRunnerApp* app);
void keys_gen_start(SshRunnerApp* app);
void keys_lock_selected(SshRunnerApp* app);
void cmd_changed(SshRunnerApp* app);

static void input_finish(SshRunnerApp* app) {
    switch(app->input_target) {
    case InputSecWord:
        sec_enable_start(app);
        break;
    case InputQuickConnect: {
        profile_init(&app->profile);
        target_parse(app->input, &app->profile.target);
        /* Quick Connect uses the default key when there is one */
        if(app->settings.default_key[0] && store_key_exists(app, app->settings.default_key))
            app->profile.auth = ProfileAuthDefault;
        app->profile_saved = false;
        app->profile_orig_alias[0] = '\0';
        app->quick_connect = true;
        app->offered_save = false;
        app->return_scene = SshRunnerSceneHome;
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneConnect, 0);
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConnect);
        break;
    }
    case InputPassword: {
        ProtoAuthPasswordMsg m;
        memset(&m, 0, sizeof(m));
        snprintf(m.password, sizeof(m.password), "%s", app->input);
        m.remember = app->profile.remember;
        bool ok = app->backend.ops->auth_password(app->backend.self, &m);
        /* never keep the password around */
        memset(&m, 0, sizeof(m));
        memset(app->input, 0, sizeof(app->input));
        if(ok) app->session->phase = SessionConnecting;
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
    case InputPassphrase:
        /* held until the Remember choice, then wiped (keys_send_passphrase) */
        snprintf(app->gen_pass, sizeof(app->gen_pass), "%s", app->input);
        memset(app->input, 0, sizeof(app->input));
        app->confirm_action = ConfirmRememberPassphrase;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
        break;
    case InputKeyName:
        snprintf(app->key_name, sizeof(app->key_name), "%s", app->input);
        memset(app->gen_pass, 0, sizeof(app->gen_pass));
        memset(app->input, 0, sizeof(app->input));
        util_start_input(app, InputKeyPass1, NULL);
        break;
    case InputKeyPass1:
        if(!app->input[0]) {
            keys_gen_start(app); /* empty: no passphrase */
            break;
        }
        snprintf(app->gen_pass, sizeof(app->gen_pass), "%s", app->input);
        memset(app->input, 0, sizeof(app->input));
        util_start_input(app, InputKeyPass2, NULL);
        break;
    case InputKeyPass2:
        memset(app->input, 0, sizeof(app->input));
        keys_gen_start(app);
        break;
    case InputKeyRename: {
        char old_name[KEY_NAME_MAX + 1];
        snprintf(old_name, sizeof(old_name), "%s", app->key_name);
        if(strcmp(old_name, app->input) != 0) keys_lock_selected(app); /* before the file moves */
        if(!strcmp(old_name, app->input)) {
            scene_manager_previous_scene(app->scene_manager);
        } else if(store_key_rename(app, old_name, app->input)) {
            snprintf(app->key_name, sizeof(app->key_name), "%s", app->input);
            app_show_message_mono(
                app,
                "Renamed",
                "Hosts that use it were updated.",
                app->key_name,
                SshRunnerSceneKeyCard);
        } else {
            app_notify_error(app);
            app_show_error(app, "Cannot Rename", "SD card error", SshRunnerSceneKeyCard);
        }
        break;
    }
    case InputCommand:
        start_command(app, app->input);
        break;
    case InputAlias:
        snprintf(app->profile.alias, sizeof(app->profile.alias), "%s", app->input);
        app->profile_dirty = true;
        scene_manager_previous_scene(app->scene_manager);
        break;
    case InputUser:
        snprintf(app->profile.target.user, sizeof(app->profile.target.user), "%s", app->input);
        app->profile_dirty = true;
        scene_manager_previous_scene(app->scene_manager);
        break;
    case InputHost:
        snprintf(app->profile.target.host, sizeof(app->profile.target.host), "%s", app->input);
        app->profile_dirty = true;
        scene_manager_previous_scene(app->scene_manager);
        break;
    case InputPort:
        app->profile.target.port = (uint16_t)strtoul(app->input, NULL, 10);
        app->profile_dirty = true;
        scene_manager_previous_scene(app->scene_manager);
        break;
    case InputHostCmd: {
        bool changed =
            app->cmd_edit_index < 0 ?
                profile_add_cmd(&app->profile, app->input) :
                profile_set_cmd(&app->profile, (uint8_t)app->cmd_edit_index, app->input);
        if(changed) cmd_changed(app);
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
    case InputSaveAlias:
        input_save_alias(app);
        break;
    case InputPingHost:
        scene_manager_next_scene(app->scene_manager, SshRunnerScenePing);
        break;
    case InputWifiPassword: {
        ProtoWifiJoin j;
        memset(&j, 0, sizeof(j));
        app->join_known = false;
        snprintf(j.ssid, sizeof(j.ssid), "%s", app->wifi_ssid);
        snprintf(j.password, sizeof(j.password), "%s", app->input);
        bool ok = app->backend.ops->wifi_join(app->backend.self, &j);
        memset(&j, 0, sizeof(j));
        memset(app->input, 0, sizeof(app->input));
        /* WifiScan scene state 2 = joining */
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneWifiScan, ok ? 2 : 1);
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
    case InputIp:
    case InputMask:
    case InputGateway:
    case InputDns:
        set_net_field(app);
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
}

bool ssh_runner_scene_input_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EvtInputDone) {
        input_finish(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        if(app->input_target == InputPassword || app->input_target == InputPassphrase) {
            /* Leaving the password prompt abandons the connection. */
            memset(app->input, 0, sizeof(app->input));
            app_end_session(app);
            return true;
        }
        if(app->input_target == InputWifiPassword || app->input_target == InputKeyPass1 ||
           app->input_target == InputKeyPass2) {
            memset(app->input, 0, sizeof(app->input));
            memset(app->gen_pass, 0, sizeof(app->gen_pass));
        }
        return false;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == EvtChanged) {
        /* A session that dies while the password prompt is open ends here too. */
        if((app->input_target == InputPassword || app->input_target == InputPassphrase) &&
           (app->session->phase == SessionFailed || app->session->phase == SessionClosed)) {
            memset(app->input, 0, sizeof(app->input));
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }
    return false;
}

void ssh_runner_scene_input_on_exit(void* context) {
    SshRunnerApp* app = context;
    ssh_kbd_reset(app->kbd);
}
