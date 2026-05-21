#pragma once

#include <stdbool.h>

typedef struct View View;

typedef struct {
    bool sound_enabled;
    bool vibro_enabled;
} StrataHeroSettings;

void stratahero_load_settings(StrataHeroSettings* settings);
void stratahero_save_settings(StrataHeroSettings* settings);

typedef struct StrataHeroSettingsWidget StrataHeroSettingsWidget;

StrataHeroSettingsWidget* stratahero_settings_widget_alloc();
void stratahero_settings_widget_free(StrataHeroSettingsWidget* widget);

View* stratahero_settings_widget_get_view(StrataHeroSettingsWidget* widget);
StrataHeroSettings* stratahero_settings_widget_get_settings(StrataHeroSettingsWidget* widget);
void stratahero_settings_widget_set_settings(StrataHeroSettingsWidget* widget, StrataHeroSettings* settings);
