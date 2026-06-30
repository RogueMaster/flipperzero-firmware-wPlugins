#include "edit_callbacks.h"
#include "cards_scene.h"
#include "../../storage/nfc_login_card_storage.h"
#include "../scene_manager.h"
#include <ctype.h>

void app_edit_text_result_callback(void* context) {
    App* app = context;
    if(app->edit_state == EditStateName || app->edit_state == EditStatePassword) {
        if(app_save_cards(app)) {
            notification_message(app->notification, &sequence_success);
        } else {
            notification_message(app->notification, &sequence_error);
            FURI_LOG_E(TAG, "Failed to save card after edit");
        }
        app->edit_state = EditStateNone;
        // Return to edit menu
        app->widget_state = 3;
        app_render_edit_menu(app);
        app_switch_to_view(app, ViewWidget);
    } else if(app->edit_state == EditStateUid) {
        // Parse hex string from app->edit_uid_text into UID bytes
        const char* p = app->edit_uid_text;
        uint8_t uid[MAX_UID_LEN] = {0};
        size_t idx = 0;
        // skip spaces and parse two hex chars per byte
        while(*p && idx < MAX_UID_LEN) {
            while(*p == ' ') p++;
            if(!isxdigit((unsigned char)p[0])) break;
            unsigned int byte_val = 0;
            if(sscanf(p, "%2x", &byte_val) == 1) {
                uid[idx++] = (uint8_t)byte_val;
                // advance over two hex digits if present
                p++;
                if(isxdigit((unsigned char)*p)) p++;
            } else {
                break;
            }
            while(*p == ' ') p++;
        }
        if(idx > 0) {
            app->cards[app->edit_card_index].uid_len = idx;
            memcpy(app->cards[app->edit_card_index].uid, uid, idx);
            if(app_save_cards(app)) {
                notification_message(app->notification, &sequence_success);
            } else {
                notification_message(app->notification, &sequence_error);
                FURI_LOG_E(TAG, "Failed to save card after UID edit");
            }
        } else {
            notification_message(app->notification, &sequence_error);
        }
        app->edit_state = EditStateNone;
        // Return to edit menu
        app->widget_state = 3;
        widget_reset(app->widget);
        widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Edit Card");
        const char* items[] = {"Name", "Password", "UID", "Delete"};
        for(size_t i = 0; i < 4; i++) {
            char line[32];
            snprintf(line, sizeof(line), "%s %s", (i == app->edit_menu_index) ? ">" : " ", items[i]);
            widget_add_string_element(app->widget, 0, 12 + i * 12, AlignLeft, AlignTop, FontSecondary, line);
        }
        app_switch_to_view(app, ViewWidget);
    }
}

void app_edit_uid_byte_input_done(void* context) {
    App* app = context;
    // Use the length that was set when opening the byte input (app->edit_uid_len)
    // This is the actual UID length, not MAX_UID_LEN
    size_t actual_len = app->edit_uid_len;
    
    if(app->edit_card_index < app->card_count && actual_len > 0 && actual_len <= MAX_UID_LEN) {
        app->cards[app->edit_card_index].uid_len = actual_len;
        memcpy(app->cards[app->edit_card_index].uid, app->edit_uid_bytes, actual_len);
        if(app_save_cards(app)) {
            notification_message(app->notification, &sequence_success);
        } else {
            notification_message(app->notification, &sequence_error);
            FURI_LOG_E(TAG, "Failed to save card after UID byte edit");
        }
    } else {
        notification_message(app->notification, &sequence_error);
    }
    app->edit_state = EditStateNone;
    // Return to edit menu
    app->widget_state = 3;
    app_render_edit_menu(app);
    app_switch_to_view(app, ViewWidget);
}