#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <flipper_format/flipper_format.h>
#include <toolbox/path.h>

#define SUBGHZ_EDITOR_APP_PATH_FOLDER "/ext/subghz"
#define SUBGHZ_EDITOR_APP_EXTENSION   ".sub"
#define SUBGHZ_EDITOR_KEY_BUFFER_SIZE 64

typedef struct {
    uint32_t frequency;
    const char* str;
} SubGhzFreqPreset;

static const SubGhzFreqPreset subghz_presets[] = {
    {315000000, "315.00 MHz"},
    {318000000, "318.00 MHz"},
    {345000000, "345.00 MHz"},
    {390000000, "390.00 MHz"},
    {433075000, "433.07 MHz"},
    {433880000, "433.88 MHz"},
    {433920000, "433.92 MHz"},
    {434420000, "434.42 MHz"},
    {434775000, "434.77 MHz"},
    {868350000, "868.35 MHz"},
    {868850000, "868.85 MHz"},
    {915000000, "915.00 MHz"},
};

#define SUBGHZ_PRESETS_COUNT (sizeof(subghz_presets) / sizeof(SubGhzFreqPreset))

typedef enum {
    SubGhzEditorEventOpenFile = 100,
    SubGhzEditorEventAbout,
    SubGhzEditorEventSaveOverwrite,
    SubGhzEditorEventSaveCopy,
    SubGhzEditorEventCloseFile,
} SubGhzEditorCustomEvent;

typedef struct {
    FuriString* file_path;
    FuriString* file_name;
    FuriString* protocol;
    FuriString* preset_str;
    char key_buf[SUBGHZ_EDITOR_KEY_BUFFER_SIZE];
    char temp_key_buf[SUBGHZ_EDITOR_KEY_BUFFER_SIZE];
    char freq_input_buf[32];
    char custom_freq_str[32];
    uint32_t frequency;
    uint32_t initial_frequency;
    uint8_t current_freq_index;
    bool is_custom_freq;
    bool is_file_loaded;
    bool has_key;
    bool has_frequency;
    bool modified;
} SubGhzEditorModel;

typedef enum {
    SubGhzEditorViewVariableList,
    SubGhzEditorViewTextInput,
    SubGhzEditorViewWidget,
} SubGhzEditorViewId;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    VariableItemList* var_list;
    TextInput* text_input;
    Widget* widget;
    SubGhzEditorModel model;
} SubGhzEditorApp;

void subghz_editor_model_init(SubGhzEditorModel* model);
void subghz_editor_model_free(SubGhzEditorModel* model);
void subghz_editor_model_reset(SubGhzEditorModel* model);
