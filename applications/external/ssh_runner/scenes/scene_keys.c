/* SSH keys as files in keys/: list, key card, actions, generate, the jobs that
 * write keys coming from the ESP32, and the confirm-action dispatcher.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"
#include <ssh_runner_icons.h>

/* ---------------- key jobs (backend events, any scene) ---------------- */

static void job_fail(SshRunnerApp* app, const char* msg) {
    app->key_job_failed = true;
    snprintf(app->key_job_msg, sizeof(app->key_job_msg), "%s", msg);
}

static void key_in_free(SshRunnerApp* app) {
    if(app->key_in) memset(app->key_in, 0, app->key_in_len);
    free(app->key_in);
    app->key_in = NULL;
    app->key_in_len = 0;
}

void keys_on_blob(SshRunnerApp* app, const BackendKeyBlob* b) {
    if(app->key_job != KeyJobGenerate) return;
    key_in_free(app);
    app->key_in = malloc(b->len);
    memcpy(app->key_in, b->data, b->len);
    app->key_in_len = b->len;
    app->key_blob_crc = b->crc32;
}

void keys_on_pub(SshRunnerApp* app, const BackendKeyPub* k) {
    if(app->key_job == KeyJobNone) return;
    free(app->key_in_line);
    app->key_in_line = malloc(k->len + 1u);
    memcpy(app->key_in_line, k->line, k->len);
    app->key_in_line[k->len] = '\0';
    app->key_in_pub = *k;
    app->key_in_pub.line = app->key_in_line;
    app->key_in_pub_due = true;
}

static void key_blob_store(SshRunnerApp* app) {
    app->key_blob_ok =
        store_key_write(app, app->key_name, app->key_in, app->key_in_len, app->key_blob_crc);
    if(!app->key_blob_ok) job_fail(app, "SD card write failed");
    key_in_free(app);
}

void keys_load(SshRunnerApp* app) {
    if(!app->keys) app->keys = malloc(sizeof(KeyList));
    store_keys_scan(app, app->keys);
}

void keys_unload(SshRunnerApp* app) {
    free(app->keys);
    app->keys = NULL;
}

static void key_pub_store(SshRunnerApp* app, const BackendKeyPub* k) {
    KeyJob job = app->key_job;
    if(job == KeyJobNone) return;
    if(k->status) {
        job_fail(app, k->line && k->len ? k->line : "Key operation failed");
        app->key_job = job == KeyJobConnectPub ? KeyJobNone : job;
        return;
    }
    snprintf(app->key_fp, sizeof(app->key_fp), "%s", k->fp);
    if(job == KeyJobConnectPub) {
        /* a key that came without .pub gets one now (shown and exported from then on) */
        if(!store_key_exists(app, app->key_name)) return;
        store_key_write_pub(app, app->key_name, k->line);
        app->key_job = KeyJobNone;
        return;
    }
    if(!app->key_blob_ok) return; /* job_fail already set */
    store_key_write_pub(app, app->key_name, k->line);
    if(!app->settings.default_key[0]) {
        /* the first key becomes the default */
        snprintf(
            app->settings.default_key, sizeof(app->settings.default_key), "%s", app->key_name);
        store_save_settings(app);
    }
    keys_load(app);
}

uint32_t keys_process(SshRunnerApp* app) {
    uint32_t chg = 0;
    if(app->key_in) key_blob_store(app);
    if(app->key_in_pub_due) {
        app->key_in_pub_due = false;
        key_pub_store(app, &app->key_in_pub);
        free(app->key_in_line);
        app->key_in_line = NULL;
        chg |= APP_CHG_KEY;
    }
    return chg;
}

const KeyEntry* keys_for_profile(SshRunnerApp* app, const HostProfile* p, bool* missing) {
    *missing = false;
    const char* name = profile_key_name(p, app->settings.default_key);
    if(p->auth == ProfileAuthPassword) return NULL;
    keys_load(app);
    int i = name && app->keys ? keylist_find(app->keys, name) : -1;
    if(i < 0) {
        *missing = true;
        return NULL;
    }
    return &app->keys->e[i];
}

static const KeyEntry* selected_key(SshRunnerApp* app) {
    int i = app->keys ? keylist_find(app->keys, app->key_name) : -1;
    return i < 0 ? NULL : &app->keys->e[i];
}

/* Key id the ESP32 remembers a passphrase under: the fingerprint, else the file hash. */
static void key_id(SshRunnerApp* app, const KeyEntry* k, char* out, size_t size) {
    if(k->pub_known)
        snprintf(out, size, "%s", k->fp);
    else if(!store_key_file_id(app, k->name, out, size))
        out[0] = '\0';
}

/* The passphrase typed at connect, with the Remember choice; wiped right after. */
void keys_send_passphrase(SshRunnerApp* app, bool remember) {
    ProtoKeyPassphrase m;
    memset(&m, 0, sizeof(m));
    snprintf(m.passphrase, sizeof(m.passphrase), "%s", app->gen_pass);
    m.remember = remember;
    bool ok = app->backend.ops->key_passphrase(app->backend.self, &m);
    memset(&m, 0, sizeof(m));
    memset(app->gen_pass, 0, sizeof(app->gen_pass));
    if(remember && !app->settings.remember_notice_seen) {
        app->settings.remember_notice_seen = true;
        store_save_settings(app);
    }
    if(ok) app->session->phase = SessionConnecting;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SshRunnerSceneConnect);
}

/* Rename: the ESP32 drops its unlocked copy (like Forget, without the passphrase). */
void keys_lock_selected(SshRunnerApp* app) {
    const KeyEntry* k = selected_key(app);
    char id[PROTO_STR_KEYID + 1];
    if(!k || !k->encrypted) return;
    key_id(app, k, id, sizeof(id));
    if(id[0]) app->backend.ops->secret_forget(app->backend.self, ProtoForgetUnlocked, id);
}

/* ---------------- Keys list ---------------- */

#define KEYS_GENERATE 0u

void ssh_runner_scene_keys_on_enter(void* context) {
    SshRunnerApp* app = context;
    keys_load(app);
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Keys");
    util_submenu_add(sm, "Generate Key", KEYS_GENERATE, util_submenu_cb, app);
    if(app->keys)
        for(uint8_t i = 0; i < app->keys->count; i++) {
            char row[KEY_NAME_MAX + KEYFMT_LABEL_SIZE + 4];
            const KeyEntry* e = &app->keys->e[i];
            key_row_label(e, strcmp(e->name, app->settings.default_key) == 0, row, sizeof(row));
            util_submenu_add(sm, row, i + 1u, util_submenu_cb, app);
        }
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneKeys));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_keys_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneKeys, event.event);
    if(event.event == KEYS_GENERATE) {
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneKeyGenType);
        return true;
    }
    uint32_t i = event.event - 1u;
    if(!app->keys || i >= app->keys->count) return true;
    snprintf(app->key_name, sizeof(app->key_name), "%s", app->keys->e[i].name);
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneKeyCard);
    return true;
}

void ssh_runner_scene_keys_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}

/* ---------------- Key card ---------------- */

#define card_text util_scratch

void ssh_runner_scene_key_card_on_enter(void* context) {
    SshRunnerApp* app = context;
    keys_load(app);
    const KeyEntry* k = selected_key(app);
    if(!k) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SshRunnerSceneKeys);
        return;
    }
    /* the card reads like the host-key screen: what it is, then the fingerprint in
     * FontKeyboard; Default / Encrypted as plain words */
    TextView* tv = app->text_view;
    text_view_reset(tv);
    text_view_set_header(tv, k->name, k->problem ? &I_Alert_9x8 : NULL);
    size_t n = 0;
    if(k->problem) {
        n += snprintf(card_text + n, sizeof(card_text) - n, "%s", k->problem);
    } else {
        char label[KEYFMT_LABEL_SIZE];
        key_entry_label(k, label);
        /* type, then each flag as a word of its own */
        bool dflt = strcmp(k->name, app->settings.default_key) == 0;
        n += snprintf(card_text + n, sizeof(card_text) - n, "%s", label);
        if(dflt || k->encrypted)
            n += snprintf(
                card_text + n,
                sizeof(card_text) - n,
                "\n%s%s%s",
                dflt ? "Default Key" : "",
                dflt && k->encrypted ? ", " : "",
                k->encrypted ? (dflt ? "encrypted" : "Encrypted") : "");
        if(!k->pub_known)
            n += snprintf(card_text + n, sizeof(card_text) - n, "\nFingerprint after first use");
    }
    text_view_set_text(tv, 0, card_text, TextViewFontBody);
    if(k->pub_known) text_view_set_text(tv, 1, k->fp, TextViewFontMono);
    text_view_set_buttons(tv, NULL, NULL, "Actions");
    text_view_set_callback(tv, util_text_view_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewText);
}

bool ssh_runner_scene_key_card_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EVT_DIALOG_RIGHT) {
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneKeyActions, 0);
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneKeyActions);
        return true;
    }
    return false;
}

void ssh_runner_scene_key_card_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}

/* ---------------- Key actions ---------------- */

enum {
    KeyActShow,
    KeyActExport,
    KeyActDefault,
    KeyActForget,
    KeyActRename,
    KeyActDelete,
};

void ssh_runner_scene_key_actions_on_enter(void* context) {
    SshRunnerApp* app = context;
    const KeyEntry* k = selected_key(app);
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(
        sm, "Key"); /* the name is on the card before (no file names in a stock header) */
    if(k && !k->problem) {
        util_submenu_add(sm, "Show Public Key", KeyActShow, util_submenu_cb, app);
        if(!k->has_pub && k->pub_known)
            util_submenu_add(sm, "Export .pub", KeyActExport, util_submenu_cb, app);
        if(strcmp(k->name, app->settings.default_key) != 0)
            util_submenu_add(sm, "Set as Default", KeyActDefault, util_submenu_cb, app);
        if(k->encrypted)
            util_submenu_add(sm, "Forget Passphrase", KeyActForget, util_submenu_cb, app);
    }
    util_submenu_add(sm, "Rename", KeyActRename, util_submenu_cb, app);
    util_submenu_add(sm, "Delete", KeyActDelete, util_submenu_cb, app);
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneKeyActions));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_key_actions_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneKeyActions, event.event);
    const KeyEntry* k = selected_key(app);
    if(!k) return true;
    switch(event.event) {
    case KeyActShow:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneKeyShow);
        break;
    case KeyActExport: {
        file_buf_acquire(app);
        char* line = KEY_LINE_BUF(app);
        bool ok = store_key_derive_pub(app, k->name, line, KEY_PUB_LINE_MAX + 1) &&
                  store_key_write_pub(app, k->name, line);
        file_buf_release(app);
        char path[48];
        snprintf(path, sizeof(path), "keys/%s.pub", k->name);
        if(ok) {
            app_notify_success(app);
            app_show_message_mono(
                app, "Exported", "Add it to authorized_keys on your hosts.", path, 0);
        } else {
            app_notify_error(app);
            app_show_error(app, "Cannot Export", "SD card write failed", 0);
        }
        break;
    }
    case KeyActDefault:
        snprintf(app->settings.default_key, sizeof(app->settings.default_key), "%s", k->name);
        store_save_settings(app);
        app_notify_success(app);
        app_show_message_mono(
            app,
            "Default Key",
            "Hosts set to Default key use it now.",
            k->name,
            SshRunnerSceneKeyCard);
        break;
    case KeyActForget: {
        char id[PROTO_STR_KEYID + 1];
        key_id(app, k, id, sizeof(id));
        if(id[0] &&
           app->backend.ops->secret_forget(app->backend.self, ProtoForgetPassphrase, id)) {
            app_show_message_mono(
                app, "Passphrase Forgotten", "Asked again on the next connect.", k->name, 0);
        } else {
            app_notify_error(app);
            app_show_error(app, "Cannot Forget", "ESP32 busy or not connected", 0);
        }
        break;
    }
    case KeyActRename:
        util_start_input(app, InputKeyRename, k->name);
        break;
    case KeyActDelete:
        app->confirm_action = ConfirmDeleteKey;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
        break;
    }
    return true;
}

void ssh_runner_scene_key_actions_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}

/* ---------------- Public key ---------------- */

void ssh_runner_scene_key_show_on_enter(void* context) {
    SshRunnerApp* app = context;
    const KeyEntry* k = selected_key(app);
    text_view_reset(app->text_view);
    text_view_set_header(app->text_view, "Public Key", NULL);
    /* the whole key line (scroll), then the fingerprint that identifies it */
    if(k) {
        file_buf_acquire(app);
        char* line = KEY_LINE_BUF(app);
        if(!store_key_read_pub(app, k->name, line, KEY_PUB_LINE_MAX + 1))
            store_key_derive_pub(app, k->name, line, KEY_PUB_LINE_MAX + 1);
        furi_string_printf(app->output_text, "%s\n\n%s", k->name, line);
        file_buf_release(app);
        text_view_set_text(
            app->text_view, 0, furi_string_get_cstr(app->output_text), TextViewFontMono);
        furi_string_printf(app->output_text, "\n%s", k->fp); /* blank line between */
        text_view_set_text(
            app->text_view, 1, furi_string_get_cstr(app->output_text), TextViewFontMono);
        furi_string_reset(app->output_text);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewText);
}

bool ssh_runner_scene_key_show_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ssh_runner_scene_key_show_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}

/* ---------------- Generate: type, name, passphrase, progress ---------------- */

void ssh_runner_scene_key_gen_type_on_enter(void* context) {
    SshRunnerApp* app = context;
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Key Type");
    util_submenu_add(sm, "Ed25519", ProtoKeyGenEd25519, util_submenu_cb, app);
    util_submenu_add(sm, "ECDSA P-256", ProtoKeyGenEcdsaP256, util_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_key_gen_type_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    app->gen_type = (uint8_t)event.event;
    keys_load(app);
    char name[KEY_NAME_MAX + 1] = "id_ed25519";
    if(app->keys)
        key_name_next(
            app->keys,
            app->gen_type == ProtoKeyGenEcdsaP256 ? "id_ecdsa" : "id_ed25519",
            '_',
            2,
            name,
            sizeof(name));
    util_start_input(app, InputKeyName, name);
    return true;
}

void ssh_runner_scene_key_gen_type_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}

/* After the name and the optional passphrase (twice). */
void keys_gen_start(SshRunnerApp* app) {
    ProtoKeyGen g;
    memset(&g, 0, sizeof(g));
    g.key_type = app->gen_type;
    snprintf(g.passphrase, sizeof(g.passphrase), "%s", app->gen_pass);
    memset(app->gen_pass, 0, sizeof(app->gen_pass));
    app->key_job = KeyJobGenerate;
    app->key_blob_ok = false;
    app->key_job_failed = false;
    app->key_fp[0] = '\0';
    bool ok = app->backend.ops->key_gen(app->backend.self, &g);
    memset(&g, 0, sizeof(g));
    if(!ok) {
        app->key_job = KeyJobNone;
        app_notify_error(app);
        app_show_error(app, "Cannot Generate", "ESP32 busy or not connected", SshRunnerSceneKeys);
        return;
    }
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneKeyGen);
}

void ssh_runner_scene_key_gen_on_enter(void* context) {
    SshRunnerApp* app = context;
    /* the shared progress layout; generation cannot be cancelled once the ESP32 runs it */
    util_progress_start(app, app->key_name, true, UTIL_SPINNER, "Generating...", "Please wait");
}

bool ssh_runner_scene_key_gen_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeBack) return true; /* the ESP32 finishes anyway */
    if(event.type != SceneManagerEventTypeCustom || event.event != EvtChanged) return false;
    uint32_t chg = app_take_changes(app);
    util_progress_tick(app);
    bool failed = app->key_job_failed ||
                  ((chg & APP_CHG_ERROR) && app->last_error.request == PROTO_T_KEY_GEN);
    if(failed) {
        app->key_job = KeyJobNone;
        app_notify_error(app);
        app_show_error(
            app,
            "Key not created",
            app->key_job_failed ? app->key_job_msg : app->last_error.msg,
            SshRunnerSceneKeys);
    } else if((chg & APP_CHG_KEY) && app->key_blob_ok) {
        app->key_job = KeyJobNone;
        app_notify_success(app);
        int i = app->keys ? keylist_find(app->keys, app->key_name) : -1;
        if(i >= 0)
            scene_manager_set_scene_state(
                app->scene_manager, SshRunnerSceneKeys, (uint32_t)i + 1u);
        /* the card of the new key, with the list behind it (cursor on it) */
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SshRunnerSceneKeys);
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneKeyCard);
    }
    return true;
}

void ssh_runner_scene_key_gen_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_progress_stop(app);
    progress_view_reset(app->progress);
}

/* ---------------- Confirm actions ---------------- */

void hostkey_decide_replace(SshRunnerApp* app);
void run_confirmed_script(SshRunnerApp* app);

void cmd_changed(SshRunnerApp* app);

static void delete_key(SshRunnerApp* app) {
    const KeyEntry* k = selected_key(app);
    if(!k) return;
    char id[PROTO_STR_KEYID + 1];
    key_id(app, k, id, sizeof(id)); /* before the file is gone */
    char name[KEY_NAME_MAX + 1];
    snprintf(name, sizeof(name), "%s", k->name);
    if(k->encrypted && id[0])
        app->backend.ops->secret_forget(app->backend.self, ProtoForgetPassphrase, id);
    memset(id, 0, sizeof(id));
    unsigned switched = 0;
    uint8_t to = ProfileAuthPassword;
    if(store_key_delete(app, name, &switched, &to)) {
        keys_load(app);
        char said[48]; /* copied by the message scene */
        if(switched) /* the break keeps "Default Key" on one line */
            snprintf(
                said,
                sizeof(said),
                "%u host%s now use%s\n%s.",
                switched,
                switched == 1 ? "" : "s",
                switched == 1 ? "s" : "",
                to == ProfileAuthDefault ? "the Default key" : "a password");
        else
            snprintf(said, sizeof(said), "No host used it.");
        app_show_message_mono(app, "Deleted", said, name, SshRunnerSceneKeys);
    } else {
        app_notify_error(app);
        app_show_error(app, "Cannot Delete", "SD card error", SshRunnerSceneKeys);
    }
}

void confirm_perform(SshRunnerApp* app) {
    switch(app->confirm_action) {
    case ConfirmDeleteHost: {
        char alias[PROFILE_ALIAS_MAX + 1];
        snprintf(alias, sizeof(alias), "%s", app->profile.alias);
        store_delete_profile(app, alias);
        /* drop the remembered password and pinned keys on the ESP32 too */
        app->backend.ops->secret_forget(app->backend.self, ProtoForgetProfile, alias);
        recent_forget_alias(&app->recent, alias);
        store_save_recent(app);
        app_show_message(app, "Deleted", alias, SshRunnerSceneHosts);
        break;
    }
    case ConfirmDeleteCmd:
        if(profile_remove_cmd(&app->profile, (uint8_t)app->confirm_index)) cmd_changed(app);
        /* back to the list the command was in (the detail screen shows it no more) */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager,
            app->cmd_in_session ? SshRunnerSceneRunMenu : SshRunnerSceneHostCmds);
        break;
    case ConfirmDeleteHostkey:
        app->backend.ops->hostkey_delete(
            app->backend.self, app->profile.alias, app->hostkeys.keys[app->confirm_index].fp);
        scene_manager_previous_scene(app->scene_manager);
        break;
    case ConfirmReplaceKeys:
        hostkey_decide_replace(app);
        break;
    case ConfirmDeleteKey:
        delete_key(app);
        break;
    case ConfirmProtectSecrets:
        app->input_target = InputSecWord;
        app->input[0] = '\0';
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneInput);
        break;
    case ConfirmRememberPassphrase:
        keys_send_passphrase(app, true);
        break;
    case ConfirmRemoveRecent:
        recent_remove(&app->recent, (uint8_t)app->confirm_index);
        store_save_recent(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SshRunnerSceneRecent);
        break;
    case ConfirmDiscardEdit:
        app->profile_dirty = false;
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SshRunnerSceneHosts);
        break;
    case ConfirmRunScript:
        run_confirmed_script(app);
        break;
    }
}
