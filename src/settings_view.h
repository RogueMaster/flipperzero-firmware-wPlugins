#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H

#include "app.h"

#include <gui/modules/variable_item_list.h>
#include <gui/view.h>

typedef struct SettingsView SettingsView;

SettingsView* settings_view_alloc(App* app);
void settings_view_free(SettingsView* settings_view);
View* settings_view_get_view(SettingsView* settings_view);
VariableItemList* settings_view_get_list(SettingsView* settings_view);
void settings_view_sync_from_app(SettingsView* settings_view);

#endif /* SETTINGS_VIEW_H */
