#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../flippass.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OTHER_FIELDS_PLUGIN_APP_ID      "flippass_other_fields"
#define FLIPPASS_OTHER_FIELDS_PLUGIN_API_VERSION 1u
#define FLIPPASS_OTHER_FIELDS_HOST_API_VERSION   1u
#define FLIPPASS_OTHER_FIELDS_LABEL_SIZE         64U

typedef enum {
    FlipPassOtherFieldsPluginViewItemInfo = 0,
    FlipPassOtherFieldsPluginViewItemField,
    FlipPassOtherFieldsPluginViewItemAdd,
} FlipPassOtherFieldsPluginViewItemType;

typedef struct {
    uint32_t field_mask;
    KDBXCustomField* custom_field;
    FlipPassEditorCustomFieldDraft* draft_field;
    FlipPassOtpKind otp_kind;
    bool new_field;
    char label[FLIPPASS_OTHER_FIELDS_LABEL_SIZE];
} FlipPassOtherFieldsSelectionV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*add_item)(void* context, FlipPassOtherFieldsPluginViewItemType type, const char* label);
} FlipPassOtherFieldsHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*render_type_list)(
        const KDBXEntry* entry,
        uint32_t selected_index,
        const FlipPassOtherFieldsHostApiV1* host_api,
        uint32_t* out_selected_index,
        FuriString* error);
    bool (*render_editor_list)(
        KDBXCustomField* custom_fields,
        FlipPassEditorCustomFieldDraft* draft_fields,
        uint32_t selected_index,
        const FlipPassOtherFieldsHostApiV1* host_api,
        uint32_t* out_selected_index,
        FuriString* error);
    bool (*select)(uint32_t selected_index, FlipPassOtherFieldsSelectionV1* out_selection);
    size_t (*count)(void);
    void (*release)(void);
} FlipPassOtherFieldsPluginV1;

const FlipperAppPluginDescriptor* flippass_other_fields_plugin_ep(void);

#ifdef __cplusplus
}
#endif
