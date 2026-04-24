#include "ui_i.h"
#include "ui_splash.h"
#include "../version.h"

#include <furi_hal.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void status_input(InputEvent *event, void *context);
static void dbgkey(FlipperHamApp *app, InputKey key);

static void dbgkey(FlipperHamApp *app, InputKey key)
{
    if (!app) return;

    if (key == InputKeyLeft)
    {
        app->dbg_mod = app->dbg_mod ? 0 : 1;
        return;
    }

    if (key == InputKeyUp)
    {
        if (app->dbg_dev < 8) app->dbg_dev++; else app->dbg_dev = 0;
        return;
    }

    if (key == InputKeyDown)
    {
        if (app->dbg_dev > 0) app->dbg_dev--; else app->dbg_dev = 8;
    }


}

void flipperham_status_view_alloc(FlipperHamApp *app)
{
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, flipperham_draw_callback, app);
    view_port_input_callback_set(app->view_port, status_input, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

void flipperham_status_view_free(FlipperHamApp *app)
{
    if (!app->view_port)
    {
        return;
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    app->view_port = NULL;
}


void flipperham_menu_free(FlipperHamApp *app)
{
    if (app->view_dispatcher)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSplash);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewMenu);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSend);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSettings);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewBulletin);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewStatus);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewMessage);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewMessageEdit);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewPosition);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewCall);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewBook);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewC2);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewFreq);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewFreqEdit);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewPosEdit);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewPosAction);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSsid);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewHam);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewHamTx);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewTextInput);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewReadme);
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
    }

    if (app->submenu)
    {
        submenu_free(app->submenu);
        app->submenu = NULL;
    }

    if (app->send_menu)
    {
        submenu_free(app->send_menu);
        app->send_menu = NULL;
    }

    if (app->settings_menu)
    {
        variable_item_list_free(app->settings_menu);
        app->settings_menu = NULL;
    }

    if (app->ham_menu)
    {
        variable_item_list_free(app->ham_menu);
        app->ham_menu = NULL;
    }

    if (app->ham_tx_menu)
    {
        variable_item_list_free(app->ham_tx_menu);
        app->ham_tx_menu = NULL;
    }

    if (app->bulletin_menu)
    {
        submenu_free(app->bulletin_menu);
        app->bulletin_menu = NULL;
    }

    if (app->status_menu)
    {
        submenu_free(app->status_menu);
        app->status_menu = NULL;
    }

    if (app->message_menu)
    {
        submenu_free(app->message_menu);
        app->message_menu = NULL;
    }

    if (app->message_edit_menu)
    {
        submenu_free(app->message_edit_menu);
        app->message_edit_menu = NULL;
    }

    if (app->position_menu)
    {
        submenu_free(app->position_menu);
        app->position_menu = NULL;
    }

    if (app->pos_action_menu)
    {
        submenu_free(app->pos_action_menu);
        app->pos_action_menu = NULL;
    }

    if (app->call_menu)
    {
        submenu_free(app->call_menu);
        app->call_menu = NULL;
    }

    if (app->book_menu)
    {
        submenu_free(app->book_menu);
        app->book_menu = NULL;
    }

    if (app->readme_widget)
    {
        widget_free(app->readme_widget);
        app->readme_widget = NULL;
    }

    if (app->freq_menu)
    {
        submenu_free(app->freq_menu);
        app->freq_menu = NULL;
    }

    if (app->c2_menu)
    {
        submenu_free(app->c2_menu);
        app->c2_menu = NULL;
    }

    if (app->freq_edit_menu)
    {
        variable_item_list_free(app->freq_edit_menu);
        app->freq_edit_menu = NULL;
    }

    if (app->pos_edit_menu)
    {
        variable_item_list_free(app->pos_edit_menu);
        app->pos_edit_menu = NULL;
    }

    if (app->ssid_menu)
    {
        variable_item_list_free(app->ssid_menu);
        app->ssid_menu = NULL;
    }

    if (app->text_input)
    {
        text_input_free(app->text_input);
        app->text_input = NULL;
    }

    splash_view_free(app);
}

static void status_input(InputEvent *event, void *context)
{
    FlipperHamApp *app = context;

    if (event->type != InputTypeShort) return;
    if (app->debug_tx)
    {
        if (event->key == InputKeyLeft || event->key == InputKeyUp || event->key == InputKeyDown)
        {
            dbgkey(app, event->key);
            return;
        }

        if (app->show_done)
        {
            if (event->key == InputKeyOk) app->repeat_more = true;
            else if (event->key == InputKeyBack) app->repeat_cancel = true;
            return;
        }
    }
    if (event->key != InputKeyBack) return;

    app->repeat_cancel = true;
    if (app->tx_started && !app->tx_done)
    {
        flipperham_radio_stop(app);
        app->tx_started = false;
        app->tx_done = true;
    }
}

static void tx_blink_green(void)
{
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightBlue, 0);
    furi_hal_light_set(LightRed, 255);
    furi_hal_light_set(LightGreen, 72);
}

uint32_t repeat_scale(FlipperHamApp *app)
{
    if (app->encoding_index == 0)
        return 4;
    return 1;
}

FlipperHamApp *flipperham_app_alloc(void)
{
    FlipperHamApp *app = malloc(sizeof(FlipperHamApp));

    gapp = app;
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->send_menu = submenu_alloc();
    app->bulletin_menu = submenu_alloc();
    app->status_menu = submenu_alloc();
    app->message_menu = submenu_alloc();
    app->message_edit_menu = submenu_alloc();
    app->position_menu = submenu_alloc();
    app->pos_action_menu = submenu_alloc();
    app->call_menu = submenu_alloc();
    app->book_menu = submenu_alloc();
    app->c2_menu = submenu_alloc();
    app->freq_menu = submenu_alloc();
    app->settings_menu = variable_item_list_alloc();
    app->ham_menu = variable_item_list_alloc();
    app->ham_tx_menu = variable_item_list_alloc();
    app->ssid_menu = variable_item_list_alloc();
    app->freq_edit_menu = variable_item_list_alloc();
    app->pos_edit_menu = variable_item_list_alloc();
    app->text_input = text_input_alloc();
    app->readme_widget = widget_alloc();
    app->splash_view = NULL;
    app->splash_timer = NULL;
    app->splash_cycle_timer = NULL;

    app->view_port = NULL;
    app->tx_started = false;
    app->tx_allowed = true;
    app->tx_done = false;
    app->show_done = false;
    app->send_requested = false;
    app->ham_ok = false;
    app->encoding_index = FlipperHamModemProfileDefault;
    app->ham_n = 0;
    app->repeat_n = 1;
    app->leadin_ms = 50;
    app->preamble_ms = 350;
    app->repeat_i = 1;
    app->repeat_wait = false;
    app->repeat_cancel = false;
    app->repeat_more = false;
    app->tx_msg_index = 0;
    app->tx_type = 0;
    memset(app->ham_ssid, 0, sizeof(app->ham_ssid));
    memset(app->ham_has_ssid, 0, sizeof(app->ham_has_ssid));
    memset(app->ham_pass, 0, sizeof(app->ham_pass));
    memset(app->ham_calls, 0, sizeof(app->ham_calls));
    app->status_index = 0;
    app->message_index = 0;
    app->message_last_tx = 0xff;
    app->pos_index = 0;
    app->dst_call_index = 0;
    app->dst_ssid = 0;
    app->edit_call_index = 0;
    app->book_call_index = 0;
    app->ham_index = 0;
    app->ham_tx_index = 0;
    app->freq_index = 0;
    app->bulletin_sel = FlipperHamBulletinIndexAdd;
    app->status_sel = FlipperHamStatusIndexAdd;
    app->message_sel = FlipperHamMessageIndexAdd;
    app->position_sel = FlipperHamPositionIndexAdd;
    app->call_sel = FlipperHamCallIndexAdd;
    app->book_sel = FlipperHamBookIndexAdd;
    app->book_action_sel = FlipperHamC2IndexEdit;
    app->freq_sel = FlipperHamFreqIndexAdd;
    app->ham_sel = 0;
    app->ham_tx_sel = 0;
    app->c2_h[0] = 0;
    app->f_edit[0] = 0;
    app->f_bad = false;
    app->aprs_path_index = 0;
    app->aprs_path_edit[0] = 0;
    app->debug_tx = false;
    app->return_view = FlipperHamViewMenu;
    app->splash_mode = 0;
    app->splash_next_view = FlipperHamViewMenu;
    app->text_mode = 0;
    app->text_view = FlipperHamViewMenu;
    app->pkt = NULL;
    app->wave = NULL;

    cfgload(app);
    app->dbg_mod = app->rf_mod;
    app->dbg_dev = app->rf_dev;

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    splash_view_alloc(app);
    app->return_view = splash_startup_view(app);

    submenu_add_item(app->submenu, "Send", FlipperHamMenuIndexSend, flipperham_menu_callback, app);
    submenu_add_item(app->submenu, "Settings", FlipperHamMenuIndexSettings,
                     flipperham_menu_callback, app);
    submenu_add_item(app->submenu, "Callbook", FlipperHamMenuIndexCallbook,
                     flipperham_menu_callback, app);
    if (app->ham_ok)
        submenu_add_item(app->submenu, "Ham Radio", FlipperHamMenuIndexHam,
                         flipperham_menu_callback, app);
    submenu_add_item(app->submenu, "About Flipper ham", FlipperHamMenuIndexReadme,
                     flipperham_menu_callback, app);

    submenu_add_item(app->send_menu, "Message", FlipperHamSendIndexMessage,
                     flipperham_send_callback, app);
    submenu_add_item(app->send_menu, "Position", FlipperHamSendIndexPosition,
                     flipperham_send_callback, app);
    submenu_add_item(app->send_menu, "Status", FlipperHamSendIndexStatus, flipperham_send_callback,
                     app);
    submenu_add_item(app->send_menu, "Bulletin", FlipperHamSendIndexBulletin,
                     flipperham_send_callback, app);

    bulletin_menu_build(app);
    status_menu_build(app);
    message_menu_build(app);
    position_menu_build(app);
    call_menu_build(app);
    book_menu_build(app);
    freq_menu_build(app);
    settings_menu_build(app);
    ssidfix(app);
    ham_menu_build(app);
    ham_tx_menu_build(app);

    snprintf(
        app->readme_h, sizeof(app->readme_h),
        "Version: %s\nCommit %s\nBuilt: %s\nHost: %s\n"
        "APRS experimental transmitter for Flipper. Don't transmit where you shouldn't. Uses FSK "
        "as a weak substitute for FM. Works, sometimes.\n\nI'm quite interested on what kind of "
        "hardware and with what parameters you got decodes.\n\nReports are really appreciated. Contact "
        "me at:\n\nwww.yo3gnd.ro\nyo3gnd@gmail.com\ngithub.com/yo3gnd\ninstagram: @yo3gnd\ntiktok: @yo3ngd\nyoutube.com/@yo3gnd\n\n",
        APP_VER_TEXT, APP_BUILD_COMMIT, APP_BUILD_TIME, APP_BUILD_HOST);

    widget_add_text_scroll_element(
        app->readme_widget, 0, 0, 128, 52, app->readme_h);
    widget_add_button_element(app->readme_widget, GuiButtonTypeLeft, "Back", readme_back, app);

    view_set_previous_callback(submenu_get_view(app->submenu), flipperham_exit_callback);
    view_set_previous_callback(submenu_get_view(app->send_menu), flipperham_send_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->settings_menu),
                               flipperham_settings_exit_callback);
    view_set_previous_callback(submenu_get_view(app->bulletin_menu),
                               flipperham_bulletin_exit_callback);
    view_set_previous_callback(submenu_get_view(app->status_menu), flipperham_status_exit_callback);
    view_set_previous_callback(submenu_get_view(app->message_menu),
                               flipperham_message_exit_callback);
    view_set_previous_callback(submenu_get_view(app->message_edit_menu),
                               flipperham_message_edit_exit_callback);
    view_set_previous_callback(submenu_get_view(app->position_menu),
                               flipperham_position_exit_callback);
    view_set_previous_callback(submenu_get_view(app->call_menu), flipperham_call_exit_callback);
    view_set_previous_callback(submenu_get_view(app->book_menu), book_exit);
    view_set_previous_callback(submenu_get_view(app->c2_menu), book_action_exit);
    view_set_previous_callback(submenu_get_view(app->freq_menu), flipperham_freq_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->freq_edit_menu),
                               flipperham_freq_edit_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->pos_edit_menu),
                               flipperham_pos_edit_exit_callback);
    view_set_previous_callback(submenu_get_view(app->pos_action_menu),
                               flipperham_pos_action_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->ssid_menu),
                               flipperham_ssid_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->ham_menu),
                               flipperham_ham_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->ham_tx_menu),
                               flipperham_ham_tx_exit_callback);
    view_set_previous_callback(text_input_get_view(app->text_input), flipperham_text_exit_callback);
    view_set_previous_callback(widget_get_view(app->readme_widget),
                               flipperham_readme_exit_callback);
    variable_item_list_set_enter_callback(app->ssid_menu, ssid_enter, app);
    variable_item_list_set_enter_callback(app->settings_menu, settings_enter, app);
    variable_item_list_set_enter_callback(app->ham_menu, ham_enter, app);
    variable_item_list_set_enter_callback(app->ham_tx_menu, ham_tx_enter, app);
    variable_item_list_set_enter_callback(app->freq_edit_menu, freq_edit_enter, app);
    variable_item_list_set_enter_callback(app->pos_edit_menu, pos_edit_enter, app);

    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSplash, app->splash_view);
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewMenu,
                             submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSend,
                             submenu_get_view(app->send_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSettings,
                             variable_item_list_get_view(app->settings_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewBulletin,
                             submenu_get_view(app->bulletin_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewStatus,
                             submenu_get_view(app->status_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewMessage,
                             submenu_get_view(app->message_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewMessageEdit,
                             submenu_get_view(app->message_edit_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewPosition,
                             submenu_get_view(app->position_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewCall,
                             submenu_get_view(app->call_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewBook,
                             submenu_get_view(app->book_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewC2,
                             submenu_get_view(app->c2_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewFreq,
                             submenu_get_view(app->freq_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewFreqEdit,
                             variable_item_list_get_view(app->freq_edit_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewPosEdit,
                             variable_item_list_get_view(app->pos_edit_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewPosAction,
                             submenu_get_view(app->pos_action_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSsid,
                             variable_item_list_get_view(app->ssid_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewHam,
                             variable_item_list_get_view(app->ham_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewHamTx,
                             variable_item_list_get_view(app->ham_tx_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewTextInput,
                             text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewReadme,
                             widget_get_view(app->readme_widget));
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewMenu);
    return app;
}

void flipperham_app_free(FlipperHamApp *app)
{
    if (!app)
        return;

    if (gapp == app)
        gapp = NULL;
    flipperham_status_view_free(app);
    flipperham_menu_free(app);
    if (app->pkt)
        free(app->pkt);
    if (app->wave)
        free(app->wave);
    if (app->gui)
        furi_record_close(RECORD_GUI);
    free(app);
}

void flipperham_send_hardcoded_message(FlipperHamApp *app)
{
    static const uint32_t repeat_delay_ms[] = {0, 2000, 4000, 8000, 15000};
    uint8_t i, n;
    uint32_t dt, rk, wait_ms;
    bool was_cancelled;

    if (!app->pkt)
        app->pkt = malloc(sizeof(Packet));
    if (!app->wave)
        app->wave = malloc(sizeof(uint16_t) * WAVE_N);
    if (!app->pkt || !app->wave)
    {
        if (app->pkt)
        {
            free(app->pkt);
            app->pkt = NULL;
        }
        if (app->wave)
        {
            free(app->wave);
            app->wave = NULL;
        }
        return;
    }

    flipperham_status_view_alloc(app);
    app->repeat_i = 0;
    app->repeat_t0 = furi_get_tick();
    app->repeat_to = 0;
    app->repeat_wait = false;
    app->repeat_cancel = false;
    app->repeat_more = false;
    app->show_done = false;
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightBlue, 0);
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    rk = repeat_scale(app);
    furi_hal_power_suppress_charge_enter();

    n = app->repeat_n;
again:
    for (i = 0; i < n; i++)
    {
        app->repeat_i++;

        txstart(app);
        if (!app->tx_ok)
        {
            furi_hal_power_suppress_charge_exit();
            furi_hal_light_blink_stop();
            furi_hal_light_set(LightBlue, 0);
            furi_hal_light_set(LightRed, 0);
            furi_hal_light_set(LightGreen, 0);
            flipperham_status_view_free(app);
            free(app->pkt);
            free(app->wave);
            app->pkt = NULL;
            app->wave = NULL;
            return;
        }
        app->tx_started = false;
        app->tx_allowed = true;
        app->repeat_wait = false;
        app->show_done = false;

        tx_blink_green();
        view_port_update(app->view_port);
        furi_delay_ms(100);

        flipperham_radio_start(app);

        while (!app->tx_done)
        {
            view_port_update(app->view_port);
            furi_delay_ms(50);
        }

        while (app->tx_started && !furi_hal_subghz_is_async_tx_complete())
        {
            view_port_update(app->view_port);
            furi_delay_ms(20);
        }

        flipperham_radio_stop(app);
        furi_hal_light_blink_stop();
        furi_hal_light_set(LightBlue, 0);
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 0);

        if (i + 1 >= n)
            break;

        if (app->debug_tx)
        {
            app->repeat_t0 = furi_get_tick();
            wait_ms = 2000;
        }
        else
            wait_ms = repeat_delay_ms[i + 1] * rk;
        app->repeat_to = wait_ms;
        app->repeat_wait = true;
        app->tx_done = false;

        while (1)
        {
            if (app->repeat_cancel)
                break;

            dt = furi_get_tick() - app->repeat_t0;
            if (dt >= app->repeat_to) break;

            view_port_update(app->view_port);
            furi_delay_ms(50 * rk);
        }

        app->repeat_wait = false;
        if (app->repeat_cancel)
            break;
    }

    was_cancelled = app->repeat_cancel;
    app->repeat_wait = false;
    if (!was_cancelled)
    {
        app->show_done = true;
        app->tx_done = true;
        view_port_update(app->view_port);
        if (app->debug_tx)
            while (!app->repeat_cancel && !app->repeat_more)
            {
                view_port_update(app->view_port);
                furi_delay_ms(50);
            }
        else
            furi_delay_ms(750);
    }
    else
    {
        app->show_done = false;
        app->tx_done = true;
    }
    if (app->repeat_more)
    {
        app->repeat_more = false;
        app->repeat_cancel = false;
        app->show_done = false;
        n = 1;
        goto again;
    }
    app->repeat_cancel = false;
    furi_hal_power_suppress_charge_exit();
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightBlue, 0);
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);

    app->repeat_cancel = false;
    app->repeat_more = false;
    flipperham_status_view_free(app);
    free(app->pkt);
    free(app->wave);
    app->pkt = NULL;
    app->wave = NULL;
}
