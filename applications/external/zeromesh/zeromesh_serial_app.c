#include "zeromesh_serial.h"
#include "zeromesh_gui.h"
#include "zeromesh_uart.h"
#include "zeromesh_protocol.h"
#include "zeromesh_settings.h"
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

    uart_open(app);

    app->stop_thread = false;
    app->rx_thread = furi_thread_alloc_ex("mt_rx", 4096, rx_thread_fn, app);
    furi_thread_start(app->rx_thread);

    furi_delay_ms(500);
    request_info(app);

    uint32_t last_render = furi_get_tick();
    const uint32_t frame_delays[] = {
        1000, 500, 333, 250, 200,
        166, 142, 125, 111, 100
    };

    while(!app->stop_thread) {
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
            
            if(now - last_render >= frame_delay) {
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

    uart_close(app);

    gui_remove_view_port(app->gui, app->vp);
    view_port_free(app->vp);

    furi_record_close(RECORD_GUI);

    furi_stream_buffer_free(app->rx_stream);

    furi_mutex_free(app->lock);

    free(app);

    return 0;
}