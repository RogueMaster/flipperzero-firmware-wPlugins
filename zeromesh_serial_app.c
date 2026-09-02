#include "zeromesh_serial.h"
#include "zeromesh_gui.h"
#include "zeromesh_uart.h"
#include "zeromesh_transport.h"
#include "zeromesh_map.h"
#include "zeromesh_notify.h"
#include "zeromesh_protocol.h"
#include "zeromesh_settings.h"
#include "zeromesh_rtttl.h"
#include "zeromesh_channel.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int32_t zeromesh_serial_app(void* p) {
    (void)p;

    ZeroMeshApp* app = malloc(sizeof(ZeroMeshApp));
    memset(app, 0, sizeof(ZeroMeshApp));

    app->lock = furi_mutex_alloc(FuriMutexTypeNormal);

    app->uart_id = FuriHalSerialIdUsart;
    app->baud = 115200;

    app->ui_mode = PAGE_MESSAGES;

    app->notify_vibro = true;
    app->notify_led = true;
    app->notify_ringtone = RingtoneShort;
    
    app->scroll_speed = 5;
    app->scroll_framerate = 5;
    app->lmh_mode = LMH_Scroll;
    
    channel_init(app);
    
    ringtones_scan(app);
    settings_load(app);

    snprintf(app->status, sizeof(app->status), "Connecting...");

    for(int i = 0; i < LOG_LINES; i++) app->lines[i][0] = 0;
    app->line_head = 0;

    app->rx_stream = furi_stream_buffer_alloc(RX_STREAM_SIZE, 1);

    app->gui = furi_record_open(RECORD_GUI);

    app->vp = view_port_alloc();
    view_port_draw_callback_set(app->vp, render_cb, app);
    view_port_input_callback_set(app->vp, input_cb, app);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);

    map_alloc(app);

    transport_open(app);

    app->stop_thread = false;
    app->rx_thread = furi_thread_alloc_ex("mt_rx", 4096, rx_thread_fn, app);
    furi_thread_start(app->rx_thread);

    furi_delay_ms(500);

    /* Ask for node config once the link is actually carrying bytes. Over
       serial that is immediate; over BLE it is whenever the peer dials in,
       and again after a reconnect. */
    bool config_requested = false;
    uint32_t last_config_req = 0;
    uint32_t last_chan_req = 0;
    uint32_t last_pos_req = 0;
    const uint32_t CONFIG_RETRY_MS = 5000;

    uint32_t last_render = furi_get_tick();
    uint32_t last_heartbeat = furi_get_tick();
    const uint32_t HEARTBEAT_INTERVAL_MS = 30000;
    const uint32_t frame_delays[] = {
        1000, 500, 333, 250, 200,
        166, 142, 125, 111, 100
    };

    while(!app->stop_thread) {
        if(app->pending_notify) {
            app->pending_notify = false;
            notify_rx_message(app);
        }

        if(app->pending_action != PendingNone) {
            PendingAction act = app->pending_action;
            app->pending_action = PendingNone;
            switch(act) {
            case PendingPosReq:
                request_position(app, app->pending_node);
                break;
            case PendingInfoReq:
                request_node_info(app, app->pending_node);
                break;
            case PendingSetLora:
                set_node_lora(app, app->pending_a, app->pending_b);
                break;
            case PendingSetGps:
                set_node_gps(app, app->cfg_gps != 0);
                break;
            case PendingGetChannel:
                request_channel(app, 0);
                break;
            case PendingSetChannel:
                set_channel_config(app, app->cfg_ch_private != 0, app->cfg_ch_pos != 0);
                break;
            case PendingSetFixed: {
                int32_t lat_i = 0, lon_i = 0;
                if(map_view_center(app, &lat_i, &lon_i))
                    set_fixed_position(app, lat_i, lon_i);
                break;
            }
            case PendingClearFixed:
                clear_fixed_position(app);
                break;
            case PendingSetRole:
                set_node_role(app, app->pending_a);
                break;
            case PendingReboot:
                reboot_node(app, 5);
                break;
            case PendingInfoAll:
                request_info(app);
                break;
            case PendingSendText:
                send_text_message(app, app->pending_text, app->pending_node);
                app->pending_text[0] = '\0';
                break;
            case PendingPlayTone:
                play_ringtone(app);
                break;
            default:
                break;
            }
        }

        if(app->ui_mode == PAGE_MAP) map_tick(app);


        if(transport_is_up(app)) {
            /* Keep asking until the radio tells us our node number. Over BLE
               there is no dependable edge for "peer subscribed", so retry
               rather than firing once and hoping we timed it right. */
            if(app->my_node_num == 0 &&
               (!config_requested ||
                furi_get_tick() - last_config_req >= CONFIG_RETRY_MS)) {
                request_info(app);
                config_requested = true;
                last_config_req = furi_get_tick();
            }

            /* Same retry shape: the channel read is what fills in the
               public/private and position-sharing rows. */
            if(app->my_node_num && !app->cfg_ch_known &&
               furi_get_tick() - last_chan_req >= CONFIG_RETRY_MS) {
                request_channel(app, 0);
                last_chan_req = furi_get_tick();
            }

            if(app->my_node_num && !app->cfg_pos_known &&
               furi_get_tick() - last_pos_req >= CONFIG_RETRY_MS) {
                request_position_config(app);
                last_pos_req = furi_get_tick();
            }
        } else {
            config_requested = false;
            app->cfg_ch_known = false;
            app->cfg_pos_known = false;
        }
        if(furi_get_tick() - last_heartbeat >= HEARTBEAT_INTERVAL_MS) {
            send_heartbeat(app);
            last_heartbeat = furi_get_tick();
        }
        if(app->show_keyboard) {
            gui_remove_view_port(app->gui, app->vp);

            app->kb_dispatcher = view_dispatcher_alloc();
            app->text_input = text_input_alloc();

            text_input_set_header_text(app->text_input, "Send Message:");
            text_input_set_result_callback(
                app->text_input,
                text_input_callback,
                app,
                app->text_buffer,
                sizeof(app->text_buffer),
                false);

            view_dispatcher_add_view(app->kb_dispatcher, 0, text_input_get_view(app->text_input));
            view_set_previous_callback(text_input_get_view(app->text_input), kb_back_callback);

            view_dispatcher_attach_to_gui(app->kb_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
            view_dispatcher_switch_to_view(app->kb_dispatcher, 0);
            view_dispatcher_run(app->kb_dispatcher);

            view_dispatcher_remove_view(app->kb_dispatcher, 0);

            text_input_free(app->text_input);
            view_dispatcher_free(app->kb_dispatcher);

            app->show_keyboard = false;
            gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);
            last_render = furi_get_tick();
        } else {
            uint32_t now = furi_get_tick();
            uint32_t frame_delay = frame_delays[app->scroll_framerate - 1];

            /* Only this thread may touch the ViewPort. view_port_update() from
               a second thread times out against a slow draw and then releases a
               mutex it never took, freeing the canvas mid-render. */
            if(app->need_render || now - last_render >= frame_delay) {
                app->need_render = false;
                view_port_update(app->vp);
                last_render = now;
            } else {
                furi_delay_ms(10);
            }
        }
    }

    app->stop_thread = true;
    
    settings_save(app);

    furi_thread_join(app->rx_thread);
    furi_thread_free(app->rx_thread);

    transport_close(app);

    gui_remove_view_port(app->gui, app->vp);
    view_port_free(app->vp);

    map_free(app);

    furi_record_close(RECORD_GUI);

    furi_stream_buffer_free(app->rx_stream);

    furi_mutex_free(app->lock);

    free(app);

    return 0;
}