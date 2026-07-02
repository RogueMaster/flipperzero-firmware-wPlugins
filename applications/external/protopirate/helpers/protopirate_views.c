#include "protopirate_views.h"
#include "../protopirate_app_i.h"

#include <furi.h>

#define TAG "ProtoPirateViews"

bool protopirate_ensure_variable_item_list(ProtoPirateApp* app) {
    furi_check(app);
    if(app->variable_item_list) {
        return true;
    }

    app->variable_item_list = variable_item_list_alloc();
    if(!app->variable_item_list) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher,
        ProtoPirateViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    return true;
}

bool protopirate_ensure_widget(ProtoPirateApp* app) {
    furi_check(app);
    if(app->widget) {
        return true;
    }

    app->widget = widget_alloc();
    if(!app->widget) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher, ProtoPirateViewWidget, widget_get_view(app->widget));
    return true;
}

bool protopirate_ensure_text_input(ProtoPirateApp* app) {
    furi_check(app);
    if(app->text_input) {
        return true;
    }

    app->text_input = text_input_alloc();
    if(!app->text_input) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher, ProtoPirateViewTextInput, text_input_get_view(app->text_input));
    return true;
}

bool protopirate_ensure_view_about(ProtoPirateApp* app) {
    furi_check(app);
    if(app->view_about) {
        return true;
    }

    app->view_about = view_alloc();
    if(!app->view_about) {
        return false;
    }

    view_dispatcher_add_view(app->view_dispatcher, ProtoPirateViewAbout, app->view_about);
    return true;
}

bool protopirate_ensure_receiver_view(ProtoPirateApp* app) {
    furi_check(app);
    if(app->protopirate_receiver) {
        return true;
    }

    app->protopirate_receiver = protopirate_view_receiver_alloc(app->auto_save);
    if(!app->protopirate_receiver) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher,
        ProtoPirateViewReceiver,
        protopirate_view_receiver_get_view(app->protopirate_receiver));
    return true;
}

void protopirate_views_free(ProtoPirateApp* app) {
    furi_check(app);

    if(app->submenu) {
        FURI_LOG_D(TAG, "Removing submenu view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewSubmenu);
        submenu_free(app->submenu);
        app->submenu = NULL;
    }

    if(app->variable_item_list) {
        FURI_LOG_D(TAG, "Removing variable_item_list view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
        variable_item_list_free(app->variable_item_list);
        app->variable_item_list = NULL;
    }

    if(app->view_about) {
        FURI_LOG_D(TAG, "Removing about view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewAbout);
        view_free(app->view_about);
        app->view_about = NULL;
    }

    if(app->widget) {
        FURI_LOG_D(TAG, "Removing widget view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewWidget);
        widget_free(app->widget);
        app->widget = NULL;
    }

    if(app->text_input) {
        FURI_LOG_D(TAG, "Removing text_input view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewTextInput);
        text_input_free(app->text_input);
        app->text_input = NULL;
    }

    if(app->protopirate_receiver) {
        FURI_LOG_D(TAG, "Removing receiver view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewReceiver);
        protopirate_view_receiver_free(app->protopirate_receiver);
        app->protopirate_receiver = NULL;
    }
}
