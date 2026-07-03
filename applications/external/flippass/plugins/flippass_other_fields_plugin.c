#include "flippass_other_fields_plugin.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FLIPPASS_OTHER_FIELDS_MAX_ITEMS 64U

typedef struct {
    uint32_t field_mask;
    KDBXCustomField* custom_field;
    FlipPassEditorCustomFieldDraft* draft_field;
    FlipPassOtpKind otp_kind;
    bool new_field;
    char label[FLIPPASS_OTHER_FIELDS_LABEL_SIZE];
} FlipPassOtherFieldsItem;

static FlipPassOtherFieldsItem* flippass_other_fields_items = NULL;
static size_t flippass_other_fields_item_count = 0U;

static bool flippass_other_fields_key_is_otp_reserved(const char* key) {
    static const char* reserved[] = {
        "HmacOtp-Secret",
        "HmacOtp-Secret-Hex",
        "HmacOtp-Secret-Base32",
        "HmacOtp-Secret-Base64",
        "HmacOtp-Counter",
        "TimeOtp-Secret",
        "TimeOtp-Secret-Hex",
        "TimeOtp-Secret-Base32",
        "TimeOtp-Secret-Base64",
        "TimeOtp-Length",
        "TimeOtp-Period",
        "TimeOtp-Algorithm",
    };

    for(size_t index = 0U; index < COUNT_OF(reserved); index++) {
        if(key != NULL && strcmp(key, reserved[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool
    flippass_other_fields_entry_has_otp_kind(const KDBXEntry* entry, FlipPassOtpKind kind) {
    static const char* hmac_secret_fields[] = {
        "HmacOtp-Secret",
        "HmacOtp-Secret-Hex",
        "HmacOtp-Secret-Base32",
        "HmacOtp-Secret-Base64",
    };
    static const char* time_secret_fields[] = {
        "TimeOtp-Secret",
        "TimeOtp-Secret-Hex",
        "TimeOtp-Secret-Base32",
        "TimeOtp-Secret-Base64",
    };
    const char* const* fields = (kind == FlipPassOtpKindHmac) ? hmac_secret_fields :
                                (kind == FlipPassOtpKindTime) ? time_secret_fields :
                                                                NULL;
    const size_t count = (kind == FlipPassOtpKindHmac) ? COUNT_OF(hmac_secret_fields) :
                         (kind == FlipPassOtpKindTime) ? COUNT_OF(time_secret_fields) :
                                                         0U;

    for(const KDBXCustomField* field = entry != NULL ? entry->custom_fields : NULL; field != NULL;
        field = field->next) {
        for(size_t index = 0U; index < count; index++) {
            if(field->key != NULL && strcmp(field->key, fields[index]) == 0) {
                return true;
            }
        }
    }
    return false;
}

static const char* flippass_other_fields_otp_label(FlipPassOtpKind kind) {
    return kind == FlipPassOtpKindHmac ? "HMACOTP" : "TIMEOTP";
}

static bool flippass_other_fields_items_ensure(void) {
    if(flippass_other_fields_items != NULL) {
        return true;
    }

    flippass_other_fields_items =
        malloc(sizeof(FlipPassOtherFieldsItem) * FLIPPASS_OTHER_FIELDS_MAX_ITEMS);
    return flippass_other_fields_items != NULL;
}

static void flippass_other_fields_plugin_release(void) {
    free(flippass_other_fields_items);
    flippass_other_fields_items = NULL;
    flippass_other_fields_item_count = 0U;
}

static void flippass_other_fields_add_view_item(
    const FlipPassOtherFieldsHostApiV1* host_api,
    FlipPassOtherFieldsPluginViewItemType type,
    const char* label) {
    if(host_api != NULL && host_api->add_item != NULL) {
        host_api->add_item(host_api->context, type, label);
    }
}

static void flippass_other_fields_reset_items(void) {
    if(flippass_other_fields_items != NULL) {
        memset(
            flippass_other_fields_items,
            0,
            sizeof(FlipPassOtherFieldsItem) * FLIPPASS_OTHER_FIELDS_MAX_ITEMS);
    }
    flippass_other_fields_item_count = 0U;
}

static void flippass_other_fields_add_item(
    uint32_t field_mask,
    KDBXCustomField* custom_field,
    FlipPassEditorCustomFieldDraft* draft_field,
    FlipPassOtpKind otp_kind,
    const char* label,
    bool new_field) {
    if(flippass_other_fields_items == NULL ||
       flippass_other_fields_item_count >= FLIPPASS_OTHER_FIELDS_MAX_ITEMS) {
        return;
    }

    FlipPassOtherFieldsItem* item = &flippass_other_fields_items[flippass_other_fields_item_count];
    item->field_mask = field_mask;
    item->custom_field = custom_field;
    item->draft_field = draft_field;
    item->otp_kind = otp_kind;
    item->new_field = new_field;
    snprintf(item->label, sizeof(item->label), "%s", label != NULL ? label : "");
    flippass_other_fields_item_count++;
}

static bool flippass_other_fields_validate_host(
    const FlipPassOtherFieldsHostApiV1* host_api,
    FuriString* error) {
    if(host_api == NULL || host_api->api_version != FLIPPASS_OTHER_FIELDS_HOST_API_VERSION ||
       host_api->add_item == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass received an invalid other-fields host API.");
        }
        return false;
    }

    return true;
}

static bool flippass_other_fields_render_type_list(
    const KDBXEntry* entry,
    uint32_t selected_index,
    const FlipPassOtherFieldsHostApiV1* host_api,
    uint32_t* out_selected_index,
    FuriString* error) {
    if(out_selected_index == NULL || !flippass_other_fields_validate_host(host_api, error)) {
        return false;
    }

    if(!flippass_other_fields_items_ensure()) {
        furi_string_set_str(error, "Not enough RAM is available to list fields.");
        return false;
    }

    flippass_other_fields_reset_items();

    if(flippass_other_fields_entry_has_otp_kind(entry, FlipPassOtpKindTime)) {
        flippass_other_fields_add_item(
            0U,
            NULL,
            NULL,
            FlipPassOtpKindTime,
            flippass_other_fields_otp_label(FlipPassOtpKindTime),
            false);
    }

    if(flippass_other_fields_entry_has_otp_kind(entry, FlipPassOtpKindHmac)) {
        flippass_other_fields_add_item(
            0U,
            NULL,
            NULL,
            FlipPassOtpKindHmac,
            flippass_other_fields_otp_label(FlipPassOtpKindHmac),
            false);
    }

    if(entry != NULL && (entry->field_mask & KDBXEntryFieldUrl) != 0U) {
        flippass_other_fields_add_item(
            KDBXEntryFieldUrl, NULL, NULL, FlipPassOtpKindNone, "URL", false);
    }

    if(entry != NULL && (entry->field_mask & KDBXEntryFieldNotes) != 0U) {
        flippass_other_fields_add_item(
            KDBXEntryFieldNotes, NULL, NULL, FlipPassOtpKindNone, "Notes", false);
    }

    for(KDBXCustomField* field = entry != NULL ? entry->custom_fields : NULL; field != NULL;
        field = field->next) {
        if(!flippass_other_fields_key_is_otp_reserved(field->key)) {
            flippass_other_fields_add_item(
                0U, field, NULL, FlipPassOtpKindNone, field->key, false);
        }
    }

    for(KDBXCustomField* field = entry != NULL ? entry->custom_fields : NULL; field != NULL;
        field = field->next) {
        if(flippass_other_fields_key_is_otp_reserved(field->key)) {
            flippass_other_fields_add_item(
                0U, field, NULL, FlipPassOtpKindNone, field->key, false);
        }
    }

    if(flippass_other_fields_item_count == 0U) {
        flippass_other_fields_add_view_item(
            host_api, FlipPassOtherFieldsPluginViewItemInfo, "No other fields");
        *out_selected_index = 0U;
        return true;
    }

    for(size_t index = 0U; index < flippass_other_fields_item_count; index++) {
        flippass_other_fields_add_view_item(
            host_api,
            FlipPassOtherFieldsPluginViewItemField,
            flippass_other_fields_items[index].label);
    }

    *out_selected_index = (selected_index >= flippass_other_fields_item_count) ? 0U :
                                                                                 selected_index;
    return true;
}

static bool flippass_other_fields_render_editor_list(
    KDBXCustomField* custom_fields,
    FlipPassEditorCustomFieldDraft* draft_fields,
    uint32_t selected_index,
    const FlipPassOtherFieldsHostApiV1* host_api,
    uint32_t* out_selected_index,
    FuriString* error) {
    if(out_selected_index == NULL || !flippass_other_fields_validate_host(host_api, error)) {
        return false;
    }

    if(!flippass_other_fields_items_ensure()) {
        furi_string_set_str(error, "Not enough RAM is available to list fields.");
        return false;
    }

    flippass_other_fields_reset_items();

    if(draft_fields != NULL) {
        for(FlipPassEditorCustomFieldDraft* draft = draft_fields; draft != NULL;
            draft = draft->next) {
            const char* label = draft->name != NULL ? draft->name : "Unnamed Field";
            if(!flippass_other_fields_key_is_otp_reserved(draft->name)) {
                flippass_other_fields_add_item(0U, NULL, draft, FlipPassOtpKindNone, label, false);
                flippass_other_fields_add_view_item(
                    host_api, FlipPassOtherFieldsPluginViewItemField, label);
            }
        }
    } else {
        for(KDBXCustomField* field = custom_fields; field != NULL; field = field->next) {
            const char* label = field->key != NULL ? field->key : "Unnamed Field";
            if(!flippass_other_fields_key_is_otp_reserved(field->key)) {
                flippass_other_fields_add_item(0U, field, NULL, FlipPassOtpKindNone, label, false);
                flippass_other_fields_add_view_item(
                    host_api, FlipPassOtherFieldsPluginViewItemField, label);
            }
        }
    }

    flippass_other_fields_add_item(0U, NULL, NULL, FlipPassOtpKindNone, "New Field", true);
    flippass_other_fields_add_view_item(
        host_api, FlipPassOtherFieldsPluginViewItemAdd, "New Field");

    *out_selected_index = (selected_index >= flippass_other_fields_item_count) ? 0U :
                                                                                 selected_index;
    return true;
}

static bool flippass_other_fields_plugin_select(
    uint32_t selected_index,
    FlipPassOtherFieldsSelectionV1* out_selection) {
    if(out_selection == NULL || selected_index >= flippass_other_fields_item_count ||
       flippass_other_fields_items == NULL) {
        return false;
    }

    const FlipPassOtherFieldsItem* item = &flippass_other_fields_items[selected_index];
    memset(out_selection, 0, sizeof(*out_selection));
    out_selection->field_mask = item->field_mask;
    out_selection->custom_field = item->custom_field;
    out_selection->draft_field = item->draft_field;
    out_selection->otp_kind = item->otp_kind;
    out_selection->new_field = item->new_field;
    snprintf(out_selection->label, sizeof(out_selection->label), "%s", item->label);
    return true;
}

static size_t flippass_other_fields_plugin_count(void) {
    return flippass_other_fields_item_count;
}

static const FlipPassOtherFieldsPluginV1 flippass_other_fields_plugin = {
    .api_version = FLIPPASS_OTHER_FIELDS_PLUGIN_API_VERSION,
    .render_type_list = flippass_other_fields_render_type_list,
    .render_editor_list = flippass_other_fields_render_editor_list,
    .select = flippass_other_fields_plugin_select,
    .count = flippass_other_fields_plugin_count,
    .release = flippass_other_fields_plugin_release,
};

static const FlipperAppPluginDescriptor flippass_other_fields_descriptor = {
    .appid = FLIPPASS_OTHER_FIELDS_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OTHER_FIELDS_PLUGIN_API_VERSION,
    .entry_point = &flippass_other_fields_plugin,
};

const FlipperAppPluginDescriptor* flippass_other_fields_plugin_ep(void) {
    return &flippass_other_fields_descriptor;
}
