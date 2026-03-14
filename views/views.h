#pragma once

/* View IDs for ViewDispatcher */
typedef enum {
    ViewMain,
    ViewMainMenu,
    ViewSettings,
    ViewsCount
} AppViews;

/* Main sensor display view */
void view_main_alloc(void);
void view_main_switch(void);
void view_main_free(void);

/* Main menu view */
void view_main_menu_alloc(void);
void view_main_menu_switch(void);
void view_main_menu_free(void);

/* Settings view */
void view_settings_alloc(void);
void view_settings_switch(void);
void view_settings_free(void);
