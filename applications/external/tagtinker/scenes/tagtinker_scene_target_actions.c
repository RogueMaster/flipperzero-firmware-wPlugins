/*
 * Target actions scene.
 */

#include "../tagtinker_app.h"
#include <dialogs/dialogs.h>

static void target_actions_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool confirm_target_action(TagTinkerApp* app, const char* header, const char* body, const char* action) {
    if(!app || !header || !body || !action) return false;

    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(message, body, 64, 18, AlignCenter, AlignTop);
    dialog_message_set_buttons(message, "Back", NULL, action);
    DialogMessageButton button = dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
    return button == DialogMessageButtonRight;
}

static void show_target_action_result(TagTinkerApp* app, const char* header, const char* body) {
    if(!app || !header || !body) return;

    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(message, body, 64, 18, AlignCenter, AlignTop);
    dialog_message_set_buttons(message, "OK", NULL, NULL);
    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

static void show_target_details(TagTinkerApp* app, const TagTinkerTarget* target) {
    if(!target) return;

    char body[160];
    if(target->profile.known && target->profile.width && target->profile.height) {
        snprintf(
            body,
            sizeof(body),
            "Type: %u\nKind: %s\nSize: %ux%u\nColor: %s",
            target->profile.type_code,
            tagtinker_profile_kind_label(target->profile.kind),
            target->profile.width,
            target->profile.height,
            tagtinker_profile_color_label(target->profile.color));
    } else if(target->profile.known) {
        snprintf(
            body,
            sizeof(body),
            "Type: %u\nKind: %s\nColor: %s",
            target->profile.type_code,
            tagtinker_profile_kind_label(target->profile.kind),
            tagtinker_profile_color_label(target->profile.color));
    } else {
        snprintf(body, sizeof(body), "Type: %u\nProfile: Unknown", target->profile.type_code);
    }

    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "Tag Info", 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(message, body, 64, 18, AlignCenter, AlignTop);
    dialog_message_set_buttons(message, "OK", NULL, NULL);
    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

void tagtinker_scene_target_actions_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    TagTinkerTarget* target = (app->selected_target >= 0) ? &app->targets[app->selected_target] : NULL;
    bool allow_graphics = tagtinker_target_supports_graphics(target);

    submenu_reset(app->submenu);

    char header[24];
    snprintf(
        header,
        sizeof(header),
        "%s",
        (target && target->name[0]) ? target->name : "Target");
    submenu_set_header(app->submenu, header);
    submenu_add_item(app->submenu, "Show Tag Info", TagTinkerTargetDetails, target_actions_cb, app);
    submenu_add_item(app->submenu, "Rename Tag", TagTinkerTargetRename, target_actions_cb, app);

    if(allow_graphics) {
        submenu_add_item(app->submenu, "Set Text", TagTinkerTargetPushText, target_actions_cb, app);
        submenu_add_item(app->submenu, "Set Image", TagTinkerTargetPushSyncedImage, target_actions_cb, app);
    }

    submenu_add_item(app->submenu, "LED Test", TagTinkerTargetPingFlash, target_actions_cb, app);
    submenu_add_item(
        app->submenu, "Delete Saved Images", TagTinkerTargetDeleteSyncedImages, target_actions_cb, app);
    submenu_add_item(app->submenu, "Delete Tag", TagTinkerTargetDeleteTag, target_actions_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewSubmenu);
}

bool tagtinker_scene_target_actions_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case TagTinkerTargetDetails:
        show_target_details(app, &app->targets[app->selected_target]);
        return true;
    case TagTinkerTargetRename:
        scene_manager_set_scene_state(
            app->scene_manager, TagTinkerSceneTextInput, TagTinkerTextInputRenameTarget);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTextInput);
        return true;
    case TagTinkerTargetPushText:
        if(!tagtinker_target_supports_graphics(&app->targets[app->selected_target])) return true;
        scene_manager_next_scene(app->scene_manager, TagTinkerScenePresetList);
        return true;
    case TagTinkerTargetPushSyncedImage:
        if(!tagtinker_target_supports_graphics(&app->targets[app->selected_target])) return true;
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneSyncedImageList);
        return true;
    case TagTinkerTargetDeleteSyncedImages:
        {
            TagTinkerTarget* target = &app->targets[app->selected_target];
            char body[96];
            snprintf(body, sizeof(body), "Remove saved images for\n%s?", target->name);
            if(!confirm_target_action(app, "Delete Images", body, "Delete")) {
                return true;
            }

            size_t removed = tagtinker_delete_synced_images_for_barcode(app, target->barcode);
            char result[96];
            snprintf(result, sizeof(result), "Removed %u saved image%s", (unsigned)removed, removed == 1U ? "" : "s");
            show_target_action_result(app, "Delete Images", result);
        }
        return true;
    case TagTinkerTargetPingFlash:
        {
            TagTinkerTarget* target = &app->targets[app->selected_target];

            app->frame_seq_count = 2;
            app->frame_sequence = malloc(sizeof(uint8_t*) * 2);
            app->frame_lengths  = malloc(sizeof(size_t) * 2);
            app->frame_repeats  = malloc(sizeof(uint16_t) * 2);

            app->frame_sequence[0] = malloc(TAGTINKER_MAX_FRAME_SIZE);
            app->frame_lengths[0] = tagtinker_make_ping_frame(app->frame_sequence[0], target->plid);
            app->frame_repeats[0] = 500;

            app->frame_sequence[1] = malloc(TAGTINKER_MAX_FRAME_SIZE);
            const uint8_t blink_payload[6] = {0x06, 0xC9, 0x00, 0x00, 0x00, 0x00};
            app->frame_lengths[1] = tagtinker_make_addressed_frame(
                app->frame_sequence[1], target->plid, blink_payload, 6);
            app->frame_repeats[1] = 100;

            memcpy(app->frame_buf, app->frame_sequence[0], app->frame_lengths[0]);
            app->frame_len = app->frame_lengths[0];

            app->tx_spam = false;
            scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
        }
        return true;
    case TagTinkerTargetDeleteTag:
        {
            TagTinkerTarget* target = &app->targets[app->selected_target];
            char body[96];
            snprintf(body, sizeof(body), "Delete %s and its\nsaved images?", target->name);
            if(!confirm_target_action(app, "Delete Tag", body, "Delete")) {
                return true;
            }

            tagtinker_delete_target(app, (uint8_t)app->selected_target);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, TagTinkerSceneTargetMenu);
        }
        return true;
    }
    return false;
}

void tagtinker_scene_target_actions_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    submenu_reset(app->submenu);
}
