/* Shared helpers, Confirm and Message scenes. SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"
#include <ssh_runner_icons.h>

void util_submenu_cb(void* context, uint32_t index) {
    SshRunnerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void util_submenu_ex_cb(void* context, InputType input_type, uint32_t index) {
    SshRunnerApp* app = context;
    if(input_type == InputTypeShort)
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
    else if(input_type == InputTypeLong)
        view_dispatcher_send_custom_event(app->view_dispatcher, index | EVT_LONG);
}

void util_var_list_enter_cb(void* context, uint32_t index) {
    SshRunnerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void util_text_view_cb(void* context, InputKey key) {
    SshRunnerApp* app = context;
    uint32_t evt = key == InputKeyLeft  ? EVT_DIALOG_LEFT :
                   key == InputKeyRight ? EVT_DIALOG_RIGHT :
                   key == InputKeyOk    ? EVT_DIALOG_CENTER :
                                          0;
    if(evt) view_dispatcher_send_custom_event(app->view_dispatcher, evt);
}

void util_widget_button_cb(GuiButtonType result, InputType type, void* context) {
    SshRunnerApp* app = context;
    if(type != InputTypeShort) return;
    uint32_t evt = result == GuiButtonTypeLeft  ? EVT_DIALOG_LEFT :
                   result == GuiButtonTypeRight ? EVT_DIALOG_RIGHT :
                                                  EVT_DIALOG_CENTER;
    view_dispatcher_send_custom_event(app->view_dispatcher, evt);
}

void util_popup_done_cb(void* context) {
    SshRunnerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EvtPopupDone);
}

void util_start_input(SshRunnerApp* app, InputTarget target, const char* initial) {
    app->input_target = target;
    snprintf(app->input, sizeof(app->input), "%s", initial ? initial : "");
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneInput);
}

void util_profile_label(const HostProfile* p, char* buf, size_t size) {
    if(p->alias[0])
        snprintf(buf, size, "%s", p->alias);
    else
        target_format(&p->target, buf, size);
}

void util_board_version(const char* version, char* buf, size_t size) {
    if(!strncmp(version, "fes ", 4)) version += 4; /* the firmware's own short name */
    snprintf(buf, size, "ESP32 fw %s", version);
}

void util_fit_value(char* s, size_t size) {
#ifdef FW_ORIGIN_Official
    /* OFW 1.5.1 draws the whole value centred over the arrows and the label: keep the tail
     * that tells values apart ("...8.1.10") */
    fontw_fit_tail(s, size, VALUE_PX);
#else
    /* Unleashed: the stock list clips the value to 37 px and marquee-scrolls the selected
     * row (elements_scrollable_text_line_str); pre-cutting it would defeat that */
    UNUSED(s);
    UNUSED(size);
#endif
}

/* Pads the gap after `label` with spaces (FontSecondary: 2 px each) until "label value"
 * reaches `right_px`. */
static void flush_pad(char* buf, size_t size, size_t at, uint16_t right_px) {
    while(fontw_secondary(buf) + 2 <= right_px && strlen(buf) + 2 <= size) {
        memmove(buf + at + 1, buf + at, strlen(buf + at) + 1);
        buf[at] = ' ';
    }
}

bool util_label_flush(char* buf, size_t size, const char* label, const char* value) {
    snprintf(buf, size, "%s %s", label, value);
    if(!value[0] || fontw_secondary(buf) > LABEL_VALUE_PX) return false;
    /* the value flush right, ending where the stock rows' '>' does, so it lines up with the
     * value column of the rows around it */
    flush_pad(buf, size, strlen(label), LABEL_VALUE_RIGHT_PX);
    return true;
}

void util_submenu_flush(char* buf, size_t size, const char* label, const char* value) {
    char v[48];
    snprintf(v, sizeof(v), "%s", value);
    /* a long value (an SSID) gives way to the label: cut with the stock ellipsis */
    uint16_t room = SUBMENU_LABEL_PX - fontw_secondary(label) - SUBMENU_GAP_PX;
    fontw_fit_end(v, sizeof(v), room);
    snprintf(buf, size, "%s %s", label, v);
    flush_pad(buf, size, strlen(label), SUBMENU_LABEL_PX);
}

char util_scratch[UTIL_SCRATCH_SIZE];

VariableItem* util_label_value(
    VariableItemList* vl,
    char* buf,
    size_t size,
    const char* label,
    const char* value,
    void* ctx) {
    if(util_label_flush(buf, size, label, value)) {
        VariableItem* it = variable_item_list_add(vl, buf, 1, NULL, ctx);
        variable_item_set_current_value_text(it, "");
        return it;
    }
    VariableItem* it = variable_item_list_add(vl, label, 1, NULL, ctx);
    snprintf(buf, size, "%s", value);
    util_fit_value(buf, size);
    variable_item_set_current_value_text(it, buf);
    return it;
}

/* Long labels: the stock list (OFW) cuts every row; the selected one scrolls instead, as
 * on Unleashed and phones: a pause, a step left every MARQUEE_STEP_MS, a pause at the end,
 * back to the start. Only the long labels are kept, on the heap, until util_submenu_reset. */
#define MARQUEE_MAX     24
#define MARQUEE_HOLD_MS 1000
#define MARQUEE_STEP_MS 300
static struct {
    Submenu* sm;
    struct {
        uint32_t index;
        char* label;
    } item[MARQUEE_MAX];
    uint8_t n;
    int8_t cur; /* the selected item's slot, -1 when its label fits */
    uint32_t sel, since;
    uint16_t shift;
    bool at_end;
} mq = {.cur = -1, .sel = UINT32_MAX};

static void marquee_show(uint8_t slot, uint16_t shift) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s", mq.item[slot].label + shift);
    fontw_fit_end(buf, sizeof(buf), SUBMENU_LABEL_PX);
    submenu_change_item_label(mq.sm, mq.item[slot].index, buf);
}

static void marquee_keep(Submenu* sm, const char* label, uint32_t index) {
    if(fontw_secondary(label) <= SUBMENU_LABEL_PX || mq.n >= MARQUEE_MAX) return;
    char* copy = strdup(label);
    if(!copy) return;
    mq.sm = sm;
    mq.item[mq.n].index = index;
    mq.item[mq.n].label = copy;
    mq.n++;
}

void util_submenu_reset(Submenu* sm) {
    for(uint8_t i = 0; i < mq.n; i++)
        free(mq.item[i].label);
    mq.n = 0;
    mq.cur = -1;
    mq.sel = UINT32_MAX;
    submenu_reset(sm);
}

void util_submenu_tick(void) {
    if(!mq.n) return;
    uint32_t now = furi_get_tick() * 1000 / furi_kernel_get_tick_frequency();
    uint32_t sel = submenu_get_selected_item(mq.sm);
    if(sel != mq.sel) {
        if(mq.cur >= 0 && mq.shift) marquee_show((uint8_t)mq.cur, 0);
        mq.sel = sel;
        mq.cur = -1;
        for(uint8_t i = 0; i < mq.n; i++)
            if(mq.item[i].index == sel) mq.cur = (int8_t)i;
        mq.since = now;
        mq.shift = 0;
        mq.at_end = false;
        return;
    }
    if(mq.cur < 0) return;
    const char* label = mq.item[mq.cur].label;
    if(mq.at_end) {
        if(now - mq.since < MARQUEE_HOLD_MS) return;
        mq.shift = 0; /* back to the start, and hold there */
        mq.at_end = false;
        mq.since = now;
        marquee_show((uint8_t)mq.cur, 0);
        return;
    }
    if(now - mq.since < (mq.shift ? MARQUEE_STEP_MS : MARQUEE_HOLD_MS)) return;
    mq.shift++;
    mq.since = now;
    mq.at_end = fontw_secondary(label + mq.shift) <= SUBMENU_LABEL_PX;
    marquee_show((uint8_t)mq.cur, mq.shift);
}

void util_submenu_add(
    Submenu* sm,
    const char* label,
    uint32_t index,
    SubmenuItemCallback cb,
    void* ctx) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s", label);
    fontw_fit_end(buf, sizeof(buf), SUBMENU_LABEL_PX);
    submenu_add_item(sm, buf, index, cb, ctx);
    marquee_keep(sm, label, index);
}

void util_submenu_add_ex(
    Submenu* sm,
    const char* label,
    uint32_t index,
    SubmenuItemCallbackEx cb,
    void* ctx) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s", label);
    fontw_fit_end(buf, sizeof(buf), SUBMENU_LABEL_PX);
    submenu_add_item_ex(sm, buf, index, cb, ctx);
    marquee_keep(sm, label, index);
}

void util_truncate(char* s, size_t max) {
    if(max < 4 || strlen(s) <= max) return;
    /* stock ellipsis (elements_string_fit_width): three dots */
    size_t k = max - 3;
    while(k > 0 && s[k - 1] == ' ')
        k--;
    memcpy(s + k, "...", 4);
}

/* ---------------- Confirm ----------------
 * Every destructive action goes through here. Stock layout: Left = Cancel (also
 * Back), Right = the action, no centre button, so a stray OK never confirms. */

void util_text_screen(
    SshRunnerApp* app,
    const char* header,
    bool warn,
    const char* body,
    const char* mono,
    bool mono_first,
    const char* left,
    const char* center,
    const char* right) {
    TextView* tv = app->text_view;
    text_view_reset(tv);
    text_view_set_header(tv, header, warn ? &I_Alert_9x8 : NULL);
    const char* t[2] = {mono_first ? mono : body, mono_first ? body : mono};
    TextViewFont f[2] = {
        mono_first ? TextViewFontMono : TextViewFontBody,
        mono_first ? TextViewFontBody : TextViewFontMono};
    uint8_t block = 0;
    for(uint8_t i = 0; i < 2; i++)
        if(t[i] && t[i][0]) text_view_set_text(tv, block++, t[i], f[i]);
    text_view_set_buttons(tv, left, center, right);
    text_view_set_callback(tv, util_text_view_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewText);
}

static struct {
    char title[40];
    bool title_mono;
    char status[32];
    char text[64];
    const Icon* icon;
    const char* hint;
    uint32_t started;
    uint32_t shown_secs;
    bool spin; /* the icon is UTIL_SPINNER: util_progress_tick() turns it */
    uint8_t frame;
} prog;

const Icon* const util_spinner[UTIL_SPINNER_FRAMES] = {
    &I_Spinner0_13x13,
    &I_Spinner1_13x13,
    &I_Spinner2_13x13,
    &I_Spinner3_13x13,
    &I_Spinner4_13x13,
    &I_Spinner5_13x13,
    &I_Spinner6_13x13,
    &I_Spinner7_13x13,
};

const Icon* const util_busy[UTIL_BUSY_FRAMES] = {
    &I_Busy0_5x8,
    &I_Busy1_5x8,
    &I_Busy2_5x8,
    &I_Busy3_5x8,
    &I_Busy4_5x8,
    &I_Busy5_5x8,
};

static void progress_icon(const Icon* icon) {
    prog.icon = icon;
    prog.spin = icon == UTIL_SPINNER;
    prog.frame = 0;
}

static void progress_draw(SshRunnerApp* app) {
    uint32_t secs = (furi_get_tick() - prog.started) / furi_kernel_get_tick_frequency();
    prog.shown_secs = secs;
    snprintf(
        prog.text,
        sizeof(prog.text),
        "%s %lu s%s%s",
        prog.status,
        (unsigned long)secs,
        prog.hint ? "\n" : "",
        prog.hint ? prog.hint : "");
    progress_view_set(app->progress, prog.title, prog.title_mono, prog.icon, prog.text);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewProgress);
}

void util_progress_start(
    SshRunnerApp* app,
    const char* title,
    bool title_mono,
    const Icon* icon,
    const char* status,
    const char* hint) {
    snprintf(prog.title, sizeof(prog.title), "%s", title);
    prog.title_mono = title_mono || text_is_technical(title);
    snprintf(prog.status, sizeof(prog.status), "%s", status);
    progress_icon(icon);
    prog.hint = hint;
    prog.started = furi_get_tick();
    app->want_ticks = true;
    progress_draw(app);
}

void util_progress_set(SshRunnerApp* app, const Icon* icon, const char* status) {
    if(icon && icon != prog.icon && !(prog.spin && icon == UTIL_SPINNER)) progress_icon(icon);
    if(status) snprintf(prog.status, sizeof(prog.status), "%s", status);
    progress_draw(app);
}

void util_progress_tick(SshRunnerApp* app) {
    uint32_t ms = (furi_get_tick() - prog.started) * 1000 / furi_kernel_get_tick_frequency();
    bool redraw = ms / 1000 != prog.shown_secs;
    if(prog.spin) {
        uint8_t f = (uint8_t)((ms / UTIL_SPINNER_MS) % UTIL_SPINNER_FRAMES);
        if(f != prog.frame) {
            prog.frame = f;
            prog.icon = util_spinner[f];
            redraw = true;
        }
    }
    if(redraw) progress_draw(app);
}

void util_progress_stop(SshRunnerApp* app) {
    app->want_ticks = false;
}

void ssh_runner_scene_confirm_on_enter(void* context) {
    SshRunnerApp* app = context;
    const char* header = "Are You Sure?";
    const char* right = "OK";
    const char* left = "Cancel";
    bool warn = false;
    static char mono[96], body[192];
    mono[0] = body[0] = '\0';
    switch(app->confirm_action) {
    case ConfirmDeleteHost:
        header = "Delete Host?";
        snprintf(mono, sizeof(mono), "%s", app->profile.alias);
        snprintf(body, sizeof(body), "Its saved password goes too.");
        right = "Delete";
        break;
    case ConfirmDeleteCmd:
        header = "Delete Command?";
        snprintf(mono, sizeof(mono), "%s", app->profile.cmds[app->confirm_index]);
        right = "Delete";
        break;
    case ConfirmDeleteHostkey:
        header = "Forget Host Key?";
        snprintf(mono, sizeof(mono), "%s", app->hostkeys.keys[app->confirm_index].fp);
        right = "Forget";
        break;
    case ConfirmReplaceKeys:
        header = "Replace All Keys?";
        snprintf(
            body,
            sizeof(body),
            "Forget every trusted key of %s and keep only the new one.",
            app->profile.alias[0] ? app->profile.alias : "this host");
        right = "Replace";
        break;
    case ConfirmDeleteKey: {
        header = "Delete Key?";
        snprintf(mono, sizeof(mono), "%s", app->key_name);
        char(*users)[PROFILE_ALIAS_MAX + 1] = malloc(2 * (PROFILE_ALIAS_MAX + 1));
        unsigned n = users ? store_key_users(app, app->key_name, users, 2) : 0;
        const char* names[2] = {users ? users[0] : "", users ? users[1] : ""};
        key_users_text(names, n, body, sizeof(body));
        free(users);
        right = "Delete";
        break;
    }
    case ConfirmRememberPassphrase:
        header = "Remember Passphrase?";
        snprintf(mono, sizeof(mono), "%s", app->key_name);
        /* macOS Keychain style, default on; the first time also says where it is kept
         * (NVS is plaintext until "Protect Secrets") */
        snprintf(
            body,
            sizeof(body),
            app->settings.remember_notice_seen ? "Kept on the ESP32; unlocked until it restarts." :
                                                 "Kept on the ESP32, not yet encrypted there.");
        left = "Once";
        right = "Remember";
        break;
    case ConfirmRemoveRecent:
        header = "Delete from Recent?";
        recent_label(&app->recent.items[app->confirm_index], mono, sizeof(mono));
        right = "Delete";
        break;
    case ConfirmDiscardEdit:
        header = "Discard Changes?";
        snprintf(body, sizeof(body), "Unsaved edits to this host will be lost.");
        right = "Discard";
        break;
    case ConfirmProtectSecrets:
        /* an irreversible step: the first page carries the whole decision, details below */
        header = "Protect Secrets?";
        warn = true;
        snprintf(
            body,
            sizeof(body),
            "Cannot be undone.\nBurns a key into the ESP32.\nUpdates still work.\n\n"
            "Encrypts saved passwords, passphrases, WiFi and host keys. The key step was not tested "
            "on real hardware.");
        right = "Protect";
        break;
    case ConfirmRunScript: {
        header = "Run Script?";
        char sz[16];
        format_size(sz, sizeof(sz), app->script_size);
        snprintf(mono, sizeof(mono), "%s", app->script_name);
        snprintf(body, sizeof(body), "%s, runs with %s", sz, app->script_interp);
        right = "Run";
        break;
    }
    }
    util_text_screen(app, header, warn, body, mono, true, left, NULL, right);
}

/* Implemented by the scenes that own each action. */
void confirm_perform(SshRunnerApp* app);
void keys_gen_start(SshRunnerApp* app);
void keys_send_passphrase(SshRunnerApp* app, bool remember);

bool ssh_runner_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(app->confirm_action == ConfirmRememberPassphrase &&
       event.type == SceneManagerEventTypeBack) {
        /* leaving the passphrase prompt abandons the connection */
        memset(app->gen_pass, 0, sizeof(app->gen_pass));
        app_end_session(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EVT_DIALOG_LEFT) {
            if(app->confirm_action == ConfirmRememberPassphrase)
                keys_send_passphrase(app, false); /* Once */
            else
                scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(event.event == EVT_DIALOG_RIGHT) {
            confirm_perform(app);
            return true;
        }
    }
    return false;
}

void ssh_runner_scene_confirm_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}

/* ---------------- Message ---------------- */

void ssh_runner_scene_message_on_enter(void* context) {
    SshRunnerApp* app = context;
    util_text_screen(
        app, app->msg_header, app->msg_warn, app->msg_text, app->msg_mono, true, NULL, "OK", NULL);
}

static void message_leave(SshRunnerApp* app) {
    if(app->msg_next_scene) {
        uint32_t next = app->msg_next_scene;
        app->msg_next_scene = 0;
        if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, next))
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, SshRunnerSceneHome);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool ssh_runner_scene_message_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       (event.event == EvtPopupDone || event.event == EVT_DIALOG_CENTER)) {
        message_leave(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        message_leave(app);
        return true;
    }
    return false;
}

void ssh_runner_scene_message_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}
