#pragma once

#include <stdbool.h>

typedef struct View View;

typedef struct {
    bool sound_enabled;
    bool vibro_enabled;
    bool flipped_enabled;
} StrataHeroSettings;

void stratahero_load_settings(StrataHeroSettings* settings);
void stratahero_save_settings(StrataHeroSettings* settings);

typedef void (*StrataHeroSettingsNavigationCallback)(void*);
typedef void (*StrataHeroSettingsSettingsChangedCallback)(StrataHeroSettings*, void*);

typedef struct StrataHeroSettingsWidget StrataHeroSettingsWidget;

StrataHeroSettingsWidget* stratahero_settings_widget_alloc();
void stratahero_settings_widget_free(StrataHeroSettingsWidget* widget);

View* stratahero_settings_widget_get_view(StrataHeroSettingsWidget* widget);
View* stratahero_settings_widget_get_confirmation_view(StrataHeroSettingsWidget* widget);
bool stratahero_settings_widget_has_pending_changes(StrataHeroSettingsWidget* widget);
StrataHeroSettings* stratahero_settings_widget_get_settings(StrataHeroSettingsWidget* widget);

void stratahero_settings_widget_set_settings(
    StrataHeroSettingsWidget* widget,
    StrataHeroSettings* settings);

void stratahero_settings_widget_set_navigation_callback(
    StrataHeroSettingsWidget* widget,
    StrataHeroSettingsNavigationCallback callback,
    void* context);

void stratahero_settings_widget_set_settings_changed_callback(
    StrataHeroSettingsWidget* widget,
    StrataHeroSettingsSettingsChangedCallback callback,
    void* context);
