#include "key_copier_icons.h"
#include "key_formats.h"
#include <applications/services/dialogs/dialogs.h>
#include <applications/services/storage/storage.h>
#include <flipper_format.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <math.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdbool.h>

#define TAG "KeyCopier"

#define BACKLIGHT_ON              1
#define KEY_COPIER_FILE_EXTENSION ".keycopy"
#define INCHES_PER_PX             0.00978

typedef enum {
    KeyCopierSubmenuIndexMeasure,
    KeyCopierSubmenuIndexConfigure,
    KeyCopierSubmenuIndexSave,
    KeyCopierSubmenuIndexLoad,
    KeyCopierSubmenuIndexAbout,
    KeyCopierSubmenuIndexQRCode,
} KeyCopierSubmenuIndex;

typedef enum {
    KeyCopierViewSubmenu,
    KeyCopierViewTextInput,
    KeyCopierViewConfigure_i,
    KeyCopierViewConfigure_e,
    KeyCopierViewSave,
    KeyCopierViewLoad,
    KeyCopierViewMeasure,
    KeyCopierViewAbout,
    KeyCopierViewQRCode,
    KeyCopierViewManufacturerList,
    KeyCopierViewFormatList,
} KeyCopierView;

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* variable_item_list_config;
    View* view_measure;
    View* view_config_e;
    View* view_save;
    View* view_load;
    Widget* widget_about;
    Widget* widget_qr_code;
    VariableItem* key_name_item;
    VariableItem* format_item;
    VariableItem* format_name_item;
    char* temp_buffer;
    uint32_t temp_buffer_size;

    DialogsApp* dialogs;
    FuriString* file_path;
    Submenu* manufacturer_list;
    Submenu* format_list;
    char* selected_manufacturer;
} KeyCopierApp;

typedef struct {
    uint32_t format_index;
    FuriString* key_name_str;
    uint8_t pin_slc; // The pin that is being adjusted
    uint8_t* depth; // The cutting depth
    bool data_loaded;
    KeyFormat format;
} KeyCopierModel;

static inline int min(int a, int b) {
    return (a < b) ? a : b;
}

static inline int max(int a, int b) {
    return (a > b) ? a : b;
}
