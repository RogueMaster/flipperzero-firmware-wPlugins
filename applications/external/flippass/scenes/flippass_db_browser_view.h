/**
 * @file flippass_db_browser_view.h
 * @brief Custom browser view for FlipPass database groups and entries.
 */
#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_DB_BROWSER_MAX_ITEMS   48U
#define FLIPPASS_DB_BROWSER_LABEL_SIZE  64U
#define FLIPPASS_DB_BROWSER_HEADER_SIZE 96U

typedef enum {
    FlipPassDbBrowserItemTypeGroup = 0,
    FlipPassDbBrowserItemTypeEntry,
    FlipPassDbBrowserItemTypeField,
    FlipPassDbBrowserItemTypeInfo,
} FlipPassDbBrowserItemType;

typedef enum {
    FlipPassDbBrowserModeBrowse = 0,
    FlipPassDbBrowserModeDirectActions,
} FlipPassDbBrowserMode;

typedef enum {
    FlipPassDbBrowserActionAutoType = 0,
    FlipPassDbBrowserActionPassword,
    FlipPassDbBrowserActionUsername,
    FlipPassDbBrowserActionOther,
    FlipPassDbBrowserActionCount,
} FlipPassDbBrowserAction;

typedef enum {
    FlipPassDbBrowserEventEnter = 1,
    FlipPassDbBrowserEventBack,
    FlipPassDbBrowserEventOpenActionMenu,
    FlipPassDbBrowserEventShow,
    FlipPassDbBrowserEventTypeUsb,
    FlipPassDbBrowserEventTypeBluetooth,
    FlipPassDbBrowserEventOpenOther,
} FlipPassDbBrowserEvent;

typedef void (*FlipPassDbBrowserViewCallback)(FlipPassDbBrowserEvent event, void* context);

typedef struct FlipPassDbBrowserView FlipPassDbBrowserView;

FlipPassDbBrowserView* flippass_db_browser_view_alloc(void);
void flippass_db_browser_view_free(FlipPassDbBrowserView* browser);
View* flippass_db_browser_view_get_view(FlipPassDbBrowserView* browser);
void flippass_db_browser_view_set_callback(
    FlipPassDbBrowserView* browser,
    FlipPassDbBrowserViewCallback callback,
    void* context);
void flippass_db_browser_view_reset(FlipPassDbBrowserView* browser);
void flippass_db_browser_view_set_header(FlipPassDbBrowserView* browser, const char* header);
void flippass_db_browser_view_set_has_parent(FlipPassDbBrowserView* browser, bool has_parent);
void flippass_db_browser_view_set_mode(
    FlipPassDbBrowserView* browser,
    FlipPassDbBrowserMode mode);
void flippass_db_browser_view_add_item(
    FlipPassDbBrowserView* browser,
    FlipPassDbBrowserItemType type,
    const char* label);
void flippass_db_browser_view_set_selected_item(FlipPassDbBrowserView* browser, uint32_t index);
uint32_t flippass_db_browser_view_get_selected_item(const FlipPassDbBrowserView* browser);
void flippass_db_browser_view_set_action_selected(
    FlipPassDbBrowserView* browser,
    uint32_t index);
uint32_t flippass_db_browser_view_get_action_selected(const FlipPassDbBrowserView* browser);
void flippass_db_browser_view_set_action_menu_open(
    FlipPassDbBrowserView* browser,
    bool open);
bool flippass_db_browser_view_is_action_menu_open(const FlipPassDbBrowserView* browser);
FlipPassDbBrowserItemType flippass_db_browser_view_get_selected_type(
    const FlipPassDbBrowserView* browser);

#ifdef __cplusplus
}
#endif
