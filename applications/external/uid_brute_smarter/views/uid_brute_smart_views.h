#ifndef UID_BRUTE_SMART_VIEWS_H
#define UID_BRUTE_SMART_VIEWS_H

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>

#include "../pattern_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UidBruteSmart UidBruteSmart;

// View IDs
typedef enum {
    UidBruteSmartViewSplash,
    UidBruteSmartViewCardLoader,
    UidBruteSmartViewPattern,
    UidBruteSmartViewConfig,
    UidBruteSmartViewBrute,
    UidBruteSmartViewMenu
} UidBruteSmartView;

// Scene IDs
typedef enum {
    UidBruteSmartSceneSplash,
    UidBruteSmartSceneCardLoader,
    UidBruteSmartScenePattern,
    UidBruteSmartSceneConfig,
    UidBruteSmartSceneBrute,
    UidBruteSmartSceneMenu
} UidBruteSmartScene;

// View setup functions
void uid_brute_smart_view_setup_splash(UidBruteSmart* instance);
void uid_brute_smart_view_setup_menu(UidBruteSmart* instance);
void uid_brute_smart_view_setup_config(UidBruteSmart* instance);
void uid_brute_smart_view_setup_brute(UidBruteSmart* instance);

// View callback functions
uint32_t uid_brute_smart_view_exit_callback(void* context);
void uid_brute_smart_view_menu_callback(void* context, uint32_t index);
void uid_brute_smart_view_config_callback(void* context, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif // UID_BRUTE_SMART_VIEWS_H
