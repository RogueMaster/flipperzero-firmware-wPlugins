/**
 * @file flippass_scene_send_confirm.c
 * @brief Helpers for executing pending credential typing actions.
 */
#include "flippass_scene_send_confirm.h"
#include "../flippass.h"
#include "../flippass_db.h"
#include "../kdbx/memzero.h"

#include <string.h>

#define FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES (8U * 1024U)

static FlipPassOutputTransport flippass_entry_action_transport(FlipPassEntryAction action) {
    switch(action) {
    case FlipPassEntryActionTypeUsernameBluetooth:
    case FlipPassEntryActionTypePasswordBluetooth:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
    case FlipPassEntryActionTypeLoginBluetooth:
    case FlipPassEntryActionTypeOtherBluetooth:
        return FlipPassOutputTransportBluetooth;
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeOtherUsb:
    default:
        return FlipPassOutputTransportUsb;
    }
}

static FlipPassOutputTransport
    flippass_entry_action_other_transport(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ? FlipPassOutputTransportUsb :
                                                             FlipPassOutputTransportBluetooth;
}

static const char* flippass_entry_action_log_prefix(FlipPassOutputTransport transport) {
    return transport == FlipPassOutputTransportBluetooth ? "BT" : "USB";
}

static const char* flippass_entry_action_log_label(const App* app, FlipPassEntryAction action) {
    switch(action) {
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
        return "username";
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypePasswordBluetooth:
        return "password";
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
        return "autotype";
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeLoginBluetooth:
        return "login";
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeOtherBluetooth:
        if(app != NULL && app->pending_other_field_name[0] != '\0') {
            return app->pending_other_field_name;
        }
        return "other";
    case FlipPassEntryActionNone:
    default:
        return "unknown";
    }
}

static void flippass_entry_action_focus_entry(App* app, KDBXEntry* entry) {
    furi_assert(app);
    furi_assert(entry);

    if(app->active_entry != entry) {
        flippass_db_deactivate_entry(app);
        app->active_entry = entry;
        FLIPPASS_DIAGNOSTIC_LOG(app, "ENTRY_MATERIALIZE");
    }

    app->current_entry = entry;
}

static unsigned long
    flippass_entry_action_text_or_ref_len(const char* text, const KDBXFieldRef* ref) {
    if(text != NULL) {
        return (unsigned long)strlen(text);
    }

    if(ref != NULL && !kdbx_vault_ref_is_empty(ref)) {
        return (unsigned long)ref->plain_len;
    }

    return 0UL;
}

static bool flippass_entry_action_can_materialize_ref(const KDBXFieldRef* ref) {
    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        return true;
    }

    const size_t required = ref->plain_len + 1U;
    const size_t free_heap = memmgr_get_free_heap();
    const size_t max_free_block = memmgr_heap_get_max_free_block();

    return required <= max_free_block &&
           required + FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES <= free_heap;
}

static bool flippass_entry_action_should_stream_ref(const KDBXFieldRef* ref) {
    return ref != NULL && !kdbx_vault_ref_is_empty(ref) && ref->record_count > 1U;
}

static bool flippass_entry_action_load_ref_text(
    App* app,
    const KDBXFieldRef* ref,
    char** out_text,
    size_t* out_size,
    FuriString* error) {
    furi_assert(app);
    furi_assert(out_text);
    furi_assert(out_size);

    *out_text = NULL;
    *out_size = 0U;

    if(app->vault == NULL || ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    if(!kdbx_vault_load_text(app->vault, ref, out_text, out_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    return true;
}

static void flippass_entry_action_free_temp_text(char** text, size_t* size) {
    if(text == NULL || *text == NULL) {
        return;
    }

    const size_t text_size = (size != NULL) ? *size : strlen(*text);
    memzero(*text, text_size + 1U);
    free(*text);
    *text = NULL;

    if(size != NULL) {
        *size = 0U;
    }
}

static const KDBXFieldRef*
    flippass_entry_action_other_ref(const App* app, const KDBXEntry* entry) {
    if(app == NULL || entry == NULL) {
        return NULL;
    }

    if(app->pending_other_custom_field != NULL) {
        return kdbx_custom_field_get_ref(app->pending_other_custom_field);
    }

    if(app->pending_other_field_mask != 0U) {
        return kdbx_entry_get_field_ref(entry, app->pending_other_field_mask);
    }

    return NULL;
}

void flippass_entry_action_prepare_pending(App* app) {
    furi_assert(app);

    const FlipPassOutputTransport transport =
        flippass_entry_action_transport(app->pending_entry_action);

    FLIPPASS_MEMORY_LOG(app, "type_prepare_known_begin", FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES);
    flippass_output_release_all(app);
    flippass_output_cleanup_transport(app, flippass_entry_action_other_transport(transport));

    if(transport == FlipPassOutputTransportUsb) {
        FLIPPASS_MEMORY_LOG(app, "type_prepare_known_end", FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES);
        return;
    }

    if(!flippass_output_bluetooth_is_connected(app)) {
        flippass_output_bluetooth_advertise(app);
    }
    FLIPPASS_MEMORY_LOG(app, "type_prepare_known_end", FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES);
}

void flippass_entry_action_prepare_type_menu(App* app) {
    furi_assert(app);

    FLIPPASS_MEMORY_LOG(app, "type_menu_prewarm_begin", FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES);
    flippass_output_release_all(app);

    flippass_output_prewarm_transport(app, FlipPassOutputTransportUsb);
    if(flippass_output_usb_is_connected(app)) {
        flippass_output_cleanup_transport(app, FlipPassOutputTransportBluetooth);
    } else if(!flippass_output_bluetooth_is_connected(app)) {
        flippass_output_prewarm_transport(app, FlipPassOutputTransportBluetooth);
    }

    FLIPPASS_MEMORY_LOG(app, "type_menu_prewarm_end", FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES);
}

void flippass_entry_action_cleanup_type_menu(App* app) {
    furi_assert(app);

    FLIPPASS_MEMORY_LOG(app, "type_menu_cleanup_begin", FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES);
    flippass_output_release_all(app);
    flippass_output_cleanup(app);
    FLIPPASS_MEMORY_LOG(app, "type_menu_cleanup_end", FLIPPASS_TYPE_LOAD_HEAP_RESERVE_BYTES);
}

bool flippass_entry_action_execute_pending(App* app, FuriString* error) {
    KDBXEntry* entry = app->active_entry;
    const char* other_value = NULL;
    const char* username_value = NULL;
    const char* password_value = NULL;
    const char* autotype_value = NULL;
    const KDBXFieldRef* username_ref = NULL;
    const KDBXFieldRef* password_ref = NULL;
    const KDBXFieldRef* autotype_ref = NULL;
    const KDBXFieldRef* other_ref = NULL;
    char* temp_username = NULL;
    char* temp_password = NULL;
    char* temp_other = NULL;
    char otp_code[FLIPPASS_OTP_CODE_MAX_CHARS + 1U];
    size_t temp_username_size = 0U;
    size_t temp_password_size = 0U;
    size_t temp_other_size = 0U;
    unsigned long char_count = 0UL;
    bool typed = false;
    const FlipPassOutputTransport transport =
        flippass_entry_action_transport(app->pending_entry_action);
    const char* log_prefix = flippass_entry_action_log_prefix(transport);
    const char* log_label = flippass_entry_action_log_label(app, app->pending_entry_action);
#if !FLIPPASS_ENABLE_LOGS
    UNUSED(log_prefix);
    UNUSED(log_label);
#endif

    if(entry == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "Return to the browser and pick an entry first.");
        }
        return false;
    }

    flippass_entry_action_focus_entry(app, entry);
    otp_code[0] = '\0';

    switch(app->pending_entry_action) {
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
        if(!flippass_db_entry_has_field(entry, KDBXEntryFieldUsername)) {
            if(error != NULL) {
                furi_string_set_str(error, "This entry does not contain a username to send.");
            }
            return false;
        }
        username_value = entry->username;
        username_ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldUsername);
        char_count = flippass_entry_action_text_or_ref_len(username_value, username_ref);
        break;
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypePasswordBluetooth:
        if(!flippass_db_entry_has_field(entry, KDBXEntryFieldPassword)) {
            if(error != NULL) {
                furi_string_set_str(error, "This entry does not contain a password to send.");
            }
            return false;
        }
        password_value = entry->password;
        password_ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldPassword);
        char_count = flippass_entry_action_text_or_ref_len(password_value, password_ref);
        break;
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
        if(!flippass_db_entry_has_field(entry, KDBXEntryFieldAutotype) &&
           !(flippass_db_entry_has_field(entry, KDBXEntryFieldUsername) &&
             flippass_db_entry_has_field(entry, KDBXEntryFieldPassword))) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "This entry needs an AutoType sequence or both username and password.");
            }
            return false;
        }
        autotype_value = entry->autotype_sequence;
        autotype_ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldAutotype);
        username_value = entry->username;
        password_value = entry->password;
        username_ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldUsername);
        password_ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldPassword);
        char_count =
            (autotype_value != NULL ||
             (autotype_ref != NULL && !kdbx_vault_ref_is_empty(autotype_ref))) ?
                flippass_entry_action_text_or_ref_len(autotype_value, autotype_ref) :
                (flippass_entry_action_text_or_ref_len(username_value, username_ref) +
                 flippass_entry_action_text_or_ref_len(password_value, password_ref) + 2UL);
        break;
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeLoginBluetooth:
        if(!(flippass_db_entry_has_field(entry, KDBXEntryFieldUsername) &&
             flippass_db_entry_has_field(entry, KDBXEntryFieldPassword))) {
            if(error != NULL) {
                furi_string_set_str(
                    error,
                    "This entry needs both a username and a password for a login sequence.");
            }
            return false;
        }
        username_value = entry->username;
        password_value = entry->password;
        username_ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldUsername);
        password_ref = kdbx_entry_get_field_ref(entry, KDBXEntryFieldPassword);
        char_count = flippass_entry_action_text_or_ref_len(username_value, username_ref) +
                     flippass_entry_action_text_or_ref_len(password_value, password_ref) + 2UL;
        break;
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeOtherBluetooth:
        if(app->pending_other_otp_kind != FlipPassOtpKindNone) {
            if(!flippass_otp_generate_code(
                   app, entry, app->pending_other_otp_kind, true, otp_code, error)) {
                return false;
            }
            other_value = otp_code;
            other_ref = NULL;
            char_count = strlen(other_value);
            break;
        }
        other_value = (app->pending_other_custom_field != NULL) ?
                          app->pending_other_custom_field->value :
                          ((app->pending_other_field_mask == KDBXEntryFieldUrl) ?
                               entry->url :
                           (app->pending_other_field_mask == KDBXEntryFieldNotes) ?
                               entry->notes :
                           (app->pending_other_field_mask == KDBXEntryFieldUsername) ?
                               entry->username :
                           (app->pending_other_field_mask == KDBXEntryFieldPassword) ?
                               entry->password :
                           (app->pending_other_field_mask == KDBXEntryFieldAutotype) ?
                               entry->autotype_sequence :
                               NULL);
        other_ref = flippass_entry_action_other_ref(app, entry);
        if((other_value == NULL || other_value[0] == '\0') &&
           (other_ref == NULL || kdbx_vault_ref_is_empty(other_ref) ||
            other_ref->plain_len == 0U)) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "The selected field does not contain any text to send.");
            }
            return false;
        }
        char_count = flippass_entry_action_text_or_ref_len(other_value, other_ref);
        break;
    case FlipPassEntryActionNone:
    default:
        if(error != NULL) {
            furi_string_set_str(
                error, "Pick a USB or Bluetooth typing action from the browser first.");
        }
        return false;
    }

    if(flippass_typing_should_cancel(app)) {
        typed = false;
        goto finish;
    }

    FLIPPASS_BENCH_LOG(app, "%s_TYPE_BEGIN field=%s chars=%lu", log_prefix, log_label, char_count);
    FLIPPASS_MEMORY_LOG(app, "type_execute_begin", (size_t)char_count);

    switch(app->pending_entry_action) {
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
        /* Keep large deferred fields on the streamed send path even after Show loaded them. */
        if(flippass_entry_action_should_stream_ref(username_ref)) {
            typed = flippass_output_type_vault_ref(app, transport, app->vault, username_ref);
        } else if(username_value != NULL) {
            typed = flippass_output_type_string(app, transport, username_value);
        } else if(!flippass_entry_action_can_materialize_ref(username_ref)) {
            typed = flippass_output_type_vault_ref(app, transport, app->vault, username_ref);
        } else if(flippass_entry_action_load_ref_text(
                      app, username_ref, &temp_username, &temp_username_size, error)) {
            typed = flippass_output_type_string(app, transport, temp_username);
        }
        break;
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypePasswordBluetooth:
        if(flippass_entry_action_should_stream_ref(password_ref)) {
            typed = flippass_output_type_vault_ref(app, transport, app->vault, password_ref);
        } else if(password_value != NULL) {
            typed = flippass_output_type_string(app, transport, password_value);
        } else if(!flippass_entry_action_can_materialize_ref(password_ref)) {
            typed = flippass_output_type_vault_ref(app, transport, app->vault, password_ref);
        } else if(flippass_entry_action_load_ref_text(
                      app, password_ref, &temp_password, &temp_password_size, error)) {
            typed = flippass_output_type_string(app, transport, temp_password);
        }
        break;
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
        if((autotype_value != NULL) ||
           (autotype_ref != NULL && !kdbx_vault_ref_is_empty(autotype_ref))) {
            typed = flippass_db_activate_entry(app, entry, false, error) &&
                    flippass_output_type_autotype(app, transport, entry);
        } else if(
            flippass_entry_action_should_stream_ref(username_ref) ||
            flippass_entry_action_should_stream_ref(password_ref)) {
            typed = flippass_output_type_login_refs(
                app, transport, app->vault, username_ref, password_ref);
        } else if(username_value != NULL && password_value != NULL) {
            typed = flippass_output_type_login(app, transport, username_value, password_value);
        } else if(
            !flippass_entry_action_can_materialize_ref(username_ref) ||
            !flippass_entry_action_can_materialize_ref(password_ref)) {
            typed = flippass_output_type_login_refs(
                app, transport, app->vault, username_ref, password_ref);
        } else if(
            flippass_entry_action_load_ref_text(
                app, username_ref, &temp_username, &temp_username_size, error) &&
            flippass_entry_action_load_ref_text(
                app, password_ref, &temp_password, &temp_password_size, error)) {
            typed = flippass_output_type_login(app, transport, temp_username, temp_password);
        }
        break;
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeLoginBluetooth:
        if(flippass_entry_action_should_stream_ref(username_ref) ||
           flippass_entry_action_should_stream_ref(password_ref)) {
            typed = flippass_output_type_login_refs(
                app, transport, app->vault, username_ref, password_ref);
        } else if(username_value != NULL && password_value != NULL) {
            typed = flippass_output_type_login(app, transport, username_value, password_value);
        } else if(
            !flippass_entry_action_can_materialize_ref(username_ref) ||
            !flippass_entry_action_can_materialize_ref(password_ref)) {
            typed = flippass_output_type_login_refs(
                app, transport, app->vault, username_ref, password_ref);
        } else if(
            flippass_entry_action_load_ref_text(
                app, username_ref, &temp_username, &temp_username_size, error) &&
            flippass_entry_action_load_ref_text(
                app, password_ref, &temp_password, &temp_password_size, error)) {
            typed = flippass_output_type_login(app, transport, temp_username, temp_password);
        }
        break;
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeOtherBluetooth:
        if(flippass_entry_action_should_stream_ref(other_ref)) {
            typed = flippass_output_type_vault_ref(app, transport, app->vault, other_ref);
        } else if(other_value != NULL) {
            typed = flippass_output_type_string(app, transport, other_value);
        } else if(!flippass_entry_action_can_materialize_ref(other_ref)) {
            typed = flippass_output_type_vault_ref(app, transport, app->vault, other_ref);
        } else if(flippass_entry_action_load_ref_text(
                      app, other_ref, &temp_other, &temp_other_size, error)) {
            typed = flippass_output_type_string(app, transport, temp_other);
        }
        break;
    case FlipPassEntryActionNone:
    default:
        typed = false;
        break;
    }

finish:
    memzero(otp_code, sizeof(otp_code));
    flippass_entry_action_free_temp_text(&temp_username, &temp_username_size);
    flippass_entry_action_free_temp_text(&temp_password, &temp_password_size);
    flippass_entry_action_free_temp_text(&temp_other, &temp_other_size);
    UNUSED(char_count);

    if(!typed && flippass_typing_should_cancel(app)) {
        FLIPPASS_LOG_EVENT(app, "%s_TYPE_CANCEL field=%s", log_prefix, log_label);
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "Typing canceled.");
        }
        return false;
    }

    FLIPPASS_LOG_EVENT(
        app, typed ? "%s_TYPE_OK field=%s" : "%s_TYPE_FAIL field=%s", log_prefix, log_label);
    FLIPPASS_MEMORY_LOG(app, typed ? "type_execute_ok" : "type_execute_fail", 0U);

    if(!typed && error != NULL && furi_string_empty(error)) {
        if(transport == FlipPassOutputTransportBluetooth) {
            if(flippass_output_bluetooth_is_advertising(app)) {
                furi_string_set_str(
                    error,
                    "Bluetooth HID is still waiting for a host connection. Reconnect or pair from the host, then try again.");
            } else {
                furi_string_set_str(
                    error,
                    "Bluetooth HID was unavailable, the host never connected in time, or the selected data uses unsupported tokens.");
            }
        } else {
            furi_string_set_str(
                error,
                "USB HID did not detect a host in time, or the selected data contains unsupported characters or tokens.");
        }
    }

    return typed;
}
