/**
 * @file main_menu.h
 * GUI: MainMenu view module API
 */

#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/** MainMenu anonymous structure */
typedef struct MainMenu MainMenu;
typedef void (*MainMenuItemCallback)(void* context, uint32_t index);

/** Allocate and initialize main menu 
 * 
 * This main menu is used to select one option
 *
 * @return     MainMenu instance
 */
MainMenu* main_menu_alloc(void);

/** Deinitialize and free main menu
 *
 * @param      main_menu  MainMenu instance
 */
void main_menu_free(MainMenu* main_menu);

/** Get main menu view
 *
 * @param      main_menu  MainMenu instance
 *
 * @return     View instance that can be used for embedding
 */
View* main_menu_get_view(MainMenu* main_menu);

/** Add item to main menu
 *
 * @param      main_menu         MainMenu instance
 * @param      label             menu item label
 * @param      index             menu item index, used for callback, may be
 *                               the same with other items
 * @param      callback          menu item callback
 * @param      callback_context  menu item callback context
 */
void main_menu_add_item(
    MainMenu* main_menu,
    const char* label,
    uint32_t index,
    MainMenuItemCallback callback,
    void* callback_context);

/** Add lockable item to main menu
 *
 * @param      main_menu         MainMenu instance
 * @param      label             menu item label
 * @param      index             menu item index, used for callback, may be
 *                               the same with other items
 * @param      callback          menu item callback
 * @param      callback_context  menu item callback context
 * @param      locked            menu item locked status
 * @param      locked_message    menu item locked message
 */
void main_menu_add_lockable_item(
    MainMenu* main_menu,
    const char* label,
    uint32_t index,
    MainMenuItemCallback callback,
    void* callback_context,
    bool locked,
    const char* locked_message);

/** Change label of an existing item
 * 
 * @param      main_menu  MainMenu instance
 * @param      index      The index of the item
 * @param      label      The new label
 */
void main_menu_change_item_label(MainMenu* main_menu, uint32_t index, const char* label);

/** Remove all items from main menu
 *
 * @param      main_menu  MainMenu instance
 */
void main_menu_reset(MainMenu* main_menu);

/** Get main menu selected item index
 *
 * @param      main_menu  MainMenu instance
 *
 * @return     Index of the selected item
 */
uint32_t main_menu_get_selected_item(MainMenu* main_menu);

/** Set main menu selected item by index
 *
 * @param      main_menu  MainMenu instance
 * @param      index      The index of the selected item
 */
void main_menu_set_selected_item(MainMenu* main_menu, uint32_t index);

/** Set optional header for main menu
 * Must be called before adding items OR after adding items and before set_selected_item()
 *
 * @param      main_menu  MainMenu instance
 * @param      header     header to set
 */
void main_menu_set_header(MainMenu* main_menu, const char* header);

/** Set main menu orientation
 *
 * @param      main_menu  MainMenu instance
 * @param      orientation  either vertical or horizontal
 */
void main_menu_set_orientation(MainMenu* main_menu, ViewOrientation orientation);

void main_menu_set_help_callback(MainMenu* main_menu, MainMenuItemCallback callback, void* context);

#ifdef __cplusplus
}
#endif
