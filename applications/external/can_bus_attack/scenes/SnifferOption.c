
#include "../app_user.h"
#include <string.h>

bool condition = true;
bool wait_to_be_set = false;

void set_filter_sniffing(MCP2515* CAN, uint32_t id);
void restart_filtering(MCP2515* CAN);
static int32_t worker_sniffing(void* context);

void draw_box_text(App* app);

void set_filter_sniffing(MCP2515* CAN, uint32_t id) {
    uint32_t mask = 0x7FF;

    if(id > mask) {
        mask = 0x1FFFFFFF;
    }

    init_mask(CAN, 0, mask);
    init_mask(CAN, 1, mask);

    init_filter(CAN, 0, id);
    init_filter(CAN, 1, id);
    init_filter(CAN, 2, id);
    init_filter(CAN, 3, id);
    init_filter(CAN, 4, id);
    init_filter(CAN, 5, id);
}

void restart_filtering(MCP2515* CAN) {
    init_mask(CAN, 0, 0);
    init_mask(CAN, 1, 0);

    init_filter(CAN, 0, 0);
    init_filter(CAN, 1, 0);
    init_filter(CAN, 2, 0);
    init_filter(CAN, 3, 0);
    init_filter(CAN, 4, 0);
    init_filter(CAN, 5, 0);
}

static void canid_context_menu_callback(void* context, uint32_t action_index) {
    App* app = context;
    switch(action_index) {
    case 0: // See values
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_history_view, app->frameArray[app->sniffer_index].canId);
        scene_manager_next_scene(app->scene_manager, app_scene_history_view);
        break;
    case 1: // Save
        app->replay_frame = app->frameArray[app->sniffer_index];
        dialog_message_show_storage_error(app->dialogs, "Message saved \non memory.");
        break;
    default:
        break;
    }
}

static void sniffer_menu_callback(void* context, uint32_t index) {
    App* app = context;
    app->sniffer_index = index;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Actions");
    submenu_add_item(app->submenu, "See values", 0, canid_context_menu_callback, app);
    submenu_add_item(app->submenu, "Save", 1, canid_context_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

void app_scene_sniffing_on_enter(void* context) {
    App* app = context;
    if(condition) {
        app->thread = furi_thread_alloc_ex("SniffingWork", 1024, worker_sniffing, app);
        furi_thread_start(app->thread);
        submenu_reset(app->submenu);
        submenu_set_header(app->submenu, "CANBUS MESSAGES");
    }
    condition = true;

    if(scene_manager_get_scene_state(app->scene_manager, app_scene_sniffing_option) == 1) {
        scene_manager_previous_scene(app->scene_manager);
        scene_manager_set_scene_state(app->scene_manager, app_scene_sniffing_option, 0);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

bool app_scene_sniffing_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case EntryEvent:
            condition = false;
            wait_to_be_set = true;
            scene_manager_next_scene(app->scene_manager, app_scene_box_sniffing);
            consumed = true;
            break;
        case DEVICE_NO_CONNECTED:
            scene_manager_set_scene_state(app->scene_manager, app_scene_sniffing_option, 1);
            scene_manager_next_scene(app->scene_manager, app_scene_device_no_connected);
            consumed = true;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void app_scene_sniffing_on_exit(void* context) {
    App* app = context;
    if(condition) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        submenu_reset(app->submenu);
    }
}

void draw_box_text(App* app) {
    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "ADDR: %lx DLC: %u\n",
        app->frameArray[app->sniffer_index].canId,
        app->frameArray[app->sniffer_index].data_lenght);

    for(uint8_t i = 0; i < app->frameArray[app->sniffer_index].data_lenght; i++) {
        furi_string_cat_printf(
            app->text, "[%u]:  %02X ", i, app->frameArray[app->sniffer_index].buffer[i]);
    }

    furi_string_cat_printf(app->text, "\ntime: %li ms", app->times[app->sniffer_index]);
    text_box_set_text(app->textBox, furi_string_get_cstr(app->text));
    text_box_set_focus(app->textBox, TextBoxFocusEnd);
}

void app_scene_box_sniffing_on_enter(void* context) {
    App* app = context;
    if(wait_to_be_set) {
        set_filter_sniffing(app->mcp_can, app->frameArray[app->sniffer_index].canId);
        furi_delay_ms(100);
        wait_to_be_set = false;
    }

    text_box_set_font(app->textBox, TextBoxFontText);
    text_box_reset(app->textBox);
    draw_box_text(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TextBoxView);
}

bool app_scene_box_sniffing_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_box_sniffing_on_exit(void* context) {
    App* app = context;
    wait_to_be_set = true;
    restart_filtering(app->mcp_can);
    furi_delay_ms(100);
    wait_to_be_set = false;
    furi_string_reset(app->text);
    text_box_reset(app->textBox);
}

static int32_t worker_sniffing(void* context) {
    App* app = context;
    MCP2515* mcp_can = app->mcp_can;
    CANFRAME frame = app->can_frame;
    FuriString* label = furi_string_alloc();
    uint8_t num_of_devices = 0;
    bool run = true;

    mcp_can->mode = MCP_NORMAL;
    if(mcp2515_init(mcp_can) != ERROR_OK) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DEVICE_NO_CONNECTED);
        run = false;
    }

    memset(app->frameArray, 0, sizeof(CANFRAME) * 100);

    while(run) {
        while(!condition && wait_to_be_set)
            furi_delay_ms(1);

        if(check_receive(mcp_can) == ERROR_OK) {
            read_can_message(mcp_can, &frame);
            save_frame_history(app, frame.canId, frame.buffer, frame.data_lenght);

            bool exists = false;
            for(uint8_t i = 0; i < num_of_devices; i++) {
                if(app->frameArray[i].canId == frame.canId) {
                    exists = true;
                    break;
                }
            }

            if(!exists && num_of_devices < 100) {
                app->frameArray[num_of_devices] = frame;
                app->times[num_of_devices] = 0;
                app->current_time[num_of_devices] = furi_get_tick();

                furi_string_reset(label);
                furi_string_cat_printf(label, "0x%lx", frame.canId);
                submenu_add_item(
                    app->submenu,
                    furi_string_get_cstr(label),
                    num_of_devices,
                    sniffer_menu_callback,
                    app);

                num_of_devices++;
            }

            for(uint8_t i = 0; i < num_of_devices; i++) {
                if(frame.canId == app->frameArray[i].canId) {
                    app->frameArray[i] = frame;
                    app->times[i] = furi_get_tick() - app->current_time[i];
                    app->current_time[i] = furi_get_tick();
                    break;
                }
            }

            app->num_of_devices = num_of_devices;
        } else {
            furi_delay_ms(1);
        }

        if(condition && !furi_hal_gpio_read(&gpio_button_back)) break;
    }

    furi_string_free(label);
    deinit_mcp2515(mcp_can);
    return 0;
}
