#include "updi_app_i.h"
#include "updi_scene.h"

#include <dialogs/dialogs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool updi_confirm(UpdiApp* app, const char* header, const char* text, const char* center) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "Cancel", center, NULL);
    DialogMessageButton button = dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
    return button == DialogMessageButtonCenter;
}

static void updi_start_work(UpdiApp* app, UpdiOp op) {
    app->op = op;
    scene_manager_next_scene(app->scene_manager, UpdiSceneWork);
}

static void updi_menu_callback(void* context, uint32_t index) {
    UpdiApp* app = context;

    if(index == UpdiMenuPins) {
        scene_manager_next_scene(app->scene_manager, UpdiScenePins);
        return;
    }
    if(index == UpdiMenuWiring) {
        scene_manager_next_scene(app->scene_manager, UpdiSceneWiring);
        return;
    }
    if(index == UpdiMenuAbout) {
        scene_manager_next_scene(app->scene_manager, UpdiSceneAbout);
        return;
    }
    if(index == UpdiMenuProgrammer) {
        scene_manager_next_scene(app->scene_manager, UpdiSceneProgrammer);
        return;
    }
    if(index == UpdiMenuDetect) {
        app->op = UpdiOpDetect;
        scene_manager_next_scene(app->scene_manager, UpdiSceneDetect);
        return;
    }
    if(index == UpdiMenuFuses) {
        updi_start_work(app, UpdiOpFuses);
        return;
    }
    if(index == UpdiMenuWriteFuse) {
        scene_manager_next_scene(app->scene_manager, UpdiSceneFuseWrite);
        return;
    }
    if(index == UpdiMenuDump) {
        scene_manager_next_scene(app->scene_manager, UpdiSceneDumpName);
        return;
    }
    if(index == UpdiMenuReadEeprom) {
        updi_start_work(app, UpdiOpReadEeprom);
        return;
    }

    if(index == UpdiMenuFlash || index == UpdiMenuWriteEeprom) {
        DialogsFileBrowserOptions options;
        dialog_file_browser_set_basic_options(&options, "hex", NULL);
        options.base_path = "/ext";
        FuriString* start = furi_string_alloc_set("/ext");
        FuriString* dest = index == UpdiMenuFlash ? app->hex_path : app->eeprom_path;
        bool picked = dialog_file_browser_show(app->dialogs, dest, start, &options);
        furi_string_free(start);
        if(!picked) return;

        if(index == UpdiMenuFlash) {
            if(!updi_confirm(
                   app,
                   "Erase and flash?",
                   "Chip erase clears flash.\nEEPROM clears unless\nEESAVE is set.",
                   "Flash")) {
                return;
            }
            updi_start_work(app, UpdiOpFlash);
        } else {
            if(!updi_confirm(app, "Write EEPROM?", "Writes HEX to EEPROM.", "Write")) {
                return;
            }
            updi_start_work(app, UpdiOpWriteEeprom);
        }
    }
}

void updi_scene_menu_on_enter(void* context) {
    UpdiApp* app = context;
    submenu_reset(app->submenu);
    submenu_add_item(app->submenu, "Detect chip", UpdiMenuDetect, updi_menu_callback, app);
    submenu_add_item(app->submenu, "Dump AVR", UpdiMenuDump, updi_menu_callback, app);
    submenu_add_item(app->submenu, "Flash HEX", UpdiMenuFlash, updi_menu_callback, app);
    submenu_add_item(app->submenu, "Read EEPROM", UpdiMenuReadEeprom, updi_menu_callback, app);
    submenu_add_item(app->submenu, "Write EEPROM", UpdiMenuWriteEeprom, updi_menu_callback, app);
    submenu_add_item(app->submenu, "Read fuses", UpdiMenuFuses, updi_menu_callback, app);
    submenu_add_item(app->submenu, "Write fuse", UpdiMenuWriteFuse, updi_menu_callback, app);
    submenu_add_item(
        app->submenu, "SerialUPDI (USB)", UpdiMenuProgrammer, updi_menu_callback, app);
    snprintf(app->pins_label, sizeof(app->pins_label), "Pins %s", updi_pins_label(app->pins));
    submenu_add_item(app->submenu, app->pins_label, UpdiMenuPins, updi_menu_callback, app);
    submenu_add_item(app->submenu, "Wiring", UpdiMenuWiring, updi_menu_callback, app);
    submenu_add_item(app->submenu, "About", UpdiMenuAbout, updi_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewSubmenu);
}

bool updi_scene_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void updi_scene_menu_on_exit(void* context) {
    UpdiApp* app = context;
    submenu_reset(app->submenu);
}

static void updi_pins_changed(VariableItem* item) {
    UpdiApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->pins = index == 1 ? UpdiPins13_14 : UpdiPins15_16;
    variable_item_set_current_value_text(item, updi_pins_label(app->pins));
    updi_pins_save(app);
}

void updi_scene_pins_on_enter(void* context) {
    UpdiApp* app = context;
    variable_item_list_reset(app->var_list);
    VariableItem* item = variable_item_list_add(app->var_list, "TX/RX", 2, updi_pins_changed, app);
    uint8_t index = app->pins == UpdiPins13_14 ? 1 : 0;
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, updi_pins_label(app->pins));
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewVarList);
}

bool updi_scene_pins_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void updi_scene_pins_on_exit(void* context) {
    UpdiApp* app = context;
    variable_item_list_reset(app->var_list);
}

void updi_scene_wiring_on_enter(void* context) {
    UpdiApp* app = context;
    widget_reset(app->widget);
    const char* text = app->pins == UpdiPins13_14 ? "13 TX - 1k - UPDI\n"
                                                    "14 RX ------ UPDI\n"
                                                    "GND pin --- GND\n"
                                                    "Or use level-shift\n"
                                                    "companion board.\n"
                                                    "No 12V HV-UPDI." :
                                                    "15 TX - 1k - UPDI\n"
                                                    "16 RX ------ UPDI\n"
                                                    "GND pin --- GND\n"
                                                    "Or use level-shift\n"
                                                    "companion board.\n"
                                                    "No 12V HV-UPDI.";
    widget_add_string_multiline_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontSecondary, text);
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewWidget);
}

bool updi_scene_wiring_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void updi_scene_wiring_on_exit(void* context) {
    UpdiApp* app = context;
    widget_reset(app->widget);
}

void updi_scene_about_on_enter(void* context) {
    UpdiApp* app = context;
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget,
        0,
        0,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "AVR UPDI v0.2\n"
        "UPDI programmer\n"
        "for Flipper Zero.\n"
        "\n"
        "HW breakout:\n"
        "github.com/gztproject/\n"
        "AVR_programmer_debugger\n"
        "\n"
        "Not ISP — use\n"
        "avr_isp for SPI.");
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewWidget);
}

bool updi_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void updi_scene_about_on_exit(void* context) {
    UpdiApp* app = context;
    widget_reset(app->widget);
}

void updi_scene_detect_on_enter(void* context) {
    UpdiApp* app = context;
    app->op = UpdiOpDetect;
    updi_scene_work_on_enter(app);
}

bool updi_scene_detect_on_event(void* context, SceneManagerEvent event) {
    return updi_scene_work_on_event(context, event);
}

void updi_scene_detect_on_exit(void* context) {
    updi_scene_work_on_exit(context);
}

static void updi_dump_name_callback(void* context) {
    UpdiApp* app = context;
    if(app->dump_name[0] == '\0') {
        snprintf(app->dump_name, sizeof(app->dump_name), "dump");
    }
    /* Sanitize path segment. */
    for(char* p = app->dump_name; *p; p++) {
        if(!(*p == '-' || *p == '_' || (*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') ||
             (*p >= 'a' && *p <= 'z'))) {
            *p = '_';
        }
    }
    updi_start_work(app, UpdiOpDump);
}

void updi_scene_dump_name_on_enter(void* context) {
    UpdiApp* app = context;
    if(app->dump_name[0] == '\0') {
        snprintf(app->dump_name, sizeof(app->dump_name), "dump");
    }
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Dump folder name");
    text_input_set_result_callback(
        app->text_input,
        updi_dump_name_callback,
        app,
        app->dump_name,
        sizeof(app->dump_name),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewTextInput);
}

bool updi_scene_dump_name_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void updi_scene_dump_name_on_exit(void* context) {
    UpdiApp* app = context;
    text_input_reset(app->text_input);
}

static void updi_fuse_input_callback(void* context) {
    UpdiApp* app = context;
    unsigned index = 0;
    unsigned value = 0;
    if(sscanf(app->fuse_input, "%u %x", &index, &value) != 2 &&
       sscanf(app->fuse_input, "%u %X", &index, &value) != 2) {
        return;
    }
    if(index > 255 || value > 255) return;
    app->fuse_index = (uint8_t)index;
    app->fuse_value = (uint8_t)value;
    if(!updi_confirm(app, "Write fuse?", "Wrong fuses can\nbrick the part.", "Write")) {
        return;
    }
    if(!updi_confirm(app, "Confirm again?", "This cannot be undone\neasily.", "Yes")) {
        return;
    }
    updi_start_work(app, UpdiOpWriteFuse);
}

void updi_scene_fuse_write_on_enter(void* context) {
    UpdiApp* app = context;
    snprintf(app->fuse_input, sizeof(app->fuse_input), "5 00");
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Index hex (e.g. 5 C0)");
    text_input_set_result_callback(
        app->text_input,
        updi_fuse_input_callback,
        app,
        app->fuse_input,
        sizeof(app->fuse_input),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewTextInput);
}

bool updi_scene_fuse_write_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void updi_scene_fuse_write_on_exit(void* context) {
    UpdiApp* app = context;
    text_input_reset(app->text_input);
}

void updi_scene_work_on_enter(void* context) {
    UpdiApp* app = context;
    app->cancel = false;
    app->showing_result = false;
    app->percent = 0;
    snprintf(app->status, sizeof(app->status), "Connecting");
    app->result[0] = '\0';

    with_view_model(
        app->progress,
        UpdiProgressModel * model,
        {
            model->percent = 0;
            snprintf(model->status, sizeof(model->status), "Connecting");
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewProgress);
    app->thread = furi_thread_alloc_ex("UpdiWorker", 8 * 1024, updi_worker, app);
    furi_thread_start(app->thread);
}

bool updi_scene_work_on_event(void* context, SceneManagerEvent event) {
    UpdiApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->showing_result) return false;
        app->cancel = true;
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == UpdiEventProgress) {
        char status[40];
        uint8_t percent = 0;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        snprintf(status, sizeof(status), "%s", app->status);
        percent = app->percent;
        furi_mutex_release(app->mutex);
        with_view_model(
            app->progress,
            UpdiProgressModel * model,
            {
                model->percent = percent;
                snprintf(model->status, sizeof(model->status), "%s", status);
            },
            true);
        return true;
    }

    if(event.event == UpdiEventFinished) {
        text_box_set_text(app->text_box, app->result);
        text_box_set_focus(app->text_box, TextBoxFocusStart);
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewText);
        app->showing_result = true;
        return true;
    }

    return false;
}

void updi_scene_work_on_exit(void* context) {
    UpdiApp* app = context;
    app->cancel = true;
    if(app->thread != NULL) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }
    text_box_reset(app->text_box);
}

void (*const updi_on_enter_handlers[])(void*) = {
    updi_scene_menu_on_enter,
    updi_scene_pins_on_enter,
    updi_scene_wiring_on_enter,
    updi_scene_about_on_enter,
    updi_scene_detect_on_enter,
    updi_scene_dump_name_on_enter,
    updi_scene_fuse_write_on_enter,
    updi_usb_bridge_scene_on_enter,
    updi_scene_work_on_enter,
};

bool (*const updi_on_event_handlers[])(void*, SceneManagerEvent) = {
    updi_scene_menu_on_event,
    updi_scene_pins_on_event,
    updi_scene_wiring_on_event,
    updi_scene_about_on_event,
    updi_scene_detect_on_event,
    updi_scene_dump_name_on_event,
    updi_scene_fuse_write_on_event,
    updi_usb_bridge_scene_on_event,
    updi_scene_work_on_event,
};

void (*const updi_on_exit_handlers[])(void*) = {
    updi_scene_menu_on_exit,
    updi_scene_pins_on_exit,
    updi_scene_wiring_on_exit,
    updi_scene_about_on_exit,
    updi_scene_detect_on_exit,
    updi_scene_dump_name_on_exit,
    updi_scene_fuse_write_on_exit,
    updi_usb_bridge_scene_on_exit,
    updi_scene_work_on_exit,
};

const SceneManagerHandlers updi_scene_handlers = {
    .on_enter_handlers = updi_on_enter_handlers,
    .on_event_handlers = updi_on_event_handlers,
    .on_exit_handlers = updi_on_exit_handlers,
    .scene_num = UpdiSceneCount,
};
