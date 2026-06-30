#include "ami_tool_i.h"
#include <input/input.h>
#include <string.h>

#define AMI_TOOL_AMIIBO_LINK_COMPLETION_PAGE (129U)

static void ami_tool_scene_amiibo_link_handle_completion(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_reset_config_tracking(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_monitor_config(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_restore_pending_auth0(AmiToolApp* app);
static bool ami_tool_scene_amiibo_link_regenerate_template(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_capture_completion_marker(AmiToolApp* app);
static bool ami_tool_scene_amiibo_link_marker_written(const AmiToolApp* app);
static void ami_tool_scene_amiibo_link_sync_listener_data(AmiToolApp* app);
static void ami_tool_scene_amiibo_link_ready_button_callback(
    GuiButtonType result,
    InputType type,
    void* context);

static size_t ami_tool_scene_amiibo_link_config_start_page(const AmiToolApp* app) {
    if(!app || !app->tag_data || app->tag_data->pages_total < 4) {
        return 0;
    }
    return app->tag_data->pages_total - 4;
}

static void ami_tool_scene_amiibo_link_reset_config_tracking(AmiToolApp* app) {
    if(!app) {
        return;
    }
    app->amiibo_link_auth0_override_active = false;
    app->amiibo_link_pending_auth0 = 0xFF;
    app->amiibo_link_current_auth0 = 0xFF;
    app->amiibo_link_access_snapshot_valid = false;
    memset(app->amiibo_link_access_snapshot, 0, sizeof(app->amiibo_link_access_snapshot));

    if(app->tag_data && app->tag_data->pages_total >= 4) {
        size_t config_start = ami_tool_scene_amiibo_link_config_start_page(app);
        app->amiibo_link_current_auth0 = app->tag_data->page[config_start].data[3];
        memcpy(
            app->amiibo_link_access_snapshot,
            app->tag_data->page[config_start + 1].data,
            MF_ULTRALIGHT_PAGE_SIZE);
        app->amiibo_link_access_snapshot_valid = true;
    }
}

static void ami_tool_scene_amiibo_link_monitor_config(AmiToolApp* app) {
    if(!app || !app->tag_data || app->tag_data->pages_total < 4) {
        return;
    }

    /* Keep local tag cache synced with listener writes before inspecting config */
    ami_tool_scene_amiibo_link_sync_listener_data(app);

    size_t config_start = ami_tool_scene_amiibo_link_config_start_page(app);
    uint8_t* config_page = app->tag_data->page[config_start].data;
    uint8_t desired_auth0 = config_page[3];

    if(!app->amiibo_link_auth0_override_active) {
        if(desired_auth0 != app->amiibo_link_current_auth0) {
            if(desired_auth0 == 0xFF) {
                app->amiibo_link_current_auth0 = desired_auth0;
            } else {
                app->amiibo_link_pending_auth0 = desired_auth0;
                app->amiibo_link_auth0_override_active = true;
                app->amiibo_link_current_auth0 = 0xFF;
                config_page[3] = 0xFF;
            }
        }
    } else if(app->amiibo_link_access_snapshot_valid) {
        uint8_t* access_page = app->tag_data->page[config_start + 1].data;
        if(memcmp(
               access_page,
               app->amiibo_link_access_snapshot,
               MF_ULTRALIGHT_PAGE_SIZE) != 0) {
            memcpy(
                app->amiibo_link_access_snapshot,
                access_page,
                MF_ULTRALIGHT_PAGE_SIZE);
            config_page[3] = app->amiibo_link_pending_auth0;
            app->amiibo_link_current_auth0 = app->amiibo_link_pending_auth0;
            app->amiibo_link_auth0_override_active = false;
        }
    }
}

static void ami_tool_scene_amiibo_link_restore_pending_auth0(AmiToolApp* app) {
    if(!app || !app->tag_data || app->tag_data->pages_total < 4) {
        return;
    }
    if(!app->amiibo_link_auth0_override_active) {
        return;
    }
    size_t config_start = ami_tool_scene_amiibo_link_config_start_page(app);
    app->tag_data->page[config_start].data[3] = app->amiibo_link_pending_auth0;
    app->amiibo_link_current_auth0 = app->amiibo_link_pending_auth0;
    app->amiibo_link_auth0_override_active = false;
}

static void ami_tool_scene_amiibo_link_show_text(AmiToolApp* app, const char* message) {
    if(!app || !message) {
        return;
    }
    furi_string_set(app->text_box_store, message);
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    app->usage_info_visible = false;
}

static void ami_tool_scene_amiibo_link_show_ready(AmiToolApp* app) {
    if(!app) {
        return;
    }
    static const char* ready_text =
        "Blank Tag\n\n"
        "Flipper is emulating a blank NTAG215.\n"
        "Use a compatible app to write data.\n"
        "Press OK once writing is complete.\n"
        "Press Back to stop.";
    widget_reset(app->info_widget);
    widget_add_text_scroll_element(app->info_widget, 2, 0, 124, 60, ready_text);
    widget_add_button_element(
        app->info_widget,
        GuiButtonTypeCenter,
        "OK",
        ami_tool_scene_amiibo_link_ready_button_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewInfo);
    app->usage_info_visible = false;
}

static void ami_tool_scene_amiibo_link_ready_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    AmiToolApp* app = context;
    if(!app) {
        return;
    }
    if((result == GuiButtonTypeCenter) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, AmiToolEventAmiiboLinkWriteComplete);
    }
}

static bool ami_tool_scene_amiibo_link_regenerate_template(AmiToolApp* app) {
    if(!app || !app->tag_data || !app->tag_data_valid) {
        return false;
    }

    MfUltralightData* tag = app->tag_data;
    if(tag->pages_total < 8) {
        return false;
    }

    uint8_t* page2 = tag->page[2].data;
    page2[1] = 0x48;
    page2[2] = 0x0F;
    page2[3] = 0xE0;

    static const uint8_t capability_defaults[4] = {0xF1, 0x10, 0xFF, 0xEE};
    memcpy(tag->page[3].data, capability_defaults, sizeof(capability_defaults));

    tag->page[4].data[0] = 0xA5;

    if(tag->pages_total < 5) {
        return false;
    }
    static const uint8_t dynamic_lock_defaults[4] = {0x01, 0x00, 0x0F, 0xBD};
    size_t dynamic_lock_page = tag->pages_total - 5;
    memcpy(tag->page[dynamic_lock_page].data, dynamic_lock_defaults, sizeof(dynamic_lock_defaults));

    size_t config_start = ami_tool_scene_amiibo_link_config_start_page(app);
    if(config_start == 0 || (config_start + 3) >= tag->pages_total) {
        return false;
    }
    static const uint8_t cfg0[4] = {0x00, 0x00, 0x00, 0x04};
    static const uint8_t cfg1[4] = {0x5F, 0x00, 0x00, 0x00};
    memcpy(tag->page[config_start].data, cfg0, sizeof(cfg0));
    memcpy(tag->page[config_start + 1].data, cfg1, sizeof(cfg1));

    size_t uid_len = 0;
    const uint8_t* uid = mf_ultralight_get_uid(tag, &uid_len);
    MfUltralightAuthPassword password = {0};
    if(!ami_tool_compute_password_from_uid(uid, uid_len, &password)) {
        return false;
    }
    memcpy(tag->page[config_start + 2].data, password.data, sizeof(password.data));
    app->tag_password = password;
    app->tag_password_valid = true;

    static const uint8_t pack_defaults[4] = {0x80, 0x80, 0x00, 0x00};
    memcpy(tag->page[config_start + 3].data, pack_defaults, sizeof(pack_defaults));
    memcpy(app->tag_pack, pack_defaults, sizeof(app->tag_pack));
    app->tag_pack_valid = true;

    amiibo_configure_rf_interface(tag);
    return true;
}

static void ami_tool_scene_amiibo_link_capture_completion_marker(AmiToolApp* app) {
    if(!app || !app->tag_data) {
        return;
    }
    if(app->tag_data->pages_total <= AMI_TOOL_AMIIBO_LINK_COMPLETION_PAGE) {
        app->amiibo_link_completion_marker_valid = false;
        return;
    }
    memcpy(
        app->amiibo_link_completion_marker,
        app->tag_data->page[AMI_TOOL_AMIIBO_LINK_COMPLETION_PAGE].data,
        MF_ULTRALIGHT_PAGE_SIZE);
    app->amiibo_link_completion_marker_valid = true;
}

static bool ami_tool_scene_amiibo_link_marker_written(const AmiToolApp* app) {
    if(!app || !app->tag_data || !app->amiibo_link_completion_marker_valid) {
        return false;
    }
    if(app->tag_data->pages_total <= AMI_TOOL_AMIIBO_LINK_COMPLETION_PAGE) {
        return false;
    }
    return memcmp(
               app->tag_data->page[AMI_TOOL_AMIIBO_LINK_COMPLETION_PAGE].data,
               app->amiibo_link_completion_marker,
               MF_ULTRALIGHT_PAGE_SIZE) != 0;
}

static void ami_tool_scene_amiibo_link_sync_listener_data(AmiToolApp* app) {
    if(!app || !app->tag_data || !app->emulation_listener) {
        return;
    }
    const NfcDeviceData* device =
        nfc_listener_get_data(app->emulation_listener, NfcProtocolMfUltralight);
    if(!device) {
        return;
    }
    const MfUltralightData* listener_data = (const MfUltralightData*)device;
    mf_ultralight_copy(app->tag_data, listener_data);
}

static bool ami_tool_scene_amiibo_link_prepare(AmiToolApp* app) {
    if(!app || !app->tag_data) {
        return false;
    }

    ami_tool_clear_cached_tag(app);

    if(amiibo_prepare_blank_tag(app->tag_data) != RFIDX_OK) {
        return false;
    }

    app->tag_data_valid = true;

    size_t uid_len = 0;
    const uint8_t* uid = mf_ultralight_get_uid(app->tag_data, &uid_len);
    ami_tool_store_uid(app, uid, uid_len);

    static const uint8_t blank_password[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(app->tag_password.data, blank_password, sizeof(blank_password));
    app->tag_password_valid = true;

    if(uid && uid_len >= 7) {
        app->last_uid_valid = true;
        app->last_uid_len = uid_len;
    }

    static const uint8_t blank_pack[4] = {0x00, 0x00, 0x00, 0x00};
    memcpy(app->tag_pack, blank_pack, sizeof(app->tag_pack));
    app->tag_pack_valid = true;

    ami_tool_scene_amiibo_link_reset_config_tracking(app);
    ami_tool_scene_amiibo_link_capture_completion_marker(app);

    return true;
}

static bool ami_tool_scene_amiibo_link_start_session(AmiToolApp* app) {
    if(!ami_tool_scene_amiibo_link_prepare(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Blank Tag\n\nUnable to prepare memory.\nPress Back to exit.");
        app->amiibo_link_active = false;
        app->amiibo_link_waiting_for_completion = false;
        return false;
    }

    if(!ami_tool_info_start_emulation(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Blank Tag\n\nUnable to start emulation.\nPress Back to exit.");
        app->amiibo_link_active = false;
        app->amiibo_link_waiting_for_completion = false;
        return false;
    }

    app->amiibo_link_active = true;
    app->amiibo_link_waiting_for_completion = true;
    ami_tool_scene_amiibo_link_show_ready(app);
    return true;
}

static bool ami_tool_scene_amiibo_link_finalize(AmiToolApp* app) {
    if(!app || !app->tag_data || !app->tag_data_valid) {
        return false;
    }

    ami_tool_info_stop_emulation(app);
    amiibo_configure_rf_interface(app->tag_data);

    if(app->tag_data->pages_total < 2) {
        return false;
    }

    size_t password_page = app->tag_data->pages_total - 2;
    size_t pack_page = app->tag_data->pages_total - 1;
    memcpy(
        app->tag_password.data,
        app->tag_data->page[password_page].data,
        sizeof(app->tag_password.data));
    app->tag_password_valid = true;
    memcpy(app->tag_pack, app->tag_data->page[pack_page].data, sizeof(app->tag_pack));
    app->tag_pack_valid = true;

    size_t uid_len = 0;
    const uint8_t* uid = mf_ultralight_get_uid(app->tag_data, &uid_len);
    if(uid && uid_len > 0) {
        ami_tool_store_uid(app, uid, uid_len);
    }

    char id_hex[17] = {0};
    const char* id_arg = NULL;
    if(ami_tool_extract_amiibo_id(app->tag_data, id_hex, sizeof(id_hex))) {
        id_arg = id_hex;
    }

    ami_tool_info_show_page(app, id_arg, true);
    return true;
}

static void ami_tool_scene_amiibo_link_handle_completion(AmiToolApp* app) {
    if(!app || !app->amiibo_link_active || !app->amiibo_link_waiting_for_completion) {
        return;
    }

    ami_tool_scene_amiibo_link_sync_listener_data(app);

    if(!ami_tool_scene_amiibo_link_marker_written(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app,
            "Blank Tag\n\nNo data changes detected.\n"
            "Ensure the writing app finishes\nbefore pressing OK.");
        return;
    }

    app->amiibo_link_waiting_for_completion = false;
    ami_tool_scene_amiibo_link_restore_pending_auth0(app);

    if(!ami_tool_scene_amiibo_link_regenerate_template(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app,
            "Blank Tag\n\nUnable to rebuild Amiibo config.\n"
            "Press Back to exit.");
        ami_tool_info_stop_emulation(app);
        app->amiibo_link_active = false;
        return;
    }

    bool success = ami_tool_scene_amiibo_link_finalize(app);
    if(success) {
        app->amiibo_link_active = false;
        return;
    }

    const char* failure_template =
        "Blank Tag\n\nNo valid Amiibo data detected.\n"
        "Detected ID: %s\n"
        "Let the app finish writing and press OK again.";
    char id_hex[17] = {0};
    const char* detected_id = "Unknown";
    if(ami_tool_extract_amiibo_id(app->tag_data, id_hex, sizeof(id_hex))) {
        detected_id = id_hex;
    }
    FuriString* message = furi_string_alloc_set(failure_template);
    furi_string_replace_all_str(message, "%s", detected_id);
    ami_tool_scene_amiibo_link_show_text(app, furi_string_get_cstr(message));
    furi_string_free(message);
    app->amiibo_link_active = false;

    if(!ami_tool_scene_amiibo_link_start_session(app)) {
        ami_tool_scene_amiibo_link_show_text(
            app, "Blank Tag\n\nUnable to restart emulation.\nPress Back to exit.");
    }
}

void ami_tool_scene_amiibo_link_on_enter(void* context) {
    AmiToolApp* app = context;
    ami_tool_scene_amiibo_link_start_session(app);
}

bool ami_tool_scene_amiibo_link_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AmiToolEventAmiiboLinkWriteComplete:
            ami_tool_scene_amiibo_link_handle_completion(app);
            return true;
        case AmiToolEventInfoShowActions:
            ami_tool_info_show_actions_menu(app);
            return true;
        case AmiToolEventInfoActionEmulate:
            if(!ami_tool_info_start_emulation(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to start emulation.\nRead or generate an Amiibo first.");
            }
            return true;
        case AmiToolEventInfoActionUsage:
            ami_tool_info_show_usage(app);
            return true;
        case AmiToolEventInfoActionChangeUid:
            if(!ami_tool_info_change_uid(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to change UID.\nInstall key_retail.bin and try again.");
            }
            return true;
        case AmiToolEventInfoActionWriteTag:
            if(!ami_tool_info_write_to_tag(app)) {
                ami_tool_info_show_action_message(
                    app,
                    "Unable to write tag.\nUse a blank NTAG215 and make\nsure key_retail.bin is installed.");
            }
            return true;
        case AmiToolEventInfoActionSaveToStorage:
            if(!ami_tool_info_save_to_storage(app)) {
                ami_tool_info_show_action_message(
                    app,
                    "Unable to save Amiibo.\nRead or generate one first\nand check storage access.");
            }
            return true;
        case AmiToolEventUsagePrevPage:
            ami_tool_info_navigate_usage(app, -1);
            return true;
        case AmiToolEventUsageNextPage:
            ami_tool_info_navigate_usage(app, 1);
            return true;
        case AmiToolEventInfoWriteStarted:
        case AmiToolEventInfoWriteSuccess:
        case AmiToolEventInfoWriteFailed:
        case AmiToolEventInfoWriteCancelled:
            ami_tool_info_handle_write_event(app, event.event);
            return true;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->amiibo_link_active && app->amiibo_link_waiting_for_completion) {
            ami_tool_scene_amiibo_link_monitor_config(app);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->write_in_progress) {
            if(app->write_waiting_for_tag) {
                ami_tool_info_request_write_cancel(app);
            }
            return true;
        }
        if(app->info_emulation_active) {
            ami_tool_info_stop_emulation(app);
            if(app->amiibo_link_active) {
                app->amiibo_link_active = false;
                app->amiibo_link_waiting_for_completion = false;
                ami_tool_scene_amiibo_link_restore_pending_auth0(app);
                return false;
            }
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->usage_info_visible) {
            app->usage_info_visible = false;
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_action_message_visible) {
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_actions_visible) {
            app->info_actions_visible = false;
            ami_tool_info_refresh_current_page(app);
            return true;
        }
        app->amiibo_link_active = false;
        app->amiibo_link_waiting_for_completion = false;
        ami_tool_scene_amiibo_link_restore_pending_auth0(app);
        ami_tool_info_stop_emulation(app);
        return false;
    }

    return false;
}

void ami_tool_scene_amiibo_link_on_exit(void* context) {
    AmiToolApp* app = context;
    ami_tool_info_stop_emulation(app);
    ami_tool_info_abort_write(app);
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    app->amiibo_link_active = false;
    app->amiibo_link_waiting_for_completion = false;
}
