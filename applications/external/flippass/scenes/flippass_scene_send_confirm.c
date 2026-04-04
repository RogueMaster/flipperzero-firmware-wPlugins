/**
 * @file flippass_scene_send_confirm.c
 * @brief Helpers for executing pending credential typing actions.
 */
#include "flippass_scene_send_confirm.h"
#include "../flippass.h"
#include "../flippass_db.h"

#include <string.h>

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
    case FlipPassEntryActionShowDetails:
    case FlipPassEntryActionRevealUsername:
    case FlipPassEntryActionRevealPassword:
    case FlipPassEntryActionRevealUrl:
    case FlipPassEntryActionRevealNotes:
    case FlipPassEntryActionRevealAutoType:
    case FlipPassEntryActionBrowseOtherFields:
    default:
        return "unknown";
    }
}

static unsigned long flippass_entry_action_char_count(
    const KDBXEntry* entry,
    FlipPassEntryAction action,
    const char* other_value) {
    switch(action) {
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
        return (entry != NULL && entry->username != NULL) ? (unsigned long)strlen(entry->username) :
                                                            0UL;
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypePasswordBluetooth:
        return (entry != NULL && entry->password != NULL) ? (unsigned long)strlen(entry->password) :
                                                            0UL;
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeLoginBluetooth:
        return (entry != NULL) ?
                   (unsigned long)(
                       (entry->username ? strlen(entry->username) : 0U) +
                       (entry->password ? strlen(entry->password) : 0U) + 2U) :
                   0UL;
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
        if(entry != NULL && entry->autotype_sequence != NULL && entry->autotype_sequence[0] != '\0') {
            return (unsigned long)strlen(entry->autotype_sequence);
        }
        return (entry != NULL) ?
                   (unsigned long)(
                       (entry->username ? strlen(entry->username) : 0U) +
                       (entry->password ? strlen(entry->password) : 0U) + 2U) :
                   0UL;
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeOtherBluetooth:
        return other_value != NULL ? (unsigned long)strlen(other_value) : 0UL;
    case FlipPassEntryActionNone:
    case FlipPassEntryActionShowDetails:
    case FlipPassEntryActionRevealUsername:
    case FlipPassEntryActionRevealPassword:
    case FlipPassEntryActionRevealUrl:
    case FlipPassEntryActionRevealNotes:
    case FlipPassEntryActionRevealAutoType:
    case FlipPassEntryActionBrowseOtherFields:
    default:
        return 0UL;
    }
}

void flippass_entry_action_prepare_pending(App* app) {
    furi_assert(app);

    flippass_usb_restore(app);
    flippass_output_release_all(app);

    if(!flippass_output_bluetooth_is_connected(app)) {
        flippass_output_bluetooth_advertise(app);
    }
}

bool flippass_entry_action_execute_pending(App* app, FuriString* error) {
    KDBXEntry* entry = app->active_entry;
    const char* other_value = NULL;
    bool typed = false;
    const FlipPassOutputTransport transport =
        flippass_entry_action_transport(app->pending_entry_action);
    const char* log_prefix = flippass_entry_action_log_prefix(transport);
    const char* log_label = flippass_entry_action_log_label(app, app->pending_entry_action);

    if(entry == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "Return to the browser and pick an entry first.");
        }
        return false;
    }

    if(!flippass_db_activate_entry(app, entry, false, error)) {
        return false;
    }

    switch(app->pending_entry_action) {
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
        if(!flippass_db_entry_has_field(entry, KDBXEntryFieldUsername)) {
            if(error != NULL) {
                furi_string_set_str(error, "This entry does not contain a username to send.");
            }
            return false;
        }
        break;
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypePasswordBluetooth:
        if(!flippass_db_entry_has_field(entry, KDBXEntryFieldPassword)) {
            if(error != NULL) {
                furi_string_set_str(error, "This entry does not contain a password to send.");
            }
            return false;
        }
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
        break;
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeLoginBluetooth:
        if(!(flippass_db_entry_has_field(entry, KDBXEntryFieldUsername) &&
             flippass_db_entry_has_field(entry, KDBXEntryFieldPassword))) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "This entry needs both a username and a password for a login sequence.");
            }
            return false;
        }
        break;
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeOtherBluetooth:
        if(!flippass_db_get_other_field_value(
               app,
               entry,
               app->pending_other_field_mask,
               app->pending_other_custom_field,
               &other_value,
               error)) {
            return false;
        }
        if(other_value == NULL || other_value[0] == '\0') {
            if(error != NULL) {
                furi_string_set_str(error, "The selected field does not contain any text to send.");
            }
            return false;
        }
        break;
    case FlipPassEntryActionNone:
    case FlipPassEntryActionShowDetails:
    case FlipPassEntryActionRevealUsername:
    case FlipPassEntryActionRevealPassword:
    case FlipPassEntryActionRevealUrl:
    case FlipPassEntryActionRevealNotes:
    case FlipPassEntryActionRevealAutoType:
    case FlipPassEntryActionBrowseOtherFields:
    default:
        if(error != NULL) {
            furi_string_set_str(
                error, "Pick a USB or Bluetooth typing action from the browser first.");
        }
        return false;
    }

    flippass_log_event(
        app,
        "%s_TYPE_BEGIN field=%s chars=%lu",
        log_prefix,
        log_label,
        flippass_entry_action_char_count(entry, app->pending_entry_action, other_value));

    switch(app->pending_entry_action) {
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
        typed = flippass_output_type_string(app, transport, entry->username);
        break;
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypePasswordBluetooth:
        typed = flippass_output_type_string(app, transport, entry->password);
        break;
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
        typed = flippass_output_type_autotype(app, transport, entry);
        break;
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeLoginBluetooth:
        typed = flippass_output_type_login(app, transport, entry->username, entry->password);
        break;
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeOtherBluetooth:
        typed = flippass_output_type_string(app, transport, other_value);
        break;
    case FlipPassEntryActionNone:
    case FlipPassEntryActionShowDetails:
    case FlipPassEntryActionRevealUsername:
    case FlipPassEntryActionRevealPassword:
    case FlipPassEntryActionRevealUrl:
    case FlipPassEntryActionRevealNotes:
    case FlipPassEntryActionRevealAutoType:
    case FlipPassEntryActionBrowseOtherFields:
    default:
        typed = false;
        break;
    }

    flippass_log_event(
        app,
        typed ? "%s_TYPE_OK field=%s" : "%s_TYPE_FAIL field=%s",
        log_prefix,
        log_label);

    if(!typed && error != NULL) {
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
