#pragma once

#include <gui/view.h>
#include "types.h"

typedef struct StratagemListWidget StratagemListWidget;

StratagemListWidget* stratagem_list_widget_alloc();
void stratagem_list_widget_free(StratagemListWidget* widget);

View* stratagem_list_widget_get_view(StratagemListWidget* widget);
void stratagem_list_widget_set_stratagem_type(StratagemListWidget* widget, StratagemType type);
