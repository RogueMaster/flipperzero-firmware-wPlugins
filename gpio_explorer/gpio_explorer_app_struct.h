#ifndef GPIO_EXPLORER_APP_STRUCT_H
#define GPIO_EXPLORER_APP_STRUCT_H

#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include "gpio_explorer_configuration_struct.h"

struct gpio_explorer_app_struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    VariableItemList* variable_item_list_config;
    View* view_rgb;
    View* view_led;
    View* view_gpio_reader;
    Widget* widget_about;
    struct gpio_explorer_configure_struct* configuration_settings;
    FuriTimer* timer;
} ;
#endif //GPIO_EXPLORER_APP_STRUCT_H