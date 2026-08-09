#include "../app_user.h"

/**
 * In this file you will find the ARP Spoofing scene and it worker,
 * for the usage of I played with the view dispatcher and the thread to show different
 * views for not use many scenes and avoid to define many
 */

// ArpSpoofing thread
int32_t arpspoofing_thread(void* context);

// ArpSpoofing on enter
void app_scene_arp_spoofing_on_enter(void* context) {
    App* app = (App*)context;
    app->arpspoofing_stop = false;
    // F0.4c — no thread_suspend; arpspoofing_thread is pure TX with
    // scanner_cancel_requested.
    app->thread_alternative =
        furi_thread_alloc_ex("ArpSpoofing", 10 * 1024, arpspoofing_thread, app);
    furi_thread_start(app->thread_alternative);
}

// ArpSpoofing on event
bool app_scene_arp_spoofing_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    if(event.type == SceneManagerEventTypeBack) {
        app->arpspoofing_stop = true;

        scene_manager_previous_scene(app->scene_manager);

        return true;
    }

    return false;
}

// ArpSpoofing on exit
void app_scene_arp_spoofing_on_exit(void* context) {
    App* app = (App*)context;

    if(app->thread_alternative) {
        app->arpspoofing_stop = true;

        furi_thread_join(app->thread_alternative);

        furi_thread_free(app->thread_alternative);

        app->thread_alternative = NULL;
    }
}

/**
 * Some views for the ArpSpoofing
 */
// Function to draw for waiting to attack
void draw_waiting_start_attack(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 40, 0, &I_SleepyCat50x50);
    // widget_add_string_element(
    //     app->widget, 64, 80, AlignCenter, AlignCenter, FontSecondary, "Start Attack??");
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Attack??", NULL, app);
}

// Function to draw when you are attacking
void draw_arpspoofing_attacking(App* app, uint8_t frame) {
    widget_reset(app->widget);

    switch(frame) {
    case 0:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation00);
        break;

    case 1:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation01);
        break;

    case 2:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation02);
        break;

    case 3:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation03);
        break;

    case 4:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation04);
        break;

    default:
        break;
    }

    widget_add_button_element(app->widget, GuiButtonTypeCenter, "STOP", NULL, app);
}

/**
 * Main Thread for the ArpSpoofing
 * Here is where the main code comes for the worker
 */
int32_t arpspoofing_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;
    uint8_t* buffer = app->ethernet->tx_buffer;
    uint16_t size = 0;

    uint32_t last_time = 0;

    uint8_t count_frame = 0;

    // F0.3b — scanner session (cancel-requested check).
    scanner_session_t scanner;
    scanner_session_init(&scanner, app);

    widget_reset(app->widget); // Reset the widget
    view_dispatcher_switch_to_view(
        app->view_dispatcher, WidgetView); // Switch view for the view dispatcher

    bool attack = false; // variable for the attacking

    bool show_once = true; // To display a view once

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff; // Start the enc28j60
        app->enc28j60_connected = start; // Update the connection status
    }

    bool program_loop = start; // This variable will help for the loop

    if(!is_the_network_connected(ethernet) && start) {
        draw_network_not_connected(app);
        program_loop = false;
    }

    if(!app->is_dora) {
        draw_dora_needed(app);
        program_loop = false;
    }

    // This condition works to get the IP
    /*if(program_loop) {
        // Draw to ask for a IP
        draw_ask_for_ip(app);

        // Loop to wait the button
        while(true) {
            // If the user wants to get back
            if(!furi_hal_gpio_read(&gpio_button_back)) {
                program_loop = false;
                break;
            }

            // Get the IP
            if(!furi_hal_gpio_read(&gpio_button_right)) {
                draw_waiting_for_ip(app);
                flipper_process_dora(
                    ethernet, app->ethernet->ip_address, app->ip_gateway, app->mac_gateway);
                break;
            }

            // Not get the IP
            if(!furi_hal_gpio_read(&gpio_button_left)) break;

            furi_delay_ms(1);
        }
    }*/

    // draw the waiting attack
    if(program_loop) {
        draw_your_ip_is(app);
        furi_delay_ms(1000);
        set_arp_message_for_attack_all(buffer, app->ethernet->mac_address, app->ip_gateway, &size);
        last_time = furi_get_tick();
    }

    while(!scanner_cancel_requested(&scanner) && !app->arpspoofing_stop && program_loop) {
        // Waiting to read the gpio ok to attack or stop to attack
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            attack = !attack;
            if(!attack) show_once = true;
            furi_delay_ms(200);
        }

        // To show when it is waiting to attack
        if(!attack && show_once) {
            draw_waiting_start_attack(app);
            show_once = false;
            count_frame = 0;
        }

        // When the attacks start
        if(attack) {
            send_arp_spoofing(ethernet, buffer, size);
        }

        if(attack && ((furi_get_tick() - last_time) > 200)) {
            last_time = furi_get_tick();
            draw_arpspoofing_attacking(app, count_frame);
            count_frame++;
            if(count_frame > 4) count_frame = 0;
        }

        furi_delay_ms(1);
    }

    // If the device is not connected
    if(!start) {
        draw_device_no_connected(app);
    }

    scanner_session_deinit(&scanner);
    return 0;
}
