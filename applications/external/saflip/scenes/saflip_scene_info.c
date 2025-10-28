#include "../saflip.h"

static const char* card_levels[] = {
    "Guest Key",
    "Connectors",
    "Suite",
    "Limited Use",
    "Failsafe",
    "Inhibit",
    "Pool/Meeting Master",
    "Housekeeping",
    "Floor Key",
    "Section Key",
    "Rooms Master",
    "Grand Master",
    "Emergency",
    "Electronic Lockout",
    "Secondary Programming Key",
    "Primary Programming Key",
};

static const char* card_types[] = {
    "Standard Key",
    "Resequencing Key",
    "Block Key",
    "Unblock Key",
    "Change Checkout Date",
    "Check Out a Key",
    "Cancel Prereg Key",
    "Check In Prereg Key",
    "Cancel-A-Key-ID",
    "Change Room Key",
};

static const char* ppk_card_types[] = {
    "Standard Key",
    "LED Diagnostic",
    "Dis/Enable E2 Changes",
    "Erase Lock (E2) Memory",
    "Battery Disconect",
    "Display Key",
    "Card Type: 6",
    "Card Type: 7",
    "Card Type: 8",
    "Card Type: 9",
};

void saflip_scene_info_widget_callback(GuiButtonType button, InputType type, void* context) {
    SaflipApp* app = context;

    if(button == GuiButtonTypeLeft) {
        scene_manager_set_scene_state(app->scene_manager, SaflipSceneVariableKeys, 0);
        scene_manager_next_scene(app->scene_manager, SaflipSceneVariableKeys);
    } else if(button == GuiButtonTypeCenter) {
        scene_manager_set_scene_state(app->scene_manager, SaflipSceneLog, 0);
        scene_manager_next_scene(app->scene_manager, SaflipSceneLog);
    } else if(button == GuiButtonTypeRight) {
        scene_manager_set_scene_state(app->scene_manager, SaflipSceneOptions, 0);
        scene_manager_next_scene(app->scene_manager, SaflipSceneOptions);
    }

    UNUSED(type);
}

void saflip_scene_info_on_enter(void* context) {
    SaflipApp* app = context;

    widget_reset(app->widget);
    FuriString* temp_str = furi_string_alloc();

    // Show card format in bold in top-left corner
    switch(app->data->format) {
    case SaflipFormatMifareClassic:
        furi_string_printf(temp_str, "MFC");
        break;
    default:
        furi_string_printf(temp_str, "Unknown Format");
        break;
    }
    widget_add_string_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, furi_string_get_cstr(temp_str));

    // Show UID on the same line, but not bold and on the right
    furi_string_reset(temp_str);
    for(size_t i = 0; i < app->uid_len; i++) {
        if(i) furi_string_cat_printf(temp_str, ":");
        furi_string_cat_printf(temp_str, "%02X", app->uid[i]);
    }
    widget_add_string_element(
        app->widget, 128, 1, AlignRight, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    // Show fields below
    furi_string_reset(temp_str);

    if(app->data->card_level >= COUNT_OF(card_levels))
        furi_string_cat_printf(temp_str, "Card Level: %d\n", app->data->card_level);
    else
        furi_string_cat_printf(temp_str, "%s\n", card_levels[app->data->card_level]);

    if(app->data->card_type >= COUNT_OF(card_types)) {
        furi_string_cat_printf(temp_str, "Card Type: %d\n", app->data->card_type);
    } else {
        if(app->data->card_level == 15)
            furi_string_cat_printf(temp_str, "%s\n", ppk_card_types[app->data->card_type]);
        else
            furi_string_cat_printf(temp_str, "%s\n", card_types[app->data->card_type]);
    }

    furi_string_cat_printf(temp_str, "Property Number: %d\n", app->data->property_id);

    furi_string_cat_printf(temp_str, "Card ID: %d\n", app->data->card_id);
    furi_string_cat_printf(temp_str, "Opening Key: %s\n", app->data->opening_key ? "Yes" : "No");
    furi_string_cat_printf(temp_str, "Lock ID: %d\n", app->data->lock_id);
    furi_string_cat_printf(temp_str, "Pass #/Areas: %d\n", app->data->pass_number);

    furi_string_cat_printf(
        temp_str, "Seq. & Combination: %d\n", app->data->sequence_and_combination);
    furi_string_cat_printf(
        temp_str, "Override Deadbolt: %s\n", app->data->deadbolt_override ? "Yes" : "No");

    furi_string_cat_printf(
        temp_str,
        "Can be used: %c %c %c %c %c %c %c\n",
        (app->data->restricted_days & 0b0000001) ? '-' : 'S',
        (app->data->restricted_days & 0b0000010) ? '-' : 'M',
        (app->data->restricted_days & 0b0000100) ? '-' : 'T',
        (app->data->restricted_days & 0b0001000) ? '-' : 'W',
        (app->data->restricted_days & 0b0010000) ? '-' : 'T',
        (app->data->restricted_days & 0b0100000) ? '-' : 'F',
        (app->data->restricted_days & 0b1000000) ? '-' : 'S');

    furi_string_cat_printf(
        temp_str,
        "Valid: %04d-%02d-%02d %02d:%02d\n",
        app->data->creation.year,
        app->data->creation.month,
        app->data->creation.day,
        app->data->creation.hour,
        app->data->creation.minute);
    furi_string_cat_printf(
        temp_str,
        "Expires: %04d-%02d-%02d %02d:%02d\n",
        app->data->expire.year,
        app->data->expire.month,
        app->data->expire.day,
        app->data->expire.hour,
        app->data->expire.minute);

    furi_string_cat_printf(
        temp_str,
        "Contains %d variable %s\n",
        app->variable_keys,
        app->variable_keys == 1 ? "key" : "keys");
    furi_string_cat_printf(
        temp_str,
        "Contains %d log %s",
        app->log_entries,
        app->log_entries == 1 ? "entry" : "entries");

    widget_add_text_scroll_element(app->widget, 0, 10, 128, 40, furi_string_get_cstr(temp_str));

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "V.Keys", saflip_scene_info_widget_callback, app);
    if(app->log_entries)
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Log", saflip_scene_info_widget_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Options", saflip_scene_info_widget_callback, app);

    furi_string_free(temp_str);
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewWidget);
}

bool saflip_scene_info_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // We don't want to go back to the Read Card scene
        uint32_t scenes[] = {
            SaflipSceneEdit,
            SaflipSceneFileSelect,
            SaflipSceneStart,
        };

        consumed = scene_manager_search_and_switch_to_previous_scene_one_of(
            app->scene_manager, scenes, COUNT_OF(scenes));
    }

    return consumed;
}

void saflip_scene_info_on_exit(void* context) {
    SaflipApp* app = context;
    widget_reset(app->widget);
}
