#pragma once

#include <gui/view.h>
#include "types.h"

typedef struct StratagemTypesWidget StratagemTypesWidget;
typedef void (*StratagemTypeSelectedCallback)(StratagemType type, void* context);

StratagemTypesWidget* stratagem_types_widget_alloc();
void stratagem_types_widget_free(StratagemTypesWidget* widget);
View* stratagem_types_widget_get_view(StratagemTypesWidget* widget);
void stratagem_types_widget_set_selected_callback(
    StratagemTypesWidget* widget,
    StratagemTypeSelectedCallback callback,
    void* context
);

typedef struct StratagemListWidget StratagemListWidget;

StratagemListWidget* stratagem_list_widget_alloc();
void stratagem_list_widget_free(StratagemListWidget* widget);

View* stratagem_list_widget_get_view(StratagemListWidget* widget);
void stratagem_list_widget_set_stratagem_type(StratagemListWidget* widget, StratagemType type);
