#include "../app_user.h"

uint8_t router_ip[4] = {192, 168, 0, 1};

// F0.4e — DORA used to be processed inside ethernet_thread (app_worker.c)
// triggered by flag_dhcp_dora. With the worker thread gone, GetIPScene
// runs DORA in its own alt thread.
static int32_t get_ip_dora_thread(void* context) {
    App* app = (App*)context;
    enc28j60_t* ethernet = app->ethernet;

    view_dispatcher_send_custom_event(app->view_dispatcher, wait_ip_event);

    // Pause rx_dispatch around DORA so DHCP OFFER/ACK packets reach
    // flipper_process_dora_with_host_name's own receive_packet calls.
    // Without this, rx_dispatch consumes the OFFER and drops it (no
    // handler matches DHCP). F0.5 chip-mutex will let us drop the pause.
    // F0.5f — pass app->dora_cancel so on_exit (Back) can interrupt
    // the 10 s DORA timeout.
    rx_dispatch_pause();
    bool got_ip = flipper_process_dora_with_host_name(
        ethernet,
        ethernet->ip_address,
        app->ip_gateway,
        ethernet->subnet_mask,
        "Flippa 0",
        &app->dora_cancel);
    rx_dispatch_resume();

    if(got_ip) {
        app->is_dora = true;
        send_arp_gratuitous(ethernet, ethernet->mac_address, ethernet->ip_address);
        view_dispatcher_send_custom_event(app->view_dispatcher, ip_gotten_event);
        app->is_static_ip = true;
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, ip_no_gotten_event);
    }
    return 0;
}

// Function for the testing scene on enter
void app_scene_get_ip_scene_on_enter(void* context) {
    App* app = (App*)context;

    // Reset the widget and switch view
    widget_reset(app->widget);

    // Start ethernet
    if(!app->enc28j60_connected) {
        enc28j60_soft_reset(app->ethernet);
        app->enc28j60_connected = enc28j60_start(app->ethernet) != 0xff;
    }

    if(app->enc28j60_connected) {
        // F0.4e — replaced flag_dhcp_dora signal-to-worker with a
        // dedicated alt thread that runs DORA directly.
        // F0.5f — clear the cancel flag before starting; on_exit may
        // have left it set from a prior cancelled run.
        app->dora_cancel = false;
        app->thread_alternative =
            furi_thread_alloc_ex("Get IP DORA", 4 * 1024, get_ip_dora_thread, app);
        furi_thread_start(app->thread_alternative);
    } else {
        draw_device_no_connected(app);
    }

    // Change view
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_get_ip_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case wait_ip_event:
            draw_waiting_for_ip(app);
            break;

        case ip_no_gotten_event:
            draw_ip_not_got_it(app);
            break;

        case ip_gotten_event:
            draw_your_ip_is(app);
            break;

        default:
            break;
        }
    }
    return consumed;
}

// Function for the testing scene on exit
void app_scene_get_ip_scene_on_exit(void* context) {
    App* app = (App*)context;
    if(app->thread_alternative) {
        // F0.5f — request cancel before join. The DORA loop polls this
        // flag every iteration and breaks out early, so join completes
        // within ~ms instead of the 10 s DHCP timeout.
        app->dora_cancel = true;
        furi_thread_join(app->thread_alternative);
        furi_thread_free(app->thread_alternative);
        app->thread_alternative = NULL;
    }
}
