#include "ui_i.h"

#include <furi_hal.h>
#include <gui/view.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void messageReselect(FlipperHamApp* app, uint8_t at);
static void positionReselect(FlipperHamApp* app, uint8_t at);
static void messageActionBuild(FlipperHamApp* app);
static void messageActionSave(void* context);
static void messageActionDo(void* context, uint32_t index);
static bool messageCopy(FlipperHamApp* app);
static void messageDelete(FlipperHamApp* app);
static void positionActionBuild(FlipperHamApp* app);
static void positionActionDo(void* context, uint32_t index);
static void positionDelete(FlipperHamApp* app);

static void messageReselect(FlipperHamApp* app, uint8_t at) {
    uint8_t j;

    app->message_sel = FlipperHamMessageIndexAdd;

    for(j = at; j < TXT_N; j++)
        if(app->message_used[j])
            if(app->message[j][0]) {
                app->message_sel = FlipperHamMessageIndexBase + j;
                break;
            }

    if(app->message_sel == FlipperHamMessageIndexAdd)
        for(j = at; j > 0; j--)
            if(app->message_used[j - 1])
                if(app->message[j - 1][0]) {
                    app->message_sel = FlipperHamMessageIndexBase + j - 1;
                    break;
                }
}

static void positionReselect(FlipperHamApp* app, uint8_t at) {
    uint8_t j;

    app->position_sel = FlipperHamPositionIndexAdd;

    for(j = at; j < TXT_N; j++)
        if(app->pos_used[j])
            if(app->pos_name[j][0]) {
                app->position_sel = FlipperHamPositionIndexBase + j;
                break;
            }

    if(app->position_sel == FlipperHamPositionIndexAdd)
        for(j = at; j > 0; j--)
            if(app->pos_used[j - 1])
                if(app->pos_name[j - 1][0]) {
                    app->position_sel = FlipperHamPositionIndexBase + j - 1;
                    break;
                }
}

static void messageActionBuild(FlipperHamApp* app) {
    submenu_reset(app->message_edit_menu);
    submenu_set_header(app->message_edit_menu, "Edit Message");
    submenu_add_item(app->message_edit_menu, "Edit", FlipperHamC2IndexEdit, messageActionDo, app);
    submenu_add_item(app->message_edit_menu, "Copy", FlipperHamC2IndexCopy, messageActionDo, app);
    if(app->message_n <= 1)
        ;
    else
        submenu_add_item(
            app->message_edit_menu, "Delete", FlipperHamC2IndexDelete, messageActionDo, app);
    submenu_set_selected_item(app->message_edit_menu, FlipperHamC2IndexEdit);
}

static void positionActionBuild(FlipperHamApp* app) {
    submenu_reset(app->pos_action_menu);
    submenu_set_header(app->pos_action_menu, "Edit GPS Position");
    submenu_add_item(
        app->pos_action_menu, "Edit name", FlipperHamPosEditIndexName, positionActionDo, app);
    submenu_add_item(
        app->pos_action_menu, "Edit latitude", FlipperHamPosEditIndexLat, positionActionDo, app);
    submenu_add_item(
        app->pos_action_menu, "Edit longitude", FlipperHamPosEditIndexLon, positionActionDo, app);
    if(app->pos_n > 1)
        if(app->pos_index < TXT_N)
            if(app->pos_used[app->pos_index])
                submenu_add_item(
                    app->pos_action_menu,
                    "Delete",
                    FlipperHamPosEditIndexDelete,
                    positionActionDo,
                    app);
    submenu_set_selected_item(app->pos_action_menu, FlipperHamPosEditIndexName);
}

static void positionDelete(FlipperHamApp* app) {
    if(!app) return;
    if(app->pos_n <= 1) return;
    if(app->pos_index >= TXT_N) return;

    app->pos_name[app->pos_index][0] = 0;
    app->pos_lat[app->pos_index][0] = 0;
    app->pos_lon[app->pos_index][0] = 0;
    app->pos_used[app->pos_index] = 0;
    positionReselect(app, app->pos_index);

    position_fix(app);
    cfgsave(app);
    position_menu_build(app);
}

static void positionActionDo(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    const char* title;
    char* out;
    uint16_t n;

    if(index == FlipperHamPosEditIndexDelete) {
        positionDelete(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewPosition);
        return;
    }

    title = "Name";
    out = app->p_name_edit;
    n = sizeof(app->p_name_edit);
    app->text_mode = 6;

    if(index == FlipperHamPosEditIndexLat) {
        title = "Edit latitude";
        out = app->p_lat_edit;
        n = sizeof(app->p_lat_edit);
        app->text_mode = 7;
    } else if(index == FlipperHamPosEditIndexLon) {
        title = "Edit longitude";
        out = app->p_lon_edit;
        n = sizeof(app->p_lon_edit);
        app->text_mode = 8;
    } else
        title = "Edit name";

    app->text_view = FlipperHamViewPosAction;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, title);
    text_input_set_result_callback(app->text_input, position_save, app, out, n, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

static bool messageCopy(FlipperHamApp* app) {
    char a[TXT_LEN];
    uint8_t i;

    if(!app) return false;
    if(app->message_index >= TXT_N) return false;
    if(!app->message[app->message_index][0]) return false;

    snprintf(a, sizeof(a), "%s", app->message[app->message_index]);

    for(i = 0; i < TXT_N; i++)
        if(!app->message_used[i] || !app->message[i][0]) {
            snprintf(app->message[i], sizeof(app->message[i]), "%s", a);
            app->message_used[i] = 1;
            app->message_sel = FlipperHamMessageIndexBase + i;
            app->message_index = i;
            message_fix(app);
            cfgsave(app);
            message_menu_build(app);
            return true;
        }

    return false;
}

static void messageActionSave(void* context) {
    FlipperHamApp* app = context;
    uint8_t i;

    if(!app) return;

    i = app->message_index;
    if(i >= TXT_N) return;

    if(app->m_edit[0]) {
        snprintf(app->message[i], sizeof(app->message[i]), "%s", app->m_edit);
        app->message_used[i] = 1;
        app->message_sel = FlipperHamMessageIndexBase + i;
        message_fix(app);
        cfgsave(app);
        message_menu_build(app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewMessage);
}

static void messageDelete(FlipperHamApp* app) {
    uint8_t i;

    if(!app) return;
    if(app->message_n <= 1) return;
    i = app->message_index;
    if(i >= TXT_N) return;

    app->message[i][0] = 0;
    app->message_used[i] = 0;
    messageReselect(app, i);

    if(app->message_last_tx == i) app->message_last_tx = 0xff;

    message_fix(app);
    cfgsave(app);
    message_menu_build(app);
}

static void messageActionDo(void* context, uint32_t index) {
    FlipperHamApp* app = context;

    if(index == FlipperHamC2IndexCopy) {
        messageCopy(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewMessage);
        return;
    }

    if(index == FlipperHamC2IndexDelete) {
        messageDelete(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewMessage);
        return;
    }

    if(index != FlipperHamC2IndexEdit) return;
    if(app->message_index >= TXT_N) return;

    snprintf(app->m_edit, sizeof(app->m_edit), "%s", app->message[app->message_index]);
    app->text_mode = 3;
    app->text_view = FlipperHamViewMessageEdit;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Edit Message");
    text_input_set_result_callback(
        app->text_input, messageActionSave, app, app->m_edit, sizeof(app->m_edit), false);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void m(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    if(index == FlipperHamMessageIndexAdd) {
        app->message_sel = FlipperHamMessageIndexAdd;
        app->message_index = 0xff;

        for(i = 0; i < TXT_N; i++)
            if(!app->message_used[i] || !app->message[i][0]) {
                app->message_index = i;
                break;
            }

        if(app->message_index == 0xff) return;

        app->m_edit[0] = 0;
    } else {
        i = index - FlipperHamMessageIndexBase;
        if(i >= TXT_N) return;

        app->message_sel = index;
        app->message_index = i;
        snprintf(app->m_edit, sizeof(app->m_edit), "%s", app->message[i]);
    }

    app->text_mode = 3;
    app->text_view = FlipperHamViewMessage;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Message");
    text_input_set_result_callback(
        app->text_input, message_save, app, app->m_edit, sizeof(app->m_edit), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void message_pick(void* context, InputType input_type, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    i = index - FlipperHamMessageIndexBase;
    if(i >= TXT_N) return;

    if(input_type == InputTypeShort) {
        app->tx_type = 2;
        app->tx_msg_index = i;
        app->message_sel = index;
        if(app->dst_call_index < CALL_N)
            if(app->calls_used[app->dst_call_index])
                if(app->calls[app->dst_call_index][0])
                    app->call_sel = FlipperHamCallIndexBase + app->dst_call_index;
        call_menu_build(app);

        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewCall);
        return;
    }

    if(input_type != InputTypeLong) return;

    app->message_sel = index;
    app->message_index = i;
    messageActionBuild(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewMessageEdit);
}

void message_save(void* context) {
    FlipperHamApp* app = context;
    uint8_t i;

    i = app->message_index;
    if(i >= TXT_N) return;

    if(!app->m_edit[0]) {
        app->message[i][0] = 0;
        app->message_used[i] = 0;
        messageReselect(app, i);
    } else {
        snprintf(app->message[i], sizeof(app->message[i]), "%s", app->m_edit);
        app->message_used[i] = 1;
        app->message_sel = FlipperHamMessageIndexBase + i;
    }

    message_fix(app);
    cfgsave(app);
    message_menu_build(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewMessage);
}

void p(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    if(index != FlipperHamPositionIndexAdd) return;

    app->position_sel = FlipperHamPositionIndexAdd;
    app->pos_index = 0xff;

    for(i = 0; i < TXT_N; i++)
        if(!app->pos_used[i] || !app->pos_name[i][0]) {
            app->pos_index = i;
            break;
        }

    if(app->pos_index == 0xff) return;

    app->pos_name[app->pos_index][0] = 0;
    app->pos_lat[app->pos_index][0] = 0;
    app->pos_lon[app->pos_index][0] = 0;
    app->pos_used[app->pos_index] = 0;

    snprintf(app->p_name_edit, sizeof(app->p_name_edit), "Position");
    snprintf(app->p_lat_edit, sizeof(app->p_lat_edit), "0.00");
    snprintf(app->p_lon_edit, sizeof(app->p_lon_edit), "0.00");
    pos_edit_menu_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewPosEdit);
}

void position_pick(void* context, InputType input_type, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    i = index - FlipperHamPositionIndexBase;
    if(i >= TXT_N) return;

    if(input_type == InputTypeShort) {
        app->position_sel = index;
        app->tx_type = 3;
        app->tx_msg_index = i;
        app->return_view = FlipperHamViewPosition;
        app->send_requested = true;
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    if(input_type != InputTypeLong) return;

    app->position_sel = index;
    app->pos_index = i;
    snprintf(app->p_name_edit, sizeof(app->p_name_edit), "%s", app->pos_name[i]);
    snprintf(app->p_lat_edit, sizeof(app->p_lat_edit), "%s", app->pos_lat[i]);
    snprintf(app->p_lon_edit, sizeof(app->p_lon_edit), "%s", app->pos_lon[i]);
    positionActionBuild(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewPosAction);
}

void position_save(void* context) {
    FlipperHamApp* app = context;
    uint8_t i;
    char a[POS_LEN];
    uint32_t v;

    i = app->pos_index;
    if(i >= TXT_N) return;

    if(app->text_mode == 6)
        snprintf(app->pos_name[i], sizeof(app->pos_name[i]), "%s", app->p_name_edit);
    if(app->text_mode == 7) {
        if(aprs_ll_clamp(a, sizeof(a), app->p_lat_edit, 0))
            snprintf(app->p_lat_edit, sizeof(app->p_lat_edit), "%s", a);
        else if(app->pos_lat[i][0])
            snprintf(app->p_lat_edit, sizeof(app->p_lat_edit), "%s", app->pos_lat[i]);
        else
            snprintf(app->p_lat_edit, sizeof(app->p_lat_edit), "0.00000");
        snprintf(app->pos_lat[i], sizeof(app->pos_lat[i]), "%s", app->p_lat_edit);
    }
    if(app->text_mode == 8) {
        if(aprs_ll_clamp(a, sizeof(a), app->p_lon_edit, 1))
            snprintf(app->p_lon_edit, sizeof(app->p_lon_edit), "%s", a);
        else if(app->pos_lon[i][0])
            snprintf(app->p_lon_edit, sizeof(app->p_lon_edit), "%s", app->pos_lon[i]);
        else
            snprintf(app->p_lon_edit, sizeof(app->p_lon_edit), "0.00000");
        snprintf(app->pos_lon[i], sizeof(app->pos_lon[i]), "%s", app->p_lon_edit);
    }

    if(!app->pos_name[i][0]) {
        app->pos_used[i] = 0;
        positionReselect(app, i);
    } else {
        app->pos_used[i] = 1;
        app->position_sel = FlipperHamPositionIndexBase + i;
    }

    position_fix(app);
    cfgsave(app);
    position_menu_build(app);
    pos_edit_menu_build(app);
    v = app->text_view;
    if(v == FlipperHamViewPosAction) {
        if(app->pos_used[i] && app->pos_name[i][0])
            positionActionBuild(app);
        else
            v = FlipperHamViewPosition;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, v);
}

void flipperham_bulletin_callback(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    if(index != FlipperHamBulletinIndexAdd) return;

    app->bulletin_sel = FlipperHamBulletinIndexAdd;
    app->bulletin_index = 0xff;

    for(i = 0; i < TXT_N; i++)
        if(!app->bulletin_used[i]) {
            app->bulletin_index = i;
            break;
        }

    if(app->bulletin_index == 0xff) return;

    app->b_edit[0] = 0;
    app->text_mode = 0;
    app->text_view = FlipperHamViewBulletin;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Bulletin");
    text_input_set_result_callback(
        app->text_input, bulletin_save, app, app->b_edit, sizeof(app->b_edit), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void bulletin_pick(void* context, InputType input_type, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    i = index - FlipperHamBulletinIndexBase;
    if(i >= TXT_N) return;

    if(input_type == InputTypeShort) {
        app->bulletin_sel = index;
        app->tx_type = 0;
        app->tx_msg_index = i;
        app->return_view = FlipperHamViewBulletin;
        app->send_requested = true;
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    if(input_type != InputTypeLong) return;

    app->bulletin_sel = index;
    app->bulletin_index = i;
    snprintf(app->b_edit, sizeof(app->b_edit), "%s", app->bulletin[i]);
    app->text_mode = 0;
    app->text_view = FlipperHamViewBulletin;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Edit bulletin");
    text_input_set_result_callback(
        app->text_input, bulletin_save, app, app->b_edit, sizeof(app->b_edit), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void bulletin_save(void* context) {
    FlipperHamApp* app = context;
    uint8_t i, j;

    i = app->bulletin_index;
    if(i >= TXT_N) return;

    if(!app->b_edit[0]) {
        app->bulletin[i][0] = 0;
        app->bulletin_used[i] = 0;
        app->bulletin_sel = FlipperHamBulletinIndexAdd;
        for(j = i; j < TXT_N; j++)
            if(app->bulletin_used[j])
                if(app->bulletin[j][0]) {
                    app->bulletin_sel = FlipperHamBulletinIndexBase + j;
                    break;
                }
        if(app->bulletin_sel == FlipperHamBulletinIndexAdd)
            for(j = i; j > 0; j--)
                if(app->bulletin_used[j - 1])
                    if(app->bulletin[j - 1][0]) {
                        app->bulletin_sel = FlipperHamBulletinIndexBase + j - 1;
                        break;
                    }
    } else {
        snprintf(app->bulletin[i], sizeof(app->bulletin[i]), "%s", app->b_edit);
        app->bulletin_used[i] = 1;
        app->bulletin_sel = FlipperHamBulletinIndexBase + i;
    }

    bulletin_fix(app);
    cfgsave(app);
    bulletin_menu_build(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewBulletin);
}

void st(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    if(index != FlipperHamStatusIndexAdd) return;

    app->status_sel = FlipperHamStatusIndexAdd;
    app->status_index = 0xff;

    for(i = 0; i < TXT_N; i++)
        if(!app->status_used[i]) {
            app->status_index = i;
            break;
        }

    if(app->status_index == 0xff) return;

    app->st_edit[0] = 0;
    app->text_mode = 1;
    app->text_view = FlipperHamViewStatus;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Status");
    text_input_set_result_callback(
        app->text_input, status_save, app, app->st_edit, sizeof(app->st_edit), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void status_pick(void* context, InputType input_type, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    i = index - FlipperHamStatusIndexBase;
    if(i >= TXT_N) return;

    if(input_type == InputTypeShort) {
        app->status_sel = index;
        app->tx_type = 1;
        app->tx_msg_index = i;
        app->return_view = FlipperHamViewStatus;
        app->send_requested = true;
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    if(input_type != InputTypeLong) return;

    app->status_sel = index;
    app->status_index = i;
    snprintf(app->st_edit, sizeof(app->st_edit), "%s", app->status[i]);
    app->text_mode = 1;
    app->text_view = FlipperHamViewStatus;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Edit status");
    text_input_set_result_callback(
        app->text_input, status_save, app, app->st_edit, sizeof(app->st_edit), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void status_save(void* context) {
    FlipperHamApp* app = context;
    uint8_t i, j;

    i = app->status_index;
    if(i >= TXT_N) return;

    if(!app->st_edit[0]) {
        app->status[i][0] = 0;
        app->status_used[i] = 0;
        app->status_sel = FlipperHamStatusIndexAdd;
        for(j = i; j < TXT_N; j++)
            if(app->status_used[j])
                if(app->status[j][0]) {
                    app->status_sel = FlipperHamStatusIndexBase + j;
                    break;
                }
        if(app->status_sel == FlipperHamStatusIndexAdd)
            for(j = i; j > 0; j--)
                if(app->status_used[j - 1])
                    if(app->status[j - 1][0]) {
                        app->status_sel = FlipperHamStatusIndexBase + j - 1;
                        break;
                    }
    } else {
        snprintf(app->status[i], sizeof(app->status[i]), "%s", app->st_edit);
        app->status_used[i] = 1;
        app->status_sel = FlipperHamStatusIndexBase + i;
    }

    status_fix(app);
    cfgsave(app);
    status_menu_build(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewStatus);
}

void cl(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    if(index == FlipperHamCallIndexAdd) {
        app->call_sel = FlipperHamCallIndexAdd;
        app->edit_call_index = 0xff;

        for(i = 0; i < CALL_N; i++)
            if(!app->calls_used[i] || !app->calls[i][0]) {
                app->edit_call_index = i;
                break;
            }

        if(app->edit_call_index == 0xff) return;

        app->c_edit[0] = 0;
    } else {
        i = index - FlipperHamCallIndexBase;
        if(i >= CALL_N) return;

        app->call_sel = index;
        app->edit_call_index = i;
        snprintf(app->c_edit, sizeof(app->c_edit), "%s", app->calls[i]);
    }

    app->text_mode = 2;
    app->text_view = FlipperHamViewCall;

    text_input_reset(app->text_input);
    if(index == FlipperHamCallIndexAdd)
        text_input_set_header_text(app->text_input, "Callsign");
    else
        text_input_set_header_text(app->text_input, "Edit callsign");
    text_input_set_result_callback(
        app->text_input, call_save, app, app->c_edit, sizeof(app->c_edit), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void cb(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;

    if(index == FlipperHamBookIndexAdd) {
        app->book_sel = FlipperHamBookIndexAdd;
        app->edit_call_index = 0xff;

        for(i = 0; i < CALL_N; i++)
            if(!app->calls_used[i] || !app->calls[i][0]) {
                app->edit_call_index = i;
                break;
            }

        if(app->edit_call_index == 0xff) return;

        app->c_edit[0] = 0;
        app->text_mode = 4;
        app->text_view = FlipperHamViewBook;
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Callsign");
        text_input_set_result_callback(
            app->text_input, call_save, app, app->c_edit, sizeof(app->c_edit), false);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
        return;
    }

    index -= FlipperHamBookIndexBase;
    if(index >= CALL_N) return;

    app->book_sel = FlipperHamBookIndexBase + index;
    app->book_call_index = index;
    app->book_action_sel = FlipperHamC2IndexEdit;
    snprintf(app->c2_h, sizeof(app->c2_h), "%s", app->calls[index]);
    book_action_menu_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewC2);
}

void call_pick(void* context, InputType input_type, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t i;
    uint8_t s;
    bool d;
    char a[CALL_LEN];

    i = index - FlipperHamCallIndexBase;
    if(i >= CALL_N) return;

    if(input_type == InputTypeShort) {
        app->call_sel = index;
        if(app->tx_type == 2) {
            app->dst_call_index = i;
            app->return_view = FlipperHamViewMessage;

            if(call_split(app->calls[i], a, &s, &d)) {
                if(d) {
                    app->dst_ssid = s;
                    app->send_requested = true;
                    view_dispatcher_stop(app->view_dispatcher);
                } else {
                    ssidfix(app);
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewSsid);
                }
            }

            return;
        }
    }

    if(input_type != InputTypeLong) return;

    app->call_sel = index;
    app->edit_call_index = i;
    snprintf(app->c_edit, sizeof(app->c_edit), "%s", app->calls[i]);
    app->text_mode = 2;
    app->text_view = FlipperHamViewCall;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Edit callsign");
    text_input_set_result_callback(
        app->text_input, call_save, app, app->c_edit, sizeof(app->c_edit), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
}

void call_save(void* context) {
    FlipperHamApp* app = context;
    uint8_t i, j;

    i = app->edit_call_index;
    if(i >= CALL_N) return;

    if(!app->c_edit[0]) {
        app->calls[i][0] = 0;
        app->calls_used[i] = 0;
    } else {
        if(!call_validate(app->c_edit)) {
            if(app->text_mode == 4)
                app->book_sel = FlipperHamBookIndexBase + i;
            else
                app->call_sel = FlipperHamCallIndexBase + i;
            if(app->text_mode == 4)
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewBook);
            else
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewCall);
            return;
        }

        snprintf(app->calls[i], sizeof(app->calls[i]), "%s", app->c_edit);
        app->calls_used[i] = 1;
        if(app->text_mode == 4)
            app->book_sel = FlipperHamBookIndexBase + i;
        else
            app->call_sel = FlipperHamCallIndexBase + i;
    }

    if(!app->c_edit[0]) {
        if(app->text_mode == 4) {
            app->book_sel = FlipperHamBookIndexAdd;
            for(j = i; j < CALL_N; j++)
                if(app->calls_used[j])
                    if(app->calls[j][0]) {
                        app->book_sel = FlipperHamBookIndexBase + j;
                        break;
                    }
            if(app->book_sel == FlipperHamBookIndexAdd)
                for(j = i; j > 0; j--)
                    if(app->calls_used[j - 1])
                        if(app->calls[j - 1][0]) {
                            app->book_sel = FlipperHamBookIndexBase + j - 1;
                            break;
                        }
        } else {
            app->call_sel = FlipperHamCallIndexAdd;
            for(j = i; j < CALL_N; j++)
                if(app->calls_used[j])
                    if(app->calls[j][0]) {
                        app->call_sel = FlipperHamCallIndexBase + j;
                        break;
                    }
            if(app->call_sel == FlipperHamCallIndexAdd)
                for(j = i; j > 0; j--)
                    if(app->calls_used[j - 1])
                        if(app->calls[j - 1][0]) {
                            app->call_sel = FlipperHamCallIndexBase + j - 1;
                            break;
                        }
        }
    }

    calls_fix(app);
    cfgsave(app);
    callbook_save_txt(app);
    call_menu_build(app);
    book_menu_build(app);

    if(app->text_mode == 4)
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewBook);
    else
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewCall);
}
