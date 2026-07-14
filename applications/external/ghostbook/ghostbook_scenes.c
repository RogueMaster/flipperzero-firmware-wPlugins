/*
 * GhostBook v0.6.0
 * Secure encrypted contacts for Flipper Zero
 * 
 * Original Author: Digi
 * Created: December 2025
 * License: MIT
 * 
 * If you fork this, keep this header intact.
 * https://github.com/digitard/ghostbook
 */

#include "ghostbook.h"

// ============================================================================
// EVENT TYPES - Values must match custom_cb in ghostbook.c
// ============================================================================

typedef enum {
    EvPasscodeComplete = 1,
    EvView = 2,
    EvEdit = 3,
    EvHandle = 4,
    EvName = 5,
    EvEmail = 6,
    EvDiscord = 7,
    EvSignal = 8,
    EvTelegram = 9,
    EvNotes = 10,
    EvShare = 11,
    EvReceive = 12,
    EvContacts = 13,
    EvFlair = 14,
    EvExport = 15,
    EvAbout = 16,
    EvContactView = 17,
    EvTextDone = 18,
    EvFlairDone = 19,
} GhostEv;

typedef enum {
    MenuView = 0,
    MenuEdit,
    MenuShare,
    MenuReceive,
    MenuContacts,
    MenuExport,
    MenuAbout
} MainIdx;
typedef enum {
    EditHandle = 0,
    EditName,
    EditEmail,
    EditDiscord,
    EditSignal,
    EditTelegram,
    EditNotes,
    EditFlair
} EditIdx;

// ============================================================================
// LOCK SCENE - Passcode entry to unlock
// ============================================================================

void ghost_scene_lock_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneLock;
    app->passcode_pos = 0;
    memset(app->passcode_input, 0, MAX_PASSCODE_LENGTH);
    ghost_passcode_view_set_mode(app, "Enter Code", true);
    view_dispatcher_switch_to_view(app->view, GhostViewPasscode);
}

bool ghost_scene_lock_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;

    if(e.type == SceneManagerEventTypeCustom && e.event == EvPasscodeComplete) {
        if(ghost_auth_verify(app, app->passcode_input)) {
            // Success!
            notification_message(app->notif, &sequence_success);
            ghost_load_profile(app);
            ghost_scene_main_enter(app);
        } else {
            // Wrong passcode
            notification_message(app->notif, &sequence_error);

            if(app->auth.failed_attempts >= app->auth.max_attempts) {
                // WIPE EVERYTHING
                ghost_wipe_all_data(app);
                ghost_scene_wiped_enter(app);
            } else {
                // Show warning and reset
                popup_reset(app->popup);
                popup_set_header(app->popup, "WRONG CODE", 64, 15, AlignCenter, AlignTop);

                char msg[64];
                snprintf(
                    msg,
                    sizeof(msg),
                    "%d attempts remaining\nAll data will be WIPED",
                    app->auth.max_attempts - app->auth.failed_attempts);
                popup_set_text(app->popup, msg, 64, 32, AlignCenter, AlignTop);
                popup_set_timeout(app->popup, 2000);
                popup_enable_timeout(app->popup);
                view_dispatcher_switch_to_view(app->view, GhostViewPopup);

                // After popup, return to lock screen
                furi_delay_ms(2000);
                ghost_passcode_view_reset(app);
                view_dispatcher_switch_to_view(app->view, GhostViewPasscode);
            }
        }
        return true;
    }

    return false;
}

void ghost_scene_lock_exit(void* ctx) {
    GhostApp* app = ctx;
    ghost_passcode_view_reset(app);
}

// ============================================================================
// SECURITY SETUP SCENE - Choose passcode length
// ============================================================================

static void security_cb(void* ctx, uint32_t idx) {
    GhostApp* app = ctx;
    app->setup_length = 6 + idx; // 0=6, 1=7, 2=8, 3=9, 4=10
    ghost_scene_wipe_setup_enter(app); // Go to wipe threshold selection
}

void ghost_scene_security_setup_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneSecuritySetup;
    app->setup_length = DEFAULT_PASSCODE_LENGTH;
    app->setup_max_attempts = DEFAULT_MAX_ATTEMPTS;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Security Level");
    submenu_add_item(app->submenu, "Standard   (6 btn) ~min", 0, security_cb, app);
    submenu_add_item(app->submenu, "Enhanced   (7 btn) ~hour", 1, security_cb, app);
    submenu_add_item(app->submenu, "Enhanced+  (8 btn) ~days", 2, security_cb, app);
    submenu_add_item(app->submenu, "Paranoid   (9 btn) ~weeks", 3, security_cb, app);
    submenu_add_item(app->submenu, "Paranoid+ (10 btn) ~months", 4, security_cb, app);
    view_dispatcher_switch_to_view(app->view, GhostViewSubmenu);
}

bool ghost_scene_security_setup_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view);
        return true;
    }
    return false;
}

void ghost_scene_security_setup_exit(void* ctx) {
    GhostApp* app = ctx;
    submenu_reset(app->submenu);
}

// ============================================================================
// WIPE SETUP SCENE - Choose wipe threshold (attempts before data destruction)
// ============================================================================

static void wipe_cb(void* ctx, uint32_t idx) {
    GhostApp* app = ctx;
    // Map index to attempts: 0=3, 1=5, 2=7, 3=10
    const uint8_t attempts[] = {3, 5, 7, 10};
    app->setup_max_attempts = attempts[idx];
    ghost_scene_setup_enter(app); // Go to passcode creation
}

void ghost_scene_wipe_setup_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneWipeSetup;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Auto-Wipe Threshold");
    submenu_add_item(app->submenu, "3 attempts  (Maximum)", 0, wipe_cb, app);
    submenu_add_item(app->submenu, "5 attempts  (Recommended)", 1, wipe_cb, app);
    submenu_add_item(app->submenu, "7 attempts  (Relaxed)", 2, wipe_cb, app);
    submenu_add_item(app->submenu, "10 attempts (Lenient)", 3, wipe_cb, app);

    // Pre-select recommended (index 1)
    submenu_set_selected_item(app->submenu, 1);

    view_dispatcher_switch_to_view(app->view, GhostViewSubmenu);
}

bool ghost_scene_wipe_setup_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        // Go back to security level selection
        ghost_scene_security_setup_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_wipe_setup_exit(void* ctx) {
    GhostApp* app = ctx;
    submenu_reset(app->submenu);
}

// ============================================================================
// SETUP SCENE - First-time passcode creation
// ============================================================================

void ghost_scene_setup_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneSetup;
    app->passcode_pos = 0;
    memset(app->passcode_input, 0, MAX_PASSCODE_LENGTH);

    ghost_passcode_view_set_mode(app, "Enter Your Code", false);
    view_dispatcher_switch_to_view(app->view, GhostViewPasscode);
}

bool ghost_scene_setup_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;

    if(e.type == SceneManagerEventTypeCustom && e.event == EvPasscodeComplete) {
        // Save first entry and move to confirm
        memcpy(app->setup_passcode, app->passcode_input, app->setup_length);
        notification_message(app->notif, &sequence_blink_green_10);
        ghost_scene_setup_confirm_enter(app);
        return true;
    }

    return false;
}

void ghost_scene_setup_exit(void* ctx) {
    GhostApp* app = ctx;
    ghost_passcode_view_reset(app);
}

// ============================================================================
// SETUP CONFIRM SCENE - Confirm passcode
// ============================================================================

void ghost_scene_setup_confirm_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneSetupConfirm;
    app->passcode_pos = 0;
    memset(app->passcode_input, 0, MAX_PASSCODE_LENGTH);
    ghost_passcode_view_set_mode(app, "Confirm Code", false);
    view_dispatcher_switch_to_view(app->view, GhostViewPasscode);
}

bool ghost_scene_setup_confirm_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;

    if(e.type == SceneManagerEventTypeCustom && e.event == EvPasscodeComplete) {
        // Check if matches
        if(memcmp(app->setup_passcode, app->passcode_input, app->setup_length) == 0) {
            // Match! Set up auth
            ghost_auth_set(app, app->passcode_input, app->setup_length, app->setup_max_attempts);
            notification_message(app->notif, &sequence_success);

            // Initialize default profile
            ghost_card_init(&app->my_card);
            strncpy(app->my_card.handle, "ghost", GHOST_HANDLE_LEN - 1);
            ghost_save_profile(app);

            ghost_scene_main_enter(app);
        } else {
            // No match - start over
            notification_message(app->notif, &sequence_error);

            popup_reset(app->popup);
            popup_set_header(app->popup, "NO MATCH", 64, 20, AlignCenter, AlignTop);
            popup_set_text(
                app->popup, "Passcodes didn't match\nTry again", 64, 35, AlignCenter, AlignTop);
            popup_set_timeout(app->popup, 1500);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view, GhostViewPopup);

            furi_delay_ms(1500);
            ghost_scene_setup_enter(app);
        }
        return true;
    }

    return false;
}

void ghost_scene_setup_confirm_exit(void* ctx) {
    GhostApp* app = ctx;
    ghost_passcode_view_reset(app);
    memset(app->setup_passcode, 0, MAX_PASSCODE_LENGTH);
}

// ============================================================================
// WIPED SCENE - Shown after data wipe with melting animation
// ============================================================================

static const char* ghost_melt_frames[] = {
    // Frame 0 - Normal ghost
    "   (o_o)\n"
    "   /| |\\\n"
    "    | |\n"
    "    ^ ^",

    // Frame 1 - Starting to drip
    "   (o_o)\n"
    "   /| |\\\n"
    "    |.|",

    // Frame 2 - Eyes drooping
    "   (o_o)\n"
    "   /|.|\\",

    // Frame 3 - Melting more
    "   (o_-)\n"
    "    ...",

    // Frame 4 - Really melting
    "   (x_-)\n"
    "   ~~~~~",

    // Frame 5 - Almost gone
    "    x_x\n"
    "  ~~~~~~~",

    // Frame 6 - Puddle
    " ~~~~~~~~~~~",

    // Frame 7 - Gone
    "   . . .",
};

#define GHOST_MELT_FRAMES 8

void ghost_scene_wiped_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneWiped;

    // Play error sound
    notification_message(app->notif, &sequence_error);

    // Animate the melting ghost
    for(int frame = 0; frame < GHOST_MELT_FRAMES; frame++) {
        widget_reset(app->widget);

        if(frame < 3) {
            widget_add_string_element(
                app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "!! I'M MELTING !!");
        } else if(frame < 6) {
            widget_add_string_element(
                app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "!! MELTING... !!");
        } else {
            widget_add_string_element(
                app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "!! GOODBYE !!");
        }

        widget_add_string_multiline_element(
            app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary, ghost_melt_frames[frame]);

        view_dispatcher_switch_to_view(app->view, GhostViewWidget);

        // Blink red during animation
        notification_message(app->notif, &sequence_blink_red_10);

        furi_delay_ms(350);
    }

    // Final screen
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "DATA WIPED");
    widget_add_string_element(app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary, ". . .");
    widget_add_string_element(
        app->widget, 64, 36, AlignCenter, AlignTop, FontSecondary, "All data destroyed.");
    widget_add_string_element(
        app->widget, 64, 48, AlignCenter, AlignTop, FontSecondary, "Too many failed attempts.");
    widget_add_string_element(
        app->widget, 64, 62, AlignCenter, AlignBottom, FontSecondary, "[BACK] Exit");

    view_dispatcher_switch_to_view(app->view, GhostViewWidget);

    // Final vibrate
    notification_message(app->notif, &sequence_error);
}

bool ghost_scene_wiped_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view);
        return true;
    }
    return false;
}

void ghost_scene_wiped_exit(void* ctx) {
    GhostApp* app = ctx;
    widget_reset(app->widget);
}

// ============================================================================
// MAIN MENU SCENE
// ============================================================================

static void main_cb(void* ctx, uint32_t idx) {
    GhostApp* app = ctx;
    uint32_t ev[] = {EvView, EvEdit, EvShare, EvReceive, EvContacts, EvExport, EvAbout};
    view_dispatcher_send_custom_event(app->view, ev[idx]);
}

void ghost_scene_main_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneMain;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "(o_o) GhostBook");
    submenu_add_item(app->submenu, "View Card", MenuView, main_cb, app);
    submenu_add_item(app->submenu, "Edit Profile", MenuEdit, main_cb, app);
    submenu_add_item(app->submenu, "Tap Share", MenuShare, main_cb, app);
    submenu_add_item(app->submenu, "Tap Receive", MenuReceive, main_cb, app);
    submenu_add_item(app->submenu, "Contacts", MenuContacts, main_cb, app);
    submenu_add_item(app->submenu, "Export", MenuExport, main_cb, app);
    submenu_add_item(app->submenu, "About", MenuAbout, main_cb, app);
    view_dispatcher_switch_to_view(app->view, GhostViewSubmenu);
}

bool ghost_scene_main_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view);
        return true;
    }
    if(e.type == SceneManagerEventTypeCustom) {
        switch(e.event) {
        case EvView:
            ghost_scene_view_enter(app);
            return true;
        case EvEdit:
            ghost_scene_edit_enter(app);
            return true;
        case EvShare:
            ghost_scene_share_enter(app);
            return true;
        case EvReceive:
            ghost_scene_receive_enter(app);
            return true;
        case EvContacts:
            ghost_scene_contacts_enter(app);
            return true;
        case EvExport:
            ghost_scene_export_enter(app);
            return true;
        case EvAbout:
            ghost_scene_about_enter(app);
            return true;
        }
    }
    return false;
}

void ghost_scene_main_exit(void* ctx) {
    GhostApp* app = ctx;
    submenu_reset(app->submenu);
}

// ============================================================================
// VIEW CARD SCENE
// ============================================================================

void ghost_scene_view_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneView;
    GhostCard* c = &app->my_card;
    widget_reset(app->widget);
    FuriString* t = furi_string_alloc();
    furi_string_printf(
        t,
        "========================\n  @%s %s\n========================\n",
        c->handle,
        ghost_flair_icon(c->flair));
    if(strlen(c->name)) furi_string_cat_printf(t, "Name: %s\n", c->name);
    if(strlen(c->email)) furi_string_cat_printf(t, "Email: %s\n", c->email);
    if(strlen(c->discord)) furi_string_cat_printf(t, "Discord: %s\n", c->discord);
    if(strlen(c->signal)) furi_string_cat_printf(t, "Signal: %s\n", c->signal);
    if(strlen(c->telegram)) furi_string_cat_printf(t, "Telegram: %s\n", c->telegram);
    if(strlen(c->notes)) furi_string_cat_printf(t, "------------------------\n%s", c->notes);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(t));
    furi_string_free(t);
    view_dispatcher_switch_to_view(app->view, GhostViewWidget);
}

bool ghost_scene_view_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_scene_main_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_view_exit(void* ctx) {
    GhostApp* app = ctx;
    widget_reset(app->widget);
}

// ============================================================================
// EDIT MENU SCENE
// ============================================================================

static void edit_cb(void* ctx, uint32_t idx) {
    GhostApp* app = ctx;
    app->edit_field = idx;
    uint32_t ev[] = {EvHandle, EvName, EvEmail, EvDiscord, EvSignal, EvTelegram, EvNotes, EvFlair};
    view_dispatcher_send_custom_event(app->view, ev[idx]);
}

void ghost_scene_edit_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneEdit;
    GhostCard* c = &app->my_card;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Edit Profile");
    FuriString* s = furi_string_alloc();
    furi_string_printf(s, "@Handle: %s", strlen(c->handle) ? c->handle : "---");
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditHandle, edit_cb, app);
    furi_string_printf(s, "Name: %s", strlen(c->name) ? c->name : "(optional)");
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditName, edit_cb, app);
    furi_string_printf(s, "Email: %s", strlen(c->email) ? c->email : "---");
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditEmail, edit_cb, app);
    furi_string_printf(s, "Discord: %s", strlen(c->discord) ? c->discord : "---");
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditDiscord, edit_cb, app);
    furi_string_printf(s, "Signal: %s", strlen(c->signal) ? c->signal : "---");
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditSignal, edit_cb, app);
    furi_string_printf(s, "Telegram: %s", strlen(c->telegram) ? c->telegram : "---");
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditTelegram, edit_cb, app);
    furi_string_printf(s, "Notes: %s", strlen(c->notes) ? c->notes : "---");
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditNotes, edit_cb, app);
    furi_string_printf(s, "Flair: %s %s", ghost_flair_icon(c->flair), ghost_flair_name(c->flair));
    submenu_add_item(app->submenu, furi_string_get_cstr(s), EditFlair, edit_cb, app);
    furi_string_free(s);
    view_dispatcher_switch_to_view(app->view, GhostViewSubmenu);
}

bool ghost_scene_edit_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_save_profile(app);
        ghost_scene_main_enter(app);
        return true;
    }
    if(e.type == SceneManagerEventTypeCustom) {
        switch(e.event) {
        case EvHandle:
        case EvName:
        case EvEmail:
        case EvDiscord:
        case EvSignal:
        case EvTelegram:
        case EvNotes:
            ghost_scene_text_enter(app);
            return true;
        case EvFlair:
            ghost_scene_flair_enter(app);
            return true;
        }
    }
    return false;
}

void ghost_scene_edit_exit(void* ctx) {
    GhostApp* app = ctx;
    submenu_reset(app->submenu);
}

// ============================================================================
// TEXT INPUT SCENE
// ============================================================================

static void text_cb(void* ctx) {
    GhostApp* app = ctx;
    view_dispatcher_send_custom_event(app->view, EvTextDone);
}

void ghost_scene_text_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneEditHandle + app->edit_field; // Maps to correct edit scene
    GhostCard* c = &app->my_card;
    const char* hdr = "Enter text";
    char* cur = NULL;
    size_t len = GHOST_HANDLE_LEN;
    switch(app->edit_field) {
    case EditHandle:
        hdr = "Your @handle";
        cur = c->handle;
        len = GHOST_HANDLE_LEN;
        break;
    case EditName:
        hdr = "Name (optional)";
        cur = c->name;
        len = GHOST_NAME_LEN;
        break;
    case EditEmail:
        hdr = "Email";
        cur = c->email;
        len = GHOST_EMAIL_LEN;
        break;
    case EditDiscord:
        hdr = "Discord";
        cur = c->discord;
        len = GHOST_DISCORD_LEN;
        break;
    case EditSignal:
        hdr = "Signal";
        cur = c->signal;
        len = GHOST_SIGNAL_LEN;
        break;
    case EditTelegram:
        hdr = "Telegram";
        cur = c->telegram;
        len = GHOST_TELEGRAM_LEN;
        break;
    case EditNotes:
        hdr = "Notes";
        cur = c->notes;
        len = GHOST_NOTES_LEN;
        break;
    }
    strncpy(app->text_buf, cur ? cur : "", sizeof(app->text_buf) - 1);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, hdr);
    text_input_set_result_callback(app->text_input, text_cb, app, app->text_buf, len, true);
    view_dispatcher_switch_to_view(app->view, GhostViewTextInput);
}

bool ghost_scene_text_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    GhostCard* c = &app->my_card;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_scene_edit_enter(app);
        return true;
    }
    if(e.type == SceneManagerEventTypeCustom && e.event == EvTextDone) {
        switch(app->edit_field) {
        case EditHandle:
            strncpy(c->handle, app->text_buf, GHOST_HANDLE_LEN - 1);
            break;
        case EditName:
            strncpy(c->name, app->text_buf, GHOST_NAME_LEN - 1);
            break;
        case EditEmail:
            strncpy(c->email, app->text_buf, GHOST_EMAIL_LEN - 1);
            break;
        case EditDiscord:
            strncpy(c->discord, app->text_buf, GHOST_DISCORD_LEN - 1);
            break;
        case EditSignal:
            strncpy(c->signal, app->text_buf, GHOST_SIGNAL_LEN - 1);
            break;
        case EditTelegram:
            strncpy(c->telegram, app->text_buf, GHOST_TELEGRAM_LEN - 1);
            break;
        case EditNotes:
            strncpy(c->notes, app->text_buf, GHOST_NOTES_LEN - 1);
            break;
        }
        ghost_scene_edit_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_text_exit(void* ctx) {
    GhostApp* app = ctx;
    text_input_reset(app->text_input);
}

// ============================================================================
// FLAIR SCENE
// ============================================================================

static void flair_cb(void* ctx, uint32_t idx) {
    GhostApp* app = ctx;
    app->my_card.flair = idx;
    view_dispatcher_send_custom_event(app->view, EvFlairDone);
}

void ghost_scene_flair_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneFlair;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Flair");
    FuriString* s = furi_string_alloc();
    for(int i = 0; i < GhostFlairNum; i++) {
        furi_string_printf(s, "%s  %s", ghost_flair_icon(i), ghost_flair_name(i));
        submenu_add_item(app->submenu, furi_string_get_cstr(s), i, flair_cb, app);
    }
    furi_string_free(s);
    submenu_set_selected_item(app->submenu, app->my_card.flair);
    view_dispatcher_switch_to_view(app->view, GhostViewSubmenu);
}

bool ghost_scene_flair_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack ||
       (e.type == SceneManagerEventTypeCustom && e.event == EvFlairDone)) {
        ghost_scene_edit_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_flair_exit(void* ctx) {
    GhostApp* app = ctx;
    submenu_reset(app->submenu);
}

// ============================================================================
// SHARE SCENE
// ============================================================================

void ghost_scene_share_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneShare;

    widget_reset(app->widget);

    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontSecondary, "(o_o)");
    widget_add_string_element(
        app->widget, 64, 14, AlignCenter, AlignTop, FontPrimary, "BROADCASTING");

    // Handle
    FuriString* s = furi_string_alloc();
    furi_string_printf(s, "@%s", app->my_card.handle);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(s));
    furi_string_free(s);

    widget_add_string_element(
        app->widget, 64, 48, AlignCenter, AlignTop, FontSecondary, "Place near receiver");
    widget_add_string_element(
        app->widget, 64, 62, AlignCenter, AlignBottom, FontSecondary, "[BACK] Stop");

    view_dispatcher_switch_to_view(app->view, GhostViewWidget);

    // Start NFC emulation
    ghost_nfc_share_start(app);
    notification_message(app->notif, &sequence_blink_start_cyan);
}

bool ghost_scene_share_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_nfc_share_stop(app);
        notification_message(app->notif, &sequence_blink_stop);
        ghost_scene_main_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_share_exit(void* ctx) {
    GhostApp* app = ctx;
    ghost_nfc_share_stop(app);
    notification_message(app->notif, &sequence_blink_stop);
    widget_reset(app->widget);
}

// ============================================================================
// RECEIVE SCENE
// ============================================================================

void ghost_scene_receive_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneReceive;
    app->card_received = false;

    widget_reset(app->widget);

    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontSecondary, "(o_o)");
    widget_add_string_element(
        app->widget, 64, 14, AlignCenter, AlignTop, FontPrimary, "LISTENING");
    widget_add_string_element(
        app->widget, 64, 34, AlignCenter, AlignTop, FontSecondary, "Place near sender");
    widget_add_string_element(
        app->widget, 64, 62, AlignCenter, AlignBottom, FontSecondary, "[BACK] Cancel");

    view_dispatcher_switch_to_view(app->view, GhostViewWidget);

    // Start NFC polling
    ghost_nfc_receive_start(app);
    notification_message(app->notif, &sequence_blink_start_magenta);
}

bool ghost_scene_receive_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;

    // Check if card was received
    if(app->card_received) {
        ghost_nfc_receive_stop(app);
        notification_message(app->notif, &sequence_blink_stop);
        notification_message(app->notif, &sequence_success);

        // Save the contact
        ghost_save_contact(app, &app->received_card);

        // Show success popup
        popup_reset(app->popup);

        FuriString* msg = furi_string_alloc();
        furi_string_printf(msg, "@%s", app->received_card.handle);

        popup_set_header(app->popup, "SAVED!", 64, 15, AlignCenter, AlignTop);
        popup_set_text(app->popup, furi_string_get_cstr(msg), 64, 35, AlignCenter, AlignCenter);
        popup_set_timeout(app->popup, 2000);
        popup_enable_timeout(app->popup);

        furi_string_free(msg);

        view_dispatcher_switch_to_view(app->view, GhostViewPopup);

        furi_delay_ms(2000);
        ghost_scene_main_enter(app);
        return true;
    }

    if(e.type == SceneManagerEventTypeBack) {
        ghost_nfc_receive_stop(app);
        notification_message(app->notif, &sequence_blink_stop);
        ghost_scene_main_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_receive_exit(void* ctx) {
    GhostApp* app = ctx;
    ghost_nfc_receive_stop(app);
    notification_message(app->notif, &sequence_blink_stop);
    widget_reset(app->widget);
}

// ============================================================================
// CONTACTS SCENE
// ============================================================================

static void contacts_cb(void* ctx, uint32_t idx) {
    GhostApp* app = ctx;
    app->selected_contact = idx;
    view_dispatcher_send_custom_event(app->view, EvContactView);
}

void ghost_scene_contacts_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneContacts;
    ghost_load_contacts(app);
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "# Contacts #");
    if(app->contact_count == 0) {
        submenu_add_item(app->submenu, "(no ghosts yet)", 0, NULL, NULL);
    } else {
        FuriString* s = furi_string_alloc();
        for(uint32_t i = 0; i < app->contact_count; i++) {
            furi_string_printf(
                s,
                "@%s %s",
                app->contacts[i].card.handle,
                ghost_flair_icon(app->contacts[i].card.flair));
            submenu_add_item(app->submenu, furi_string_get_cstr(s), i, contacts_cb, app);
        }
        furi_string_free(s);
    }
    view_dispatcher_switch_to_view(app->view, GhostViewSubmenu);
}

bool ghost_scene_contacts_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_scene_main_enter(app);
        return true;
    }
    if(e.type == SceneManagerEventTypeCustom && e.event == EvContactView) {
        ghost_scene_contact_view_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_contacts_exit(void* ctx) {
    GhostApp* app = ctx;
    submenu_reset(app->submenu);
}

// ============================================================================
// CONTACT VIEW SCENE
// ============================================================================

void ghost_scene_contact_view_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneContactView;
    if(app->selected_contact >= app->contact_count) {
        ghost_scene_contacts_enter(app);
        return;
    }
    GhostCard* c = &app->contacts[app->selected_contact].card;
    widget_reset(app->widget);
    FuriString* t = furi_string_alloc();
    furi_string_printf(
        t,
        "========================\n  @%s %s\n========================\n",
        c->handle,
        ghost_flair_icon(c->flair));
    if(strlen(c->name)) furi_string_cat_printf(t, "Name: %s\n", c->name);
    if(strlen(c->email)) furi_string_cat_printf(t, "Email: %s\n", c->email);
    if(strlen(c->discord)) furi_string_cat_printf(t, "Discord: %s\n", c->discord);
    if(strlen(c->signal)) furi_string_cat_printf(t, "Signal: %s\n", c->signal);
    if(strlen(c->notes)) furi_string_cat_printf(t, "------------------------\n%s", c->notes);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(t));
    furi_string_free(t);
    view_dispatcher_switch_to_view(app->view, GhostViewWidget);
}

bool ghost_scene_contact_view_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_scene_contacts_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_contact_view_exit(void* ctx) {
    GhostApp* app = ctx;
    widget_reset(app->widget);
}

// ============================================================================
// EXPORT SCENE
// ============================================================================

static void export_cb(void* ctx, uint32_t idx) {
    GhostApp* app = ctx;
    bool ok = false;
    const char* msg = "Failed";
    switch(idx) {
    case 0:
        ok = ghost_export_vcard(app, &app->my_card, "/ext/ghostbook_me.vcf");
        msg = ok ? "ghostbook_me.vcf" : "Failed";
        break;
    case 1:
        ok = ghost_export_all_vcard(app, "/ext/ghostbook_all.vcf");
        msg = ok ? "ghostbook_all.vcf" : "Failed";
        break;
    case 2:
        ok = ghost_export_csv(app, "/ext/ghostbook.csv");
        msg = ok ? "ghostbook.csv" : "Failed";
        break;
    }
    popup_reset(app->popup);
    popup_set_header(app->popup, ok ? "Saved!" : "Error", 64, 20, AlignCenter, AlignTop);
    popup_set_text(app->popup, msg, 64, 38, AlignCenter, AlignTop);
    popup_set_timeout(app->popup, 2000);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view, GhostViewPopup);
    notification_message(app->notif, ok ? &sequence_success : &sequence_error);
}

void ghost_scene_export_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneExport;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "^ Export ^");
    submenu_add_item(app->submenu, "My Card -> vCard", 0, export_cb, app);
    submenu_add_item(app->submenu, "Contacts -> vCard", 1, export_cb, app);
    submenu_add_item(app->submenu, "All -> CSV", 2, export_cb, app);
    view_dispatcher_switch_to_view(app->view, GhostViewSubmenu);
}

bool ghost_scene_export_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_scene_main_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_export_exit(void* ctx) {
    GhostApp* app = ctx;
    submenu_reset(app->submenu);
}

// ============================================================================
// ABOUT SCENE
// ============================================================================

void ghost_scene_about_enter(void* ctx) {
    GhostApp* app = ctx;
    app->current_scene = GhostSceneAbout;
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        ">> GhostBook v0.6.0 <<\n"
        "====================\n"
        "[ENCRYPTED CONTACTS]\n"
        "[PASSCODE LOCKED]\n"
        "[AUTO-DESTRUCT: 5x]\n"
        "\n"
        "Trust no one.\n"
        "Leave nothing.\n"
        "\n"
        "~#!~ (o_o) ~#!~\n"
        "\n"
        "by Digi\n"
        "github.com/digitard");
    view_dispatcher_switch_to_view(app->view, GhostViewWidget);
}

bool ghost_scene_about_event(void* ctx, SceneManagerEvent e) {
    GhostApp* app = ctx;
    if(e.type == SceneManagerEventTypeBack) {
        ghost_scene_main_enter(app);
        return true;
    }
    return false;
}

void ghost_scene_about_exit(void* ctx) {
    GhostApp* app = ctx;
    widget_reset(app->widget);
}
