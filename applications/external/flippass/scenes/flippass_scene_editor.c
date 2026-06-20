#include "flippass_scene_editor.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "../kdbx/kdbx_constants.h"
#include "../kdbx/memzero.h"
#include "../plugins/flippass_editor_crud_plugin.h"
#include "../plugins/flippass_keyboard_layout_plugin.h"
#include "flippass_scene.h"
#include "flippass_scene_other_fields.h"
#include "flippass_scene_password_generator.h"
#include "flippass_scene_status.h"

#include <storage/storage.h>
#include <toolbox/path.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FlipPassEditorItemPrimaryText = 0,
    FlipPassEditorItemSecondaryText,
    FlipPassEditorItemPasswordText,
    FlipPassEditorItemCipher,
    FlipPassEditorItemCompression,
    FlipPassEditorItemKdfRounds,
    FlipPassEditorItemUsername,
    FlipPassEditorItemEntryPassword,
    FlipPassEditorItemUrl,
    FlipPassEditorItemNotes,
    FlipPassEditorItemAutotype,
    FlipPassEditorItemOtherFields,
    FlipPassEditorItemOtp,
    FlipPassEditorItemCommit,
    FlipPassEditorItemDelete,
};

enum {
    FlipPassEditorEventOtpRebuild = 0x300U,
};

typedef enum {
    FlipPassEditorDialogNone = 0,
    FlipPassEditorDialogDeleteGroup,
    FlipPassEditorDialogDeleteEntry,
    FlipPassEditorDialogDeleteField,
} FlipPassEditorDialogState;

enum {
    FlipPassEditorOtpRowType = 0U,
    FlipPassEditorOtpRowSecret,
    FlipPassEditorOtpRowEncoding,
    FlipPassEditorOtpRowCounter,
};

static void flippass_editor_enter_callback(void* context, uint32_t index);
static void flippass_editor_open_text_target(App* app, FlipPassEditorTextTarget target);

static void flippass_editor_free_custom_field_draft(FlipPassEditorCustomFieldDraft* draft) {
    if(draft == NULL) {
        return;
    }

    if(draft->name != NULL) {
        memzero(draft->name, strlen(draft->name));
        free(draft->name);
    }
    if(draft->value != NULL) {
        memzero(draft->value, strlen(draft->value));
        free(draft->value);
    }
    memzero(draft, sizeof(*draft));
    free(draft);
}

void flippass_editor_clear_custom_field_drafts(App* app) {
    furi_assert(app);

    FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields;
    while(draft != NULL) {
        FlipPassEditorCustomFieldDraft* next = draft->next;
        flippass_editor_free_custom_field_draft(draft);
        draft = next;
    }

    app->editor_custom_fields = NULL;
    app->editor_custom_field_draft = NULL;
}

size_t flippass_editor_custom_field_count(const App* app) {
    size_t count = 0U;

    if(app == NULL) {
        return 0U;
    }

    if(app->editor_mode == FlipPassEditorModeAddEntry ||
       (app->editor_mode == FlipPassEditorModeAddCustomField && app->editor_entry == NULL) ||
       (app->editor_mode == FlipPassEditorModeEditCustomField &&
        app->editor_custom_field_draft != NULL)) {
        for(const FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields; draft != NULL;
            draft = draft->next) {
            if(!flippass_otp_draft_is_reserved(draft)) {
                count++;
            }
        }
        return count;
    }

    const KDBXEntry* entry = app->editor_entry != NULL ? app->editor_entry : app->active_entry;
    for(const KDBXCustomField* field = entry != NULL ? entry->custom_fields : NULL; field != NULL;
        field = field->next) {
        if(!flippass_otp_custom_field_is_reserved(field->key)) {
            count++;
        }
    }

    return count;
}

static void flippass_editor_clear_context(App* app) {
    furi_assert(app);

    flippass_editor_clear_custom_field_drafts(app);
    app->editor_mode = FlipPassEditorModeNone;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_custom_field = NULL;
    app->editor_custom_field_draft = NULL;
    app->editor_custom_field_protected = false;
    app->editor_custom_field_name[0] = '\0';
    app->editor_custom_field_value[0] = '\0';
    app->editor_otp_kind = FlipPassOtpKindTime;
    app->editor_otp_secret_encoding = FlipPassOtpSecretEncodingBase32;
    app->editor_otp_algorithm = FlipPassOtpAlgorithmSha1;
    app->editor_otp_digits = FLIPPASS_OTP_DEFAULT_DIGITS;
    app->editor_otp_period = FLIPPASS_OTP_DEFAULT_PERIOD;
    app->editor_otp_time_zone_minutes = 0;
    app->editor_otp_settled = false;
    app->editor_otp_secret[0] = '\0';
    snprintf(
        app->editor_otp_counter,
        sizeof(app->editor_otp_counter),
        "%llu",
        (unsigned long long)FLIPPASS_OTP_DEFAULT_COUNTER);
    app->password_gen_target = FlipPassPasswordGenTargetNone;
    app->password_gen_capture_active = false;
    app->password_gen_auto_open_field_name = false;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_idle_lock_minutes = app->idle_lock_minutes;
    app->editor_idle_unlock_attempts = app->idle_unlock_attempts;
    app->editor_idle_exit_minutes = app->idle_exit_minutes;
    app->editor_always_allow_ext = app->always_allow_ext;
    app->editor_keyboard_layout_index = 0U;
    app->editor_keyboard_layout_use_alt = true;
    app->editor_keyboard_layout_available = false;
    app->editor_keyboard_layout_path[0] = '\0';
    app->editor_close_after_commit = false;
    memzero(app->editor_database_password, sizeof(app->editor_database_password));
}

static const char* flippass_editor_commit_label(const App* app) {
    switch(app->editor_mode) {
    case FlipPassEditorModeAddCustomField:
        return "Add";
    case FlipPassEditorModeNewDatabase:
    case FlipPassEditorModeNewDirectory:
    case FlipPassEditorModeAddGroup:
    case FlipPassEditorModeAddEntry:
        return "Create";
    case FlipPassEditorModeRenameFile:
        return "Rename";
    case FlipPassEditorModeModifyDatabase:
    case FlipPassEditorModeEditGroup:
    case FlipPassEditorModeEditEntry:
    case FlipPassEditorModeGlobalConfig:
        return "Save";
    case FlipPassEditorModeEditCustomField:
        return "Ok";
    default:
        return "Save";
    }
}

static void flippass_editor_make_preview(const char* value, bool secret, char out[24]) {
    const char* source = (value != NULL) ? value : "";
    size_t length = strlen(source);

    if(secret) {
        snprintf(out, 24U, "%s", length > 0U ? "Set" : "Empty");
        return;
    }

    if(length == 0U) {
        snprintf(out, 24U, "%s", "Empty");
        return;
    }

    if(length <= 20U) {
        snprintf(out, 24U, "%s", source);
        return;
    }

    snprintf(out, 24U, "%.20s...", source);
}

static VariableItem*
    flippass_editor_add_text_item(App* app, const char* label, const char* value, bool secret) {
    VariableItem* item = variable_item_list_add(app->variable_item_list, label, 1U, NULL, app);
    char preview[24];

    flippass_editor_make_preview(value, secret, preview);
    variable_item_set_current_value_text(item, preview);
    return item;
}

static VariableItem* flippass_editor_add_commit_item(App* app) {
    return flippass_editor_add_text_item(app, flippass_editor_commit_label(app), " ", false);
}

static bool flippass_editor_modify_database_uses_password(const App* app) {
    return app->editor_mode == FlipPassEditorModeModifyDatabase &&
           app->editor_return_scene == FlipPassScene_FileBrowser;
}

static bool flippass_editor_file_name_has_kdbx_extension(const char* name) {
    const char* extension = NULL;

    if(name == NULL) {
        return false;
    }

    extension = strrchr(name, '.');
    if(extension == NULL || strlen(extension) != 5U) {
        return false;
    }

    return (tolower((unsigned char)extension[1]) == 'k') &&
           (tolower((unsigned char)extension[2]) == 'd') &&
           (tolower((unsigned char)extension[3]) == 'b') &&
           (tolower((unsigned char)extension[4]) == 'x');
}

static void flippass_editor_compose_file_name(FuriString* out, const char* name) {
    furi_string_reset(out);
    if(name == NULL || name[0] == '\0') {
        return;
    }

    furi_string_set_str(out, name);
    if(!flippass_editor_file_name_has_kdbx_extension(name)) {
        furi_string_cat_str(out, ".kdbx");
    }
}

static void flippass_editor_compose_root_name(FuriString* out, const char* name) {
    const char* extension = NULL;

    furi_string_reset(out);
    if(name == NULL || name[0] == '\0') {
        furi_string_set_str(out, "Root");
        return;
    }

    furi_string_set_str(out, name);
    extension = strrchr(furi_string_get_cstr(out), '.');
    if(extension != NULL && flippass_editor_file_name_has_kdbx_extension(name)) {
        furi_string_left(out, (size_t)(extension - furi_string_get_cstr(out)));
    }

    if(furi_string_empty(out)) {
        furi_string_set_str(out, "Root");
    }
}

static uint32_t flippass_editor_commit_index(const App* app) {
    switch(app->editor_mode) {
    case FlipPassEditorModeNewDatabase:
        return 5U;
    case FlipPassEditorModeModifyDatabase:
        return flippass_editor_modify_database_uses_password(app) ? 4U : 3U;
    case FlipPassEditorModeNewDirectory:
    case FlipPassEditorModeAddGroup:
    case FlipPassEditorModeEditGroup:
    case FlipPassEditorModeRenameFile:
        return 1U;
    case FlipPassEditorModeAddEntry:
    case FlipPassEditorModeEditEntry:
        return FlipPassEditorEntryRowCommit;
    case FlipPassEditorModeEditOtp:
        return (app->editor_otp_kind == FlipPassOtpKindHmac) ? 4U : 7U;
    case FlipPassEditorModeAddCustomField:
    case FlipPassEditorModeEditCustomField:
        return 3U;
    case FlipPassEditorModeGlobalConfig:
        return 6U;
    case FlipPassEditorModeNone:
    default:
        return 0U;
    }
}

static uint32_t flippass_editor_last_index(const App* app) {
    switch(app->editor_mode) {
    case FlipPassEditorModeEditGroup:
        return 2U;
    case FlipPassEditorModeEditEntry:
        return FlipPassEditorEntryRowDelete;
    case FlipPassEditorModeEditOtp:
        return app->editor_otp_settled ? (flippass_editor_commit_index(app) + 1U) :
                                         flippass_editor_commit_index(app);
    case FlipPassEditorModeEditCustomField:
        return 4U;
    case FlipPassEditorModeGlobalConfig:
        return flippass_editor_commit_index(app);
    default:
        return flippass_editor_commit_index(app);
    }
}

static bool flippass_editor_is_commit_index(const App* app, uint32_t index) {
    return app->editor_mode != FlipPassEditorModeNone &&
           index == flippass_editor_commit_index(app);
}

static uint64_t flippass_editor_kdf_index_to_rounds(uint8_t index) {
    return FLIPPASS_KDBX_MIN_AES_KDF_ROUNDS + ((uint64_t)index * FLIPPASS_KDBX_AES_KDF_ROUND_STEP);
}

static uint8_t flippass_editor_kdf_rounds_to_index(uint64_t rounds) {
    uint64_t index = 0U;

    if(rounds < FLIPPASS_KDBX_MIN_AES_KDF_ROUNDS) {
        rounds = FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    }
    index = (rounds - FLIPPASS_KDBX_MIN_AES_KDF_ROUNDS + (FLIPPASS_KDBX_AES_KDF_ROUND_STEP / 2U)) /
            FLIPPASS_KDBX_AES_KDF_ROUND_STEP;
    if(index >= FLIPPASS_KDBX_AES_KDF_UI_VALUES) {
        index = FLIPPASS_KDBX_AES_KDF_UI_VALUES - 1U;
    }

    return (uint8_t)index;
}

static void flippass_editor_kdf_set_value_text(VariableItem* item, uint64_t rounds) {
    char text[16];

    snprintf(text, sizeof(text), "%luK", (unsigned long)(rounds / 1000ULL));
    variable_item_set_current_value_text(item, text);
}

static void flippass_editor_cipher_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->database_cipher = (index == 0U) ? FlipPassKdbxCipherAes256 : FlipPassKdbxCipherChaCha20;
    variable_item_set_current_value_text(item, index == 0U ? "AES-256" : "ChaCha20");
}

static void flippass_editor_compression_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->database_compression = (index == 0U) ? KDBX_COMPRESSION_GZIP : KDBX_COMPRESSION_NONE;
    variable_item_set_current_value_text(item, index == 0U ? "GZip" : "None");
}

static void flippass_editor_kdf_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->database_kdf_rounds = flippass_editor_kdf_index_to_rounds(index);
    flippass_editor_kdf_set_value_text(item, app->database_kdf_rounds);
}

static void flippass_editor_add_cipher_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Cipher", 2U, flippass_editor_cipher_change_callback, app);
    const uint8_t index = (app->database_cipher == FlipPassKdbxCipherChaCha20) ? 1U : 0U;
    variable_item_set_current_value_index(item, index);
    flippass_editor_cipher_change_callback(item);
}

static void flippass_editor_add_compression_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Compression",
        2U,
        flippass_editor_compression_change_callback,
        app);
    const uint8_t index = (app->database_compression == KDBX_COMPRESSION_NONE) ? 1U : 0U;
    variable_item_set_current_value_index(item, index);
    flippass_editor_compression_change_callback(item);
}

static void flippass_editor_add_kdf_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "AES-KDF",
        FLIPPASS_KDBX_AES_KDF_UI_VALUES,
        flippass_editor_kdf_change_callback,
        app);
    const uint8_t index = flippass_editor_kdf_rounds_to_index(app->database_kdf_rounds);
    variable_item_set_current_value_index(item, index);
    flippass_editor_kdf_change_callback(item);
}

static void flippass_editor_custom_field_protected_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_custom_field_protected = index != 0U;
    variable_item_set_current_value_text(item, app->editor_custom_field_protected ? "Yes" : "No");
}

static void flippass_editor_add_custom_field_protected_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Protected",
        2U,
        flippass_editor_custom_field_protected_change_callback,
        app);
    variable_item_set_current_value_index(item, app->editor_custom_field_protected ? 1U : 0U);
    flippass_editor_custom_field_protected_change_callback(item);
}

static const char* flippass_editor_otp_type_text(FlipPassOtpKind kind) {
    return kind == FlipPassOtpKindHmac ? "Hmac-Otp" : "Time-OTP";
}

static const char* flippass_editor_otp_encoding_text(FlipPassOtpSecretEncoding encoding) {
    switch(encoding) {
    case FlipPassOtpSecretEncodingText:
        return "Text";
    case FlipPassOtpSecretEncodingHex:
        return "Hex";
    case FlipPassOtpSecretEncodingBase32:
        return "Base32";
    case FlipPassOtpSecretEncodingBase64:
        return "Base64";
    default:
        return "Base32";
    }
}

static const char* flippass_editor_otp_algorithm_text(FlipPassOtpAlgorithm algorithm) {
    switch(algorithm) {
    case FlipPassOtpAlgorithmSha256:
        return "SHA-256";
    case FlipPassOtpAlgorithmSha512:
        return "SHA-512";
    case FlipPassOtpAlgorithmSha1:
    default:
        return "SHA-1";
    }
}

static void flippass_editor_otp_type_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_otp_kind = index == 0U ? FlipPassOtpKindTime : FlipPassOtpKindHmac;
    variable_item_set_current_value_text(
        item, flippass_editor_otp_type_text(app->editor_otp_kind));
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassEditorEventOtpRebuild);
}

static void flippass_editor_otp_encoding_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_otp_secret_encoding = (FlipPassOtpSecretEncoding)index;
    variable_item_set_current_value_text(
        item, flippass_editor_otp_encoding_text(app->editor_otp_secret_encoding));
}

static void flippass_editor_otp_digits_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_otp_digits = (uint8_t)(index + 1U);
    char text[4];
    snprintf(text, sizeof(text), "%u", (unsigned int)app->editor_otp_digits);
    variable_item_set_current_value_text(item, text);
}

static void flippass_editor_otp_period_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_otp_period = (uint32_t)index + 1U;
    char text[8];
    snprintf(text, sizeof(text), "%lus", (unsigned long)app->editor_otp_period);
    variable_item_set_current_value_text(item, text);
}

static void flippass_editor_otp_algorithm_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_otp_algorithm = (FlipPassOtpAlgorithm)index;
    variable_item_set_current_value_text(
        item, flippass_editor_otp_algorithm_text(app->editor_otp_algorithm));
}

static uint8_t flippass_editor_otp_time_zone_to_index(int16_t minutes) {
    if(minutes < FLIPPASS_OTP_TIME_ZONE_MIN_MINUTES ||
       minutes > FLIPPASS_OTP_TIME_ZONE_MAX_MINUTES ||
       (minutes % FLIPPASS_OTP_TIME_ZONE_STEP_MINUTES) != 0) {
        return (uint8_t)((0 - FLIPPASS_OTP_TIME_ZONE_MIN_MINUTES) /
                         FLIPPASS_OTP_TIME_ZONE_STEP_MINUTES);
    }

    return (uint8_t)((minutes - FLIPPASS_OTP_TIME_ZONE_MIN_MINUTES) /
                     FLIPPASS_OTP_TIME_ZONE_STEP_MINUTES);
}

static void flippass_editor_otp_time_zone_text(int16_t minutes, char* text, size_t text_size) {
    const char sign = minutes < 0 ? '-' : '+';
    const uint16_t absolute = (uint16_t)(minutes < 0 ? -minutes : minutes);
    snprintf(
        text,
        text_size,
        "%c%u:%02uh",
        sign,
        (unsigned int)(absolute / 60U),
        (unsigned int)(absolute % 60U));
}

static void flippass_editor_otp_time_zone_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    char text[10];

    app->editor_otp_time_zone_minutes =
        (int16_t)(FLIPPASS_OTP_TIME_ZONE_MIN_MINUTES +
                  ((int16_t)index * FLIPPASS_OTP_TIME_ZONE_STEP_MINUTES));
    flippass_editor_otp_time_zone_text(app->editor_otp_time_zone_minutes, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void flippass_editor_add_otp_type_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Type", 2U, flippass_editor_otp_type_change_callback, app);
    const uint8_t index = app->editor_otp_kind == FlipPassOtpKindHmac ? 1U : 0U;
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(
        item, flippass_editor_otp_type_text(app->editor_otp_kind));
}

static void flippass_editor_add_otp_encoding_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Encoding", 4U, flippass_editor_otp_encoding_change_callback, app);
    variable_item_set_current_value_index(item, (uint8_t)app->editor_otp_secret_encoding);
    variable_item_set_current_value_text(
        item, flippass_editor_otp_encoding_text(app->editor_otp_secret_encoding));
}

static void flippass_editor_add_otp_digits_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Length",
        FLIPPASS_OTP_MAX_DIGITS,
        flippass_editor_otp_digits_change_callback,
        app);
    const uint8_t index = (app->editor_otp_digits > 0U ? app->editor_otp_digits : 1U) - 1U;
    variable_item_set_current_value_index(item, index);
    flippass_editor_otp_digits_change_callback(item);
}

static void flippass_editor_add_otp_period_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Period", 255U, flippass_editor_otp_period_change_callback, app);
    uint32_t period = app->editor_otp_period;
    if(period == 0U || period > 255U) {
        period = FLIPPASS_OTP_DEFAULT_PERIOD;
    }
    variable_item_set_current_value_index(item, (uint8_t)(period - 1U));
    flippass_editor_otp_period_change_callback(item);
}

static void flippass_editor_add_otp_algorithm_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Algorithm",
        3U,
        flippass_editor_otp_algorithm_change_callback,
        app);
    variable_item_set_current_value_index(item, (uint8_t)app->editor_otp_algorithm);
    variable_item_set_current_value_text(
        item, flippass_editor_otp_algorithm_text(app->editor_otp_algorithm));
}

static void flippass_editor_add_otp_time_zone_item(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Time Zone",
        FLIPPASS_OTP_TIME_ZONE_COUNT,
        flippass_editor_otp_time_zone_change_callback,
        app);
    variable_item_set_current_value_index(
        item, flippass_editor_otp_time_zone_to_index(app->editor_otp_time_zone_minutes));
    flippass_editor_otp_time_zone_change_callback(item);
}

static const uint16_t flippass_editor_idle_lock_minutes[] = {
    0U,
    1U,
    2U,
    FLIPPASS_DEFAULT_IDLE_LOCK_MINUTES,
    5U,
    10U,
    15U,
    30U,
    0U,
};

static const uint16_t flippass_editor_idle_exit_minutes[] = {
    0U,
    5U,
    10U,
    FLIPPASS_DEFAULT_IDLE_EXIT_MINUTES,
    30U,
    60U,
    0U,
};

static const uint8_t flippass_editor_idle_unlock_attempts[] = {
    1U,
    3U,
    FLIPPASS_DEFAULT_IDLE_UNLOCK_ATTEMPTS,
    10U,
};

static void flippass_editor_minutes_text(uint16_t minutes, char* text, size_t text_size) {
    if(minutes == 0U) {
        snprintf(text, text_size, "%s", "Off");
    } else {
        snprintf(text, text_size, "%um", (unsigned int)minutes);
    }
}

static uint8_t flippass_editor_lock_minutes_to_index(uint16_t minutes) {
    for(uint8_t index = 0U; index < COUNT_OF(flippass_editor_idle_lock_minutes); index++) {
        if(flippass_editor_idle_lock_minutes[index] == minutes) {
            return index;
        }
    }
    return 2U;
}

static uint8_t flippass_editor_exit_minutes_to_index(uint16_t minutes) {
    for(uint8_t index = 0U; index < COUNT_OF(flippass_editor_idle_exit_minutes); index++) {
        if(flippass_editor_idle_exit_minutes[index] == minutes) {
            return index;
        }
    }
    return 2U;
}

static uint8_t flippass_editor_unlock_attempts_to_index(uint8_t attempts) {
    for(uint8_t index = 0U; index < COUNT_OF(flippass_editor_idle_unlock_attempts); index++) {
        if(flippass_editor_idle_unlock_attempts[index] == attempts) {
            return index;
        }
    }
    return 2U;
}

static void flippass_editor_config_lock_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    char text[8];

    app->editor_idle_lock_minutes = flippass_editor_idle_lock_minutes[index];
    flippass_editor_minutes_text(app->editor_idle_lock_minutes, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void flippass_editor_config_exit_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    char text[8];

    app->editor_idle_exit_minutes = flippass_editor_idle_exit_minutes[index];
    flippass_editor_minutes_text(app->editor_idle_exit_minutes, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void flippass_editor_config_attempts_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    char text[8];

    app->editor_idle_unlock_attempts = flippass_editor_idle_unlock_attempts[index];
    snprintf(text, sizeof(text), "%u", (unsigned int)app->editor_idle_unlock_attempts);
    variable_item_set_current_value_text(item, text);
}

static void flippass_editor_config_allow_ext_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_always_allow_ext = index != 0U;
    variable_item_set_current_value_text(item, app->editor_always_allow_ext ? "Yes" : "No");
}

static const char* flippass_editor_config_layout_get_current_path(void* host_context) {
    App* app = host_context;
    if(app == NULL || app->editor_keyboard_layout_use_alt ||
       app->editor_keyboard_layout_path[0] == '\0') {
        return "";
    }

    return app->editor_keyboard_layout_path;
}

static bool flippass_editor_config_layout_set_current_path(
    void* host_context,
    const char* path,
    bool use_alt_numpad) {
    App* app = host_context;
    if(app == NULL) {
        return false;
    }

    app->editor_keyboard_layout_use_alt = use_alt_numpad || path == NULL || path[0] == '\0';
    snprintf(
        app->editor_keyboard_layout_path,
        sizeof(app->editor_keyboard_layout_path),
        "%s",
        app->editor_keyboard_layout_use_alt ? "" : path);
    return true;
}

static void flippass_editor_config_layout_log(
    void* host_context,
    const char* module_name,
    const char* message) {
    App* app = host_context;
    if(app != NULL && module_name != NULL && message != NULL) {
        FLIPPASS_LOG_EVENT(app, "%s %s", module_name, message);
    }
}

static FlipPassKeyboardLayoutHostApiV1 flippass_editor_config_layout_host_api(App* app) {
    const FlipPassKeyboardLayoutHostApiV1 host_api = {
        .api_version = FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION,
        .host_context = app,
        .get_current_layout_path = flippass_editor_config_layout_get_current_path,
        .set_current_layout_path = flippass_editor_config_layout_set_current_path,
        .log = flippass_editor_config_layout_log,
    };
    return host_api;
}

static const FlipPassKeyboardLayoutPluginV1*
    flippass_editor_config_layout_plugin_loaded(const App* app) {
    if(app == NULL) {
        return NULL;
    }

    const FlipPassModuleInstance* instance =
        &app->module_loader.slot[FlipPassModuleSlotKeyboardLayout];
    return (instance->descriptor != NULL) ? instance->descriptor->entry_point : NULL;
}

static const FlipPassKeyboardLayoutPluginV1*
    flippass_editor_config_layout_plugin_ensure(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotKeyboardLayout,
        NULL,
        FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_APP_ID,
        FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        return NULL;
    }

    const FlipPassKeyboardLayoutPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_KEYBOARD_LAYOUT_PLUGIN_API_VERSION ||
       plugin->load_items == NULL || plugin->item_count == NULL || plugin->item_label == NULL ||
       plugin->selected_index == NULL || plugin->apply_selection == NULL ||
       plugin->reset == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "Keyboard layout plugin API mismatch.");
        }
        flippass_module_unload(app, FlipPassModuleSlotKeyboardLayout);
        return NULL;
    }

    return plugin;
}

static void flippass_editor_config_layout_unload_plugin(App* app) {
    const FlipPassKeyboardLayoutPluginV1* plugin =
        flippass_editor_config_layout_plugin_loaded(app);

    if(plugin != NULL && plugin->reset != NULL) {
        plugin->reset();
    }
    flippass_module_unload(app, FlipPassModuleSlotKeyboardLayout);
}

static void flippass_editor_config_layout_change_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    const FlipPassKeyboardLayoutPluginV1* plugin =
        flippass_editor_config_layout_plugin_loaded(app);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->editor_keyboard_layout_index = index;
    if(plugin == NULL || plugin->item_label == NULL) {
        variable_item_set_current_value_text(item, "Unavailable");
        return;
    }

    const char* label = plugin->item_label(index);
    if(label == NULL) {
        label = "Unknown";
    }
    variable_item_set_current_value_text(item, label);

    if(plugin->apply_selection != NULL) {
        const FlipPassKeyboardLayoutHostApiV1 host_api =
            flippass_editor_config_layout_host_api(app);
        plugin->apply_selection(&host_api, index);
    }
}

static void flippass_editor_add_config_items(App* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Lock Time",
        COUNT_OF(flippass_editor_idle_lock_minutes),
        flippass_editor_config_lock_change_callback,
        app);
    variable_item_set_current_value_index(
        item, flippass_editor_lock_minutes_to_index(app->editor_idle_lock_minutes));
    flippass_editor_config_lock_change_callback(item);

    item = variable_item_list_add(
        app->variable_item_list,
        "Unlock attempts",
        COUNT_OF(flippass_editor_idle_unlock_attempts),
        flippass_editor_config_attempts_change_callback,
        app);
    variable_item_set_current_value_index(
        item, flippass_editor_unlock_attempts_to_index(app->editor_idle_unlock_attempts));
    flippass_editor_config_attempts_change_callback(item);

    item = variable_item_list_add(
        app->variable_item_list,
        "Exit Time",
        COUNT_OF(flippass_editor_idle_exit_minutes),
        flippass_editor_config_exit_change_callback,
        app);
    variable_item_set_current_value_index(
        item, flippass_editor_exit_minutes_to_index(app->editor_idle_exit_minutes));
    flippass_editor_config_exit_change_callback(item);

    item = variable_item_list_add(
        app->variable_item_list,
        "TimeZone TOTP",
        FLIPPASS_OTP_TIME_ZONE_COUNT,
        flippass_editor_otp_time_zone_change_callback,
        app);
    variable_item_set_current_value_index(
        item, flippass_editor_otp_time_zone_to_index(app->editor_otp_time_zone_minutes));
    flippass_editor_otp_time_zone_change_callback(item);

    item = variable_item_list_add(
        app->variable_item_list,
        "Allow /ext",
        2U,
        flippass_editor_config_allow_ext_change_callback,
        app);
    variable_item_set_current_value_index(item, app->editor_always_allow_ext ? 1U : 0U);
    flippass_editor_config_allow_ext_change_callback(item);

    FuriString* error = furi_string_alloc();
    app->editor_keyboard_layout_available = false;
    const FlipPassKeyboardLayoutPluginV1* plugin =
        flippass_editor_config_layout_plugin_ensure(app, error);
    const FlipPassKeyboardLayoutHostApiV1 host_api = flippass_editor_config_layout_host_api(app);
    if(plugin != NULL && plugin->load_items(&host_api) && plugin->item_count() > 0U) {
        const uint32_t count = plugin->item_count();
        uint32_t selected_index = plugin->selected_index(&host_api);
        if(selected_index >= count) {
            selected_index = 0U;
        }
        app->editor_keyboard_layout_available = true;
        app->editor_keyboard_layout_index = selected_index;
        item = variable_item_list_add(
            app->variable_item_list,
            "Keyboard Layout",
            count,
            flippass_editor_config_layout_change_callback,
            app);
        variable_item_set_current_value_index(item, (uint8_t)selected_index);
        flippass_editor_config_layout_change_callback(item);
    } else {
        flippass_editor_add_text_item(app, "Keyboard Layout", "Unavailable", false);
    }
    furi_string_free(error);
}

static bool flippass_editor_entry_otp_settled(const App* app) {
    if(app->editor_entry != NULL) {
        return flippass_otp_entry_has_any_config(app->editor_entry);
    }
    return flippass_otp_drafts_have_any_config(app->editor_custom_fields);
}

static void flippass_editor_add_other_fields_item(App* app) {
    char preview[24];
    const size_t count = flippass_editor_custom_field_count(app);

    if(count == 0U) {
        snprintf(preview, sizeof(preview), "%s", "Add");
    } else if(count == 1U) {
        snprintf(preview, sizeof(preview), "%s", "1 Field");
    } else {
        snprintf(preview, sizeof(preview), "%lu Fields", (unsigned long)count);
    }

    flippass_editor_add_text_item(app, "Other Fields", preview, false);
}

static void flippass_editor_add_otp_status_item(App* app) {
    flippass_editor_add_text_item(
        app, "OTP", flippass_editor_entry_otp_settled(app) ? "Settled" : "Set", false);
}

static void flippass_editor_add_delete_item(App* app) {
    flippass_editor_add_text_item(app, "Delete", " ", false);
}

static void flippass_editor_build_form(App* app) {
    variable_item_list_reset(app->variable_item_list);

    switch(app->editor_mode) {
    case FlipPassEditorModeNewDatabase:
        flippass_editor_add_text_item(app, "Name", app->editor_file_name, false);
        flippass_editor_add_text_item(app, "Password", app->editor_database_password, true);
        flippass_editor_add_cipher_item(app);
        flippass_editor_add_compression_item(app);
        flippass_editor_add_kdf_item(app);
        flippass_editor_add_commit_item(app);
        break;
    case FlipPassEditorModeModifyDatabase:
        if(flippass_editor_modify_database_uses_password(app)) {
            flippass_editor_add_text_item(app, "Password", app->editor_database_password, true);
        }
        flippass_editor_add_cipher_item(app);
        flippass_editor_add_compression_item(app);
        flippass_editor_add_kdf_item(app);
        flippass_editor_add_commit_item(app);
        break;
    case FlipPassEditorModeNewDirectory:
    case FlipPassEditorModeAddGroup:
    case FlipPassEditorModeEditGroup:
        flippass_editor_add_text_item(app, "Name", app->editor_group_name, false);
        flippass_editor_add_commit_item(app);
        if(app->editor_mode == FlipPassEditorModeEditGroup) {
            flippass_editor_add_delete_item(app);
        }
        break;
    case FlipPassEditorModeAddEntry:
    case FlipPassEditorModeEditEntry:
        flippass_editor_add_text_item(app, "Title", app->editor_entry_title, false);
        flippass_editor_add_text_item(app, "Username", app->editor_entry_username, false);
        flippass_editor_add_text_item(app, "Password", app->editor_entry_password, true);
        flippass_editor_add_text_item(app, "URL", app->editor_entry_url, false);
        flippass_editor_add_text_item(app, "Notes", app->editor_entry_notes, false);
        flippass_editor_add_text_item(app, "AutoType", app->editor_entry_autotype, false);
        flippass_editor_add_other_fields_item(app);
        flippass_editor_add_otp_status_item(app);
        flippass_editor_add_commit_item(app);
        if(app->editor_mode == FlipPassEditorModeEditEntry) {
            flippass_editor_add_delete_item(app);
        }
        break;
    case FlipPassEditorModeEditOtp:
        flippass_editor_add_otp_type_item(app);
        flippass_editor_add_text_item(app, "Secret", app->editor_otp_secret, true);
        flippass_editor_add_otp_encoding_item(app);
        if(app->editor_otp_kind == FlipPassOtpKindHmac) {
            flippass_editor_add_text_item(app, "Counter", app->editor_otp_counter, false);
        } else {
            flippass_editor_add_otp_digits_item(app);
            flippass_editor_add_otp_period_item(app);
            flippass_editor_add_otp_algorithm_item(app);
            flippass_editor_add_otp_time_zone_item(app);
        }
        flippass_editor_add_commit_item(app);
        if(app->editor_otp_settled) {
            flippass_editor_add_delete_item(app);
        }
        break;
    case FlipPassEditorModeRenameFile:
        flippass_editor_add_text_item(app, "File", app->editor_file_name, false);
        flippass_editor_add_commit_item(app);
        break;
    case FlipPassEditorModeGlobalConfig:
        flippass_editor_add_config_items(app);
        flippass_editor_add_commit_item(app);
        break;
    case FlipPassEditorModeAddCustomField:
    case FlipPassEditorModeEditCustomField:
        flippass_editor_add_text_item(app, "Name", app->editor_custom_field_name, false);
        flippass_editor_add_custom_field_protected_item(app);
        flippass_editor_add_text_item(app, "Value", app->editor_custom_field_value, true);
        flippass_editor_add_commit_item(app);
        if(app->editor_mode == FlipPassEditorModeEditCustomField) {
            flippass_editor_add_delete_item(app);
        }
        break;
    case FlipPassEditorModeNone:
    default:
        break;
    }

    variable_item_list_set_enter_callback(
        app->variable_item_list, flippass_editor_enter_callback, app);
    if(app->editor_mode != FlipPassEditorModeNone &&
       app->editor_selected_index > flippass_editor_last_index(app)) {
        app->editor_selected_index = flippass_editor_last_index(app);
    }
    variable_item_list_set_selected_item(
        app->variable_item_list, (uint8_t)app->editor_selected_index);
}

static bool flippass_editor_validate_file_component(
    const char* text,
    FuriString* error,
    const char* required_message) {
    if(text == NULL || text[0] == '\0') {
        furi_string_set_str(error, required_message);
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(strchr("<>:\"/\\|?*", *cursor) != NULL) {
            furi_string_set_str(error, "Use a valid file name.");
            return false;
        }
    }

    return true;
}

static bool
    flippass_editor_file_name_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    return flippass_editor_validate_file_component(text, error, "File name is required.");
}

static bool
    flippass_editor_directory_name_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    return flippass_editor_validate_file_component(text, error, "Directory name is required.");
}

static bool flippass_editor_database_password_validator(
    const char* text,
    FuriString* error,
    void* context) {
    App* app = context;

    if(text == NULL || text[0] == '\0') {
        if(app != NULL && flippass_editor_modify_database_uses_password(app)) {
            return true;
        }
        furi_string_set_str(error, "Password is required to save.");
        return false;
    }

    return true;
}

static void flippass_editor_restore_parent_mode(App* app) {
    if(app->editor_parent_mode == FlipPassEditorModeAddEntry ||
       app->editor_parent_mode == FlipPassEditorModeEditEntry) {
        app->editor_mode = app->editor_parent_mode;
    }
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_custom_field = NULL;
    app->editor_custom_field_draft = NULL;
    app->editor_custom_field_protected = false;
    app->editor_custom_field_name[0] = '\0';
    app->editor_custom_field_value[0] = '\0';
    app->password_gen_auto_open_field_name = false;
    app->editor_selected_index = FlipPassEditorEntryRowOtherFields;
}

void flippass_editor_prepare_new_custom_field(App* app) {
    furi_assert(app);

    app->editor_parent_mode = (app->editor_mode == FlipPassEditorModeAddEntry ||
                               app->editor_mode == FlipPassEditorModeEditEntry) ?
                                  app->editor_mode :
                                  app->editor_parent_mode;
    app->editor_mode = FlipPassEditorModeAddCustomField;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_custom_field = NULL;
    app->editor_custom_field_draft = NULL;
    app->editor_custom_field_protected = false;
    app->editor_custom_field_name[0] = '\0';
    app->editor_custom_field_value[0] = '\0';
    app->editor_selected_index = 0U;
    app->password_gen_auto_open_field_name = true;
    app->editor_return_scene = FlipPassScene_OtherFields;
}

void flippass_editor_prepare_edit_custom_field(
    App* app,
    KDBXCustomField* field,
    FlipPassEditorCustomFieldDraft* draft) {
    furi_assert(app);

    app->editor_parent_mode = (app->editor_mode == FlipPassEditorModeAddEntry ||
                               app->editor_mode == FlipPassEditorModeEditEntry) ?
                                  app->editor_mode :
                                  app->editor_parent_mode;
    app->editor_mode = FlipPassEditorModeEditCustomField;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_custom_field = field;
    app->editor_custom_field_draft = draft;
    app->editor_custom_field_protected = draft != NULL ? draft->protected_value :
                                                         (field != NULL && field->protected_value);
    snprintf(
        app->editor_custom_field_name,
        sizeof(app->editor_custom_field_name),
        "%s",
        draft != NULL ? (draft->name != NULL ? draft->name : "") :
                        (field != NULL && field->key != NULL ? field->key : ""));
    snprintf(
        app->editor_custom_field_value,
        sizeof(app->editor_custom_field_value),
        "%s",
        draft != NULL ? (draft->value != NULL ? draft->value : "") :
                        (field != NULL && field->value != NULL ? field->value : ""));
    app->editor_selected_index = 3U;
    app->editor_return_scene = FlipPassScene_OtherFields;
}

static const char* flippass_editor_otp_all_field_names[] = {
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

static char* flippass_editor_alloc_string(const char* value) {
    const char* source = value != NULL ? value : "";
    const size_t size = strlen(source) + 1U;
    char* copy = malloc(size);
    if(copy != NULL) {
        memcpy(copy, source, size);
    }
    return copy;
}

static bool flippass_editor_parse_u64_text(const char* text, uint64_t* out_value) {
    uint64_t value = 0ULL;

    if(text == NULL || text[0] == '\0' || out_value == NULL) {
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(*cursor < '0' || *cursor > '9') {
            return false;
        }
        const uint64_t digit = (uint64_t)(*cursor - '0');
        if(value > (UINT64_MAX - digit) / 10ULL) {
            return false;
        }
        value = (value * 10ULL) + digit;
    }

    *out_value = value;
    return true;
}

static bool
    flippass_editor_parse_algorithm_text(const char* text, FlipPassOtpAlgorithm* out_algorithm) {
    if(text == NULL || text[0] == '\0') {
        return false;
    }

    if(strcmp(text, "HMAC-SHA-1") == 0) {
        *out_algorithm = FlipPassOtpAlgorithmSha1;
        return true;
    }
    if(strcmp(text, "HMAC-SHA-256") == 0) {
        *out_algorithm = FlipPassOtpAlgorithmSha256;
        return true;
    }
    if(strcmp(text, "HMAC-SHA-512") == 0) {
        *out_algorithm = FlipPassOtpAlgorithmSha512;
        return true;
    }
    return false;
}

static KDBXCustomField* flippass_editor_find_custom_field(KDBXEntry* entry, const char* name) {
    for(KDBXCustomField* field = entry != NULL ? entry->custom_fields : NULL; field != NULL;
        field = field->next) {
        if(field->key != NULL && strcmp(field->key, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static FlipPassEditorCustomFieldDraft*
    flippass_editor_find_custom_field_draft(App* app, const char* name) {
    for(FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields; draft != NULL;
        draft = draft->next) {
        if(draft->name != NULL && strcmp(draft->name, name) == 0) {
            return draft;
        }
    }
    return NULL;
}

static void flippass_editor_set_otp_defaults(App* app) {
    app->editor_otp_kind = FlipPassOtpKindTime;
    app->editor_otp_secret_encoding = FlipPassOtpSecretEncodingBase32;
    app->editor_otp_algorithm = FlipPassOtpAlgorithmSha1;
    app->editor_otp_digits = FLIPPASS_OTP_DEFAULT_DIGITS;
    app->editor_otp_period = FLIPPASS_OTP_DEFAULT_PERIOD;
    app->editor_otp_time_zone_minutes = app->otp_time_zone_minutes;
    app->editor_otp_settled = flippass_editor_entry_otp_settled(app);
    app->editor_otp_secret[0] = '\0';
    snprintf(
        app->editor_otp_counter,
        sizeof(app->editor_otp_counter),
        "%llu",
        (unsigned long long)FLIPPASS_OTP_DEFAULT_COUNTER);
}

static void flippass_editor_load_otp_draft_secret(App* app, FlipPassOtpKind kind) {
    static const FlipPassOtpSecretEncoding encodings[] = {
        FlipPassOtpSecretEncodingText,
        FlipPassOtpSecretEncodingHex,
        FlipPassOtpSecretEncodingBase32,
        FlipPassOtpSecretEncodingBase64,
    };

    for(size_t index = 0U; index < COUNT_OF(encodings); index++) {
        const char* name = flippass_otp_secret_field_name(kind, encodings[index]);
        FlipPassEditorCustomFieldDraft* draft = flippass_editor_find_custom_field_draft(app, name);
        if(draft != NULL) {
            app->editor_otp_kind = kind;
            app->editor_otp_secret_encoding = encodings[index];
            snprintf(
                app->editor_otp_secret,
                sizeof(app->editor_otp_secret),
                "%s",
                draft->value != NULL ? draft->value : "");
            return;
        }
    }
}

static void flippass_editor_load_otp_from_drafts(App* app) {
    flippass_editor_load_otp_draft_secret(app, FlipPassOtpKindTime);
    flippass_editor_load_otp_draft_secret(app, FlipPassOtpKindHmac);

    FlipPassEditorCustomFieldDraft* counter =
        flippass_editor_find_custom_field_draft(app, "HmacOtp-Counter");
    if(counter != NULL && counter->value != NULL) {
        snprintf(app->editor_otp_counter, sizeof(app->editor_otp_counter), "%s", counter->value);
    }

    FlipPassEditorCustomFieldDraft* length =
        flippass_editor_find_custom_field_draft(app, "TimeOtp-Length");
    if(length != NULL && length->value != NULL) {
        uint64_t digits = 0ULL;
        if(flippass_editor_parse_u64_text(length->value, &digits) && digits > 0U &&
           digits <= FLIPPASS_OTP_MAX_DIGITS) {
            app->editor_otp_digits = (uint8_t)digits;
        }
    }

    FlipPassEditorCustomFieldDraft* period =
        flippass_editor_find_custom_field_draft(app, "TimeOtp-Period");
    if(period != NULL && period->value != NULL) {
        uint64_t period_value = 0ULL;
        if(flippass_editor_parse_u64_text(period->value, &period_value) && period_value > 0U &&
           period_value <= 255U) {
            app->editor_otp_period = (uint32_t)period_value;
        }
    }

    FlipPassEditorCustomFieldDraft* algorithm =
        flippass_editor_find_custom_field_draft(app, "TimeOtp-Algorithm");
    if(algorithm != NULL && algorithm->value != NULL) {
        FlipPassOtpAlgorithm parsed = FlipPassOtpAlgorithmSha1;
        if(flippass_editor_parse_algorithm_text(algorithm->value, &parsed)) {
            app->editor_otp_algorithm = parsed;
        }
    }
}

static bool
    flippass_editor_load_otp_field_text(App* app, KDBXCustomField* field, FuriString* error) {
    return field == NULL || flippass_db_ensure_custom_field(app, app->editor_entry, field, error);
}

static bool
    flippass_editor_load_otp_entry_secret(App* app, FlipPassOtpKind kind, FuriString* error) {
    static const FlipPassOtpSecretEncoding encodings[] = {
        FlipPassOtpSecretEncodingText,
        FlipPassOtpSecretEncodingHex,
        FlipPassOtpSecretEncodingBase32,
        FlipPassOtpSecretEncodingBase64,
    };

    for(size_t index = 0U; index < COUNT_OF(encodings); index++) {
        const char* name = flippass_otp_secret_field_name(kind, encodings[index]);
        KDBXCustomField* field = flippass_editor_find_custom_field(app->editor_entry, name);
        if(field == NULL) {
            continue;
        }
        if(!flippass_editor_load_otp_field_text(app, field, error)) {
            return false;
        }
        app->editor_otp_kind = kind;
        app->editor_otp_secret_encoding = encodings[index];
        snprintf(
            app->editor_otp_secret,
            sizeof(app->editor_otp_secret),
            "%s",
            field->value != NULL ? field->value : "");
        return true;
    }

    return true;
}

static bool flippass_editor_load_otp_from_entry(App* app, FuriString* error) {
    if(app->editor_entry == NULL) {
        return true;
    }

    if(!flippass_editor_load_otp_entry_secret(app, FlipPassOtpKindTime, error) ||
       !flippass_editor_load_otp_entry_secret(app, FlipPassOtpKindHmac, error)) {
        return false;
    }

    KDBXCustomField* counter =
        flippass_editor_find_custom_field(app->editor_entry, "HmacOtp-Counter");
    if(counter != NULL) {
        if(!flippass_editor_load_otp_field_text(app, counter, error)) {
            return false;
        }
        snprintf(
            app->editor_otp_counter,
            sizeof(app->editor_otp_counter),
            "%s",
            counter->value != NULL ? counter->value : "0");
    }

    KDBXCustomField* length =
        flippass_editor_find_custom_field(app->editor_entry, "TimeOtp-Length");
    if(length != NULL) {
        uint64_t digits = 0ULL;
        if(!flippass_editor_load_otp_field_text(app, length, error)) {
            return false;
        }
        if(length->value != NULL && flippass_editor_parse_u64_text(length->value, &digits) &&
           digits > 0U && digits <= FLIPPASS_OTP_MAX_DIGITS) {
            app->editor_otp_digits = (uint8_t)digits;
        }
    }

    KDBXCustomField* period =
        flippass_editor_find_custom_field(app->editor_entry, "TimeOtp-Period");
    if(period != NULL) {
        uint64_t period_value = 0ULL;
        if(!flippass_editor_load_otp_field_text(app, period, error)) {
            return false;
        }
        if(period->value != NULL && flippass_editor_parse_u64_text(period->value, &period_value) &&
           period_value > 0U && period_value <= 255U) {
            app->editor_otp_period = (uint32_t)period_value;
        }
    }

    KDBXCustomField* algorithm =
        flippass_editor_find_custom_field(app->editor_entry, "TimeOtp-Algorithm");
    if(algorithm != NULL) {
        FlipPassOtpAlgorithm parsed = FlipPassOtpAlgorithmSha1;
        if(!flippass_editor_load_otp_field_text(app, algorithm, error)) {
            return false;
        }
        if(algorithm->value != NULL &&
           flippass_editor_parse_algorithm_text(algorithm->value, &parsed)) {
            app->editor_otp_algorithm = parsed;
        }
    }

    return true;
}

static void flippass_editor_prepare_otp_form(App* app) {
    FuriString* error = furi_string_alloc();

    app->editor_parent_mode = (app->editor_mode == FlipPassEditorModeAddEntry ||
                               app->editor_mode == FlipPassEditorModeEditEntry) ?
                                  app->editor_mode :
                                  app->editor_parent_mode;
    app->editor_mode = FlipPassEditorModeEditOtp;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    flippass_editor_set_otp_defaults(app);

    bool ok = true;
    if(app->editor_entry == NULL) {
        flippass_editor_load_otp_from_drafts(app);
    } else {
        ok = flippass_editor_load_otp_from_entry(app, error);
    }

    if(!ok) {
        app->editor_mode = app->editor_parent_mode;
        app->editor_parent_mode = FlipPassEditorModeNone;
        flippass_scene_status_show(
            app, "OTP Load Failed", furi_string_get_cstr(error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }

    app->editor_selected_index = app->editor_otp_settled ? FlipPassEditorOtpRowType :
                                                           FlipPassEditorOtpRowSecret;
    app->editor_return_scene = FlipPassScene_Editor;
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
    furi_string_free(error);
}

static bool flippass_editor_delete_otp_draft_by_name(App* app, const char* name) {
    bool deleted = false;
    FlipPassEditorCustomFieldDraft** link = &app->editor_custom_fields;
    while(*link != NULL) {
        FlipPassEditorCustomFieldDraft* draft = *link;
        if(draft->name != NULL && strcmp(draft->name, name) == 0) {
            *link = draft->next;
            flippass_editor_free_custom_field_draft(draft);
            deleted = true;
            continue;
        }
        link = &(*link)->next;
    }
    return deleted;
}

static bool flippass_editor_upsert_otp_draft(
    App* app,
    const char* name,
    const char* value,
    bool protected_value,
    FuriString* error) {
    FlipPassEditorCustomFieldDraft* draft = flippass_editor_find_custom_field_draft(app, name);
    if(draft == NULL) {
        draft = malloc(sizeof(FlipPassEditorCustomFieldDraft));
        if(draft == NULL) {
            furi_string_set_str(error, "Not enough RAM to update OTP fields.");
            return false;
        }
        memset(draft, 0, sizeof(*draft));
        draft->next = app->editor_custom_fields;
        app->editor_custom_fields = draft;
    }

    char* name_copy = flippass_editor_alloc_string(name);
    char* value_copy = flippass_editor_alloc_string(value);
    if(name_copy == NULL || value_copy == NULL) {
        free(name_copy);
        free(value_copy);
        furi_string_set_str(error, "Not enough RAM to update OTP fields.");
        return false;
    }

    if(draft->name != NULL) {
        memzero(draft->name, strlen(draft->name));
        free(draft->name);
    }
    if(draft->value != NULL) {
        memzero(draft->value, strlen(draft->value));
        free(draft->value);
    }
    draft->name = name_copy;
    draft->value = value_copy;
    draft->protected_value = protected_value;
    return true;
}

static bool
    flippass_editor_delete_otp_entry_field_by_name(App* app, const char* name, FuriString* error) {
    bool ok = true;

    while(ok) {
        KDBXCustomField* field = flippass_editor_find_custom_field(app->editor_entry, name);
        if(field == NULL) {
            break;
        }
        ok = flippass_db_delete_custom_field(app, app->editor_entry, field, error);
    }

    return ok;
}

static bool flippass_editor_upsert_otp_entry_field(
    App* app,
    const char* name,
    const char* value,
    bool protected_value,
    FuriString* error) {
    KDBXCustomField* field = flippass_editor_find_custom_field(app->editor_entry, name);
    if(field != NULL) {
        return flippass_db_update_custom_field(
            app, app->editor_entry, field, name, value, protected_value, error);
    }
    return flippass_db_create_custom_field(
        app, app->editor_entry, name, value, protected_value, NULL, error);
}

static bool flippass_editor_delete_all_otp_fields(App* app, FuriString* error) {
    bool ok = true;

    for(size_t index = 0U; ok && index < COUNT_OF(flippass_editor_otp_all_field_names); index++) {
        const char* name = flippass_editor_otp_all_field_names[index];
        if(app->editor_entry == NULL) {
            flippass_editor_delete_otp_draft_by_name(app, name);
        } else {
            ok = flippass_editor_delete_otp_entry_field_by_name(app, name, error);
        }
    }

    return ok;
}

static bool flippass_editor_save_otp_field(
    App* app,
    const char* name,
    const char* value,
    bool protected_value,
    FuriString* error) {
    if(app->editor_entry == NULL) {
        return flippass_editor_upsert_otp_draft(app, name, value, protected_value, error);
    }

    return flippass_editor_upsert_otp_entry_field(app, name, value, protected_value, error);
}

static void flippass_editor_restore_after_otp(App* app) {
    if(app->editor_parent_mode == FlipPassEditorModeAddEntry ||
       app->editor_parent_mode == FlipPassEditorModeEditEntry) {
        app->editor_mode = app->editor_parent_mode;
    }
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_selected_index = FlipPassEditorEntryRowOtp;
}

static bool flippass_editor_execute_otp_delete(App* app) {
    FuriString* error = furi_string_alloc();
    const bool ok = flippass_editor_delete_all_otp_fields(app, error);

    if(ok) {
        flippass_editor_restore_after_otp(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_Editor);
    } else {
        flippass_scene_status_show(
            app, "Delete Failed", furi_string_get_cstr(error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
    return ok;
}

static bool flippass_editor_execute_otp_save(App* app) {
    FuriString* error = furi_string_alloc();
    bool ok = false;
    char text[FLIPPASS_OTP_COUNTER_TEXT_SIZE];

    if(app->editor_otp_secret[0] == '\0') {
        furi_string_set_str(error, "OTP secret is required.");
        goto finish;
    }

    uint64_t counter = 0ULL;
    if(app->editor_otp_kind == FlipPassOtpKindHmac &&
       !flippass_editor_parse_u64_text(app->editor_otp_counter, &counter)) {
        furi_string_set_str(error, "HmacOtp-Counter must be a decimal number.");
        goto finish;
    }

    ok = flippass_editor_delete_all_otp_fields(app, error);
    if(!ok) {
        goto finish;
    }

    const char* secret_field =
        flippass_otp_secret_field_name(app->editor_otp_kind, app->editor_otp_secret_encoding);
    ok = flippass_editor_save_otp_field(app, secret_field, app->editor_otp_secret, true, error);
    if(!ok) {
        goto finish;
    }

    if(app->editor_otp_kind == FlipPassOtpKindHmac) {
        if(counter != FLIPPASS_OTP_DEFAULT_COUNTER) {
            snprintf(text, sizeof(text), "%llu", (unsigned long long)counter);
            ok = flippass_editor_save_otp_field(app, "HmacOtp-Counter", text, false, error);
        }
    } else {
        if(app->editor_otp_digits != FLIPPASS_OTP_DEFAULT_DIGITS) {
            snprintf(text, sizeof(text), "%u", (unsigned int)app->editor_otp_digits);
            ok = flippass_editor_save_otp_field(app, "TimeOtp-Length", text, false, error);
        }
        if(ok && app->editor_otp_period != FLIPPASS_OTP_DEFAULT_PERIOD) {
            snprintf(text, sizeof(text), "%lu", (unsigned long)app->editor_otp_period);
            ok = flippass_editor_save_otp_field(app, "TimeOtp-Period", text, false, error);
        }
        if(ok && app->editor_otp_algorithm != FlipPassOtpAlgorithmSha1) {
            ok = flippass_editor_save_otp_field(
                app,
                "TimeOtp-Algorithm",
                flippass_otp_algorithm_field_value(app->editor_otp_algorithm),
                false,
                error);
        }
    }

    if(ok) {
        app->otp_time_zone_minutes = app->editor_otp_time_zone_minutes;
        flippass_save_settings(app);
        flippass_editor_restore_after_otp(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_Editor);
    }

finish:
    if(!ok && !furi_string_empty(error)) {
        flippass_scene_status_show(
            app, "OTP Save Failed", furi_string_get_cstr(error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    memzero(text, sizeof(text));
    furi_string_free(error);
    return ok;
}

static bool flippass_editor_save_database(
    App* app,
    const char* target_path,
    const char* password,
    FuriString* error) {
    return flippass_save_current_database(app, target_path, password, error);
}

static bool flippass_editor_execute_new_database_commit(App* app) {
    FuriString* error = furi_string_alloc();
    FuriString* file_name = furi_string_alloc();
    FuriString* root_name = furi_string_alloc();
    FuriString* target_path = furi_string_alloc();
    Storage* storage = NULL;
    bool ok = false;

    furi_assert(app);

    const FlipPassKdbxCipher requested_cipher = app->database_cipher;
    const uint32_t requested_compression = app->database_compression;
    const uint64_t requested_kdf_rounds = app->database_kdf_rounds;

    storage = furi_record_open(RECORD_STORAGE);
    flippass_editor_compose_file_name(file_name, app->editor_file_name);
    if(furi_string_empty(file_name)) {
        furi_string_set_str(error, "A target file name is required.");
        goto cleanup;
    }

    path_concat(
        furi_string_get_cstr(app->browser_directory),
        furi_string_get_cstr(file_name),
        target_path);
    if(storage_file_exists(storage, furi_string_get_cstr(target_path))) {
        furi_string_set_str(error, "A database with that name already exists.");
        goto cleanup;
    }

    flippass_editor_compose_root_name(root_name, app->editor_file_name);
    ok = flippass_db_create_new_database(
        app, furi_string_get_cstr(root_name), requested_cipher, requested_compression, error);
    if(ok) {
        app->database_cipher = requested_cipher;
        app->database_compression = requested_compression;
        app->database_kdf_rounds = requested_kdf_rounds;
        ok = flippass_editor_save_database(
            app, furi_string_get_cstr(target_path), app->editor_database_password, error);
    }

    if(ok) {
        flippass_editor_clear_context(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_FileBrowser);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_DbEntries);
    }

cleanup:
    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
    if(!ok && !furi_string_empty(error)) {
        flippass_scene_status_show(
            app, "Save Failed", furi_string_get_cstr(error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
    furi_string_free(file_name);
    furi_string_free(root_name);
    furi_string_free(target_path);
    return ok;
}

static bool flippass_editor_execute_modify_database_commit(App* app) {
    FuriString* error = furi_string_alloc();
    bool ok = false;

    furi_assert(app);

    ok = flippass_editor_save_database(
        app,
        furi_string_get_cstr(app->file_path),
        flippass_editor_modify_database_uses_password(app) ? app->editor_database_password : NULL,
        error);
    if(ok) {
        const bool close_after_commit = app->editor_close_after_commit;
        flippass_editor_clear_context(app);
        if(close_after_commit) {
            flippass_close_database(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_FileBrowser);
        } else {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_DbEntries);
        }
    } else if(!furi_string_empty(error)) {
        flippass_scene_status_show(
            app, "Save Failed", furi_string_get_cstr(error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
    return ok;
}

static bool flippass_editor_execute_global_config_commit(App* app) {
    FuriString* error = furi_string_alloc();
    bool ok = false;

    furi_assert(app);

    if(app->editor_idle_unlock_attempts == 0U) {
        app->editor_idle_unlock_attempts = FLIPPASS_DEFAULT_IDLE_UNLOCK_ATTEMPTS;
    }

    if(app->editor_idle_lock_minutes > 0U && app->editor_idle_exit_minutes > 0U &&
       app->editor_idle_exit_minutes <= app->editor_idle_lock_minutes) {
        furi_string_set_str(error, "Exit Time must be greater than Lock Time.");
        goto finish;
    }

    app->idle_lock_minutes = app->editor_idle_lock_minutes;
    app->idle_unlock_attempts = app->editor_idle_unlock_attempts;
    app->idle_exit_minutes = app->editor_idle_exit_minutes;
    app->otp_time_zone_minutes = app->editor_otp_time_zone_minutes;
    app->always_allow_ext = app->editor_always_allow_ext;

    if(app->editor_keyboard_layout_available) {
        if(app->editor_keyboard_layout_use_alt || app->editor_keyboard_layout_path[0] == '\0') {
            furi_string_reset(app->keyboard_layout_path);
        } else {
            furi_string_set_str(app->keyboard_layout_path, app->editor_keyboard_layout_path);
        }
        app->keyboard_layout_configured = true;
    }
    flippass_save_settings(app);
    ok = true;

finish:
    if(ok) {
        flippass_editor_clear_context(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_FileBrowser);
    } else if(!furi_string_empty(error)) {
        flippass_scene_status_show(
            app, "Config Failed", furi_string_get_cstr(error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
    return ok;
}

static const FlipPassEditorCrudPluginV1*
    flippass_editor_crud_plugin_load(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotEditorCrud,
        NULL,
        FLIPPASS_EDITOR_CRUD_PLUGIN_APP_ID,
        FLIPPASS_EDITOR_CRUD_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        return NULL;
    }

    const FlipPassEditorCrudPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_EDITOR_CRUD_PLUGIN_API_VERSION ||
       plugin->execute_commit == NULL || plugin->execute_delete == NULL) {
        furi_string_set_str(error, "FlipPass editor CRUD plugin has an incompatible API.");
        return NULL;
    }

    return plugin;
}

static bool flippass_editor_crud_host_create_group(
    void* context,
    KDBXGroup* parent,
    const char* name,
    KDBXGroup** out_group,
    FuriString* error) {
    return flippass_db_create_group(context, parent, name, out_group, error);
}

static bool flippass_editor_crud_host_update_group(
    void* context,
    KDBXGroup* group,
    const char* name,
    FuriString* error) {
    return flippass_db_update_group(context, group, name, error);
}

static bool
    flippass_editor_crud_host_delete_group(void* context, KDBXGroup* group, FuriString* error) {
    return flippass_db_delete_group(context, group, error);
}

static bool flippass_editor_crud_host_create_entry(
    void* context,
    KDBXGroup* group,
    const char* title,
    const char* username,
    const char* password,
    const char* url,
    const char* notes,
    const char* autotype,
    KDBXEntry** out_entry,
    FuriString* error) {
    return flippass_db_create_entry(
        context, group, title, username, password, url, notes, autotype, out_entry, error);
}

static bool flippass_editor_crud_host_update_entry(
    void* context,
    KDBXEntry* entry,
    const char* title,
    const char* username,
    const char* password,
    const char* url,
    const char* notes,
    const char* autotype,
    FuriString* error) {
    return flippass_db_update_entry(
        context, entry, title, username, password, url, notes, autotype, error);
}

static bool
    flippass_editor_crud_host_delete_entry(void* context, KDBXEntry* entry, FuriString* error) {
    return flippass_db_delete_entry(context, entry, error);
}

static bool flippass_editor_crud_host_create_custom_field(
    void* context,
    KDBXEntry* entry,
    const char* name,
    const char* value,
    bool protected_value,
    KDBXCustomField** out_field,
    FuriString* error) {
    return flippass_db_create_custom_field(
        context, entry, name, value, protected_value, out_field, error);
}

static bool flippass_editor_crud_host_update_custom_field(
    void* context,
    KDBXEntry* entry,
    KDBXCustomField* field,
    const char* name,
    const char* value,
    bool protected_value,
    FuriString* error) {
    return flippass_db_update_custom_field(
        context, entry, field, name, value, protected_value, error);
}

static bool flippass_editor_crud_host_delete_custom_field(
    void* context,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error) {
    return flippass_db_delete_custom_field(context, entry, field, error);
}

static void flippass_editor_crud_host_save_settings(void* context) {
    flippass_save_settings(context);
}

static void flippass_editor_crud_host_show_status(
    void* context,
    const char* title,
    const char* message,
    uint32_t return_scene) {
    flippass_scene_status_show(context, title, message, return_scene);
}

static const FlipPassEditorCrudHostApiV1 flippass_editor_crud_host_api = {
    .api_version = FLIPPASS_EDITOR_CRUD_HOST_API_VERSION,
    .context = NULL,
    .create_group = flippass_editor_crud_host_create_group,
    .update_group = flippass_editor_crud_host_update_group,
    .delete_group = flippass_editor_crud_host_delete_group,
    .create_entry = flippass_editor_crud_host_create_entry,
    .update_entry = flippass_editor_crud_host_update_entry,
    .delete_entry = flippass_editor_crud_host_delete_entry,
    .create_custom_field = flippass_editor_crud_host_create_custom_field,
    .update_custom_field = flippass_editor_crud_host_update_custom_field,
    .delete_custom_field = flippass_editor_crud_host_delete_custom_field,
    .save_settings = flippass_editor_crud_host_save_settings,
    .show_status = flippass_editor_crud_host_show_status,
};

static FlipPassEditorCrudHostApiV1 flippass_editor_make_crud_host_api(App* app) {
    FlipPassEditorCrudHostApiV1 host_api = flippass_editor_crud_host_api;
    host_api.context = app;
    return host_api;
}

static bool flippass_editor_execute_commit(App* app) {
    const FlipPassEditorCrudPluginV1* plugin = NULL;
    bool ok = false;

    switch(app->editor_mode) {
    case FlipPassEditorModeNewDatabase:
        return flippass_editor_execute_new_database_commit(app);
    case FlipPassEditorModeModifyDatabase:
        return flippass_editor_execute_modify_database_commit(app);
    case FlipPassEditorModeGlobalConfig:
        return flippass_editor_execute_global_config_commit(app);
    default:
        break;
    }

    FuriString* load_error = furi_string_alloc();
    plugin = flippass_editor_crud_plugin_load(app, load_error);
    if(plugin != NULL) {
        FlipPassEditorCrudHostApiV1 host_api = flippass_editor_make_crud_host_api(app);
        ok = plugin->execute_commit(app, &host_api);
    } else {
        flippass_scene_status_show(
            app, "Edit Failed", furi_string_get_cstr(load_error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    flippass_module_unload(app, FlipPassModuleSlotEditorCrud);
    furi_string_free(load_error);
    return ok;
}

static void flippass_editor_open_text_target(App* app, FlipPassEditorTextTarget target) {
    app->editor_text_target = target;
    scene_manager_next_scene(app->scene_manager, FlipPassScene_EditorTextInput);
}

static void flippass_editor_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0x100U + result);
}

static void flippass_editor_show_delete_dialog(
    App* app,
    FlipPassEditorDialogState state,
    const char* header,
    const char* text) {
    scene_manager_set_scene_state(app->scene_manager, FlipPassScene_Editor, state);
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, header, 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        text != NULL ? text : "This action cannot be undone.",
        64,
        20,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");
    dialog_ex_set_right_button_text(app->dialog_ex, "Delete");
    dialog_ex_set_result_callback(app->dialog_ex, flippass_editor_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
}

static void flippass_editor_open_other_fields(App* app) {
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_OtherFields, FlipPassOtherFieldsModeEdit);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_OtherFields);
}

static bool flippass_editor_execute_delete(App* app, FlipPassEditorDialogState state) {
    FuriString* load_error = furi_string_alloc();
    const FlipPassEditorCrudPluginV1* plugin = flippass_editor_crud_plugin_load(app, load_error);
    FlipPassEditorCrudDeleteTarget target = FlipPassEditorCrudDeleteNone;
    bool ok = false;

    switch(state) {
    case FlipPassEditorDialogDeleteGroup:
        target = FlipPassEditorCrudDeleteGroup;
        break;
    case FlipPassEditorDialogDeleteEntry:
        target = FlipPassEditorCrudDeleteEntry;
        break;
    case FlipPassEditorDialogDeleteField:
        target = FlipPassEditorCrudDeleteField;
        break;
    case FlipPassEditorDialogNone:
    default:
        break;
    }

    if(plugin != NULL && target != FlipPassEditorCrudDeleteNone) {
        FlipPassEditorCrudHostApiV1 host_api = flippass_editor_make_crud_host_api(app);
        ok = plugin->execute_delete(app, target, &host_api);
    } else if(plugin == NULL) {
        flippass_scene_status_show(
            app, "Delete Failed", furi_string_get_cstr(load_error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    flippass_module_unload(app, FlipPassModuleSlotEditorCrud);
    furi_string_free(load_error);
    return ok;
}

static void flippass_editor_enter_callback(void* context, uint32_t index) {
    App* app = context;
    app->editor_selected_index = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, index + 1U);
}

static void flippass_editor_handle_enter(App* app, uint32_t index) {
    switch(app->editor_mode) {
    case FlipPassEditorModeNewDatabase:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_commit(app);
        } else if(index == FlipPassEditorItemPrimaryText) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetFileName);
        } else if(index == FlipPassEditorItemSecondaryText) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetDatabasePassword);
        }
        break;
    case FlipPassEditorModeModifyDatabase:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_commit(app);
        } else if(
            flippass_editor_modify_database_uses_password(app) &&
            index == FlipPassEditorItemPrimaryText) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetDatabasePassword);
        }
        break;
    case FlipPassEditorModeNewDirectory:
    case FlipPassEditorModeAddGroup:
    case FlipPassEditorModeEditGroup:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_commit(app);
        } else if(app->editor_mode == FlipPassEditorModeEditGroup && index == 2U) {
            flippass_editor_show_delete_dialog(
                app, FlipPassEditorDialogDeleteGroup, "Delete Group?", app->editor_group_name);
        } else if(index == FlipPassEditorItemPrimaryText) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetGroupName);
        }
        break;
    case FlipPassEditorModeAddEntry:
    case FlipPassEditorModeEditEntry:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_commit(app);
        } else if(index == FlipPassEditorEntryRowTitle) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetEntryTitle);
        } else if(index == FlipPassEditorEntryRowUsername) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetEntryUsername);
        } else if(index == FlipPassEditorEntryRowPassword) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetEntryPassword);
        } else if(index == FlipPassEditorEntryRowUrl) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetEntryUrl);
        } else if(index == FlipPassEditorEntryRowNotes) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetEntryNotes);
        } else if(index == FlipPassEditorEntryRowAutotype) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetEntryAutotype);
        } else if(index == FlipPassEditorEntryRowOtherFields) {
            flippass_editor_open_other_fields(app);
        } else if(index == FlipPassEditorEntryRowOtp) {
            flippass_editor_prepare_otp_form(app);
        } else if(
            app->editor_mode == FlipPassEditorModeEditEntry &&
            index == FlipPassEditorEntryRowDelete) {
            flippass_editor_show_delete_dialog(
                app, FlipPassEditorDialogDeleteEntry, "Delete Entry?", app->editor_entry_title);
        }
        break;
    case FlipPassEditorModeEditOtp:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_otp_save(app);
        } else if(index == FlipPassEditorOtpRowSecret) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetOtpSecret);
        } else if(app->editor_otp_kind == FlipPassOtpKindHmac && index == FlipPassEditorOtpRowCounter) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetOtpCounter);
        } else if(app->editor_otp_settled && index == flippass_editor_last_index(app)) {
            flippass_editor_execute_otp_delete(app);
        }
        break;
    case FlipPassEditorModeRenameFile:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_commit(app);
        } else if(index == FlipPassEditorItemPrimaryText) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetFileName);
        }
        break;
    case FlipPassEditorModeGlobalConfig:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_commit(app);
        }
        break;
    case FlipPassEditorModeAddCustomField:
    case FlipPassEditorModeEditCustomField:
        if(flippass_editor_is_commit_index(app, index)) {
            flippass_editor_execute_commit(app);
        } else if(index == FlipPassEditorItemPrimaryText) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetCustomFieldName);
        } else if(index == 2U) {
            flippass_editor_open_text_target(app, FlipPassEditorTextTargetCustomFieldValue);
        } else if(app->editor_mode == FlipPassEditorModeEditCustomField && index == 4U) {
            flippass_editor_show_delete_dialog(
                app,
                FlipPassEditorDialogDeleteField,
                "Delete Field?",
                app->editor_custom_field_name);
        }
        break;
    case FlipPassEditorModeNone:
    default:
        break;
    }
}

static void flippass_editor_cancel(App* app) {
    const FlipPassEditorMode mode = app->editor_mode;
    const uint32_t return_scene = app->editor_return_scene;

    if(mode == FlipPassEditorModeAddCustomField || mode == FlipPassEditorModeEditCustomField) {
        flippass_editor_restore_parent_mode(app);
        if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, return_scene)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return;
    }

    if(mode == FlipPassEditorModeEditOtp) {
        flippass_editor_restore_after_otp(app);
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, FlipPassScene_Editor)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return;
    }

    flippass_editor_clear_context(app);
    if(((mode == FlipPassEditorModeModifyDatabase && return_scene == FlipPassScene_FileBrowser) ||
        mode == FlipPassEditorModeNewDatabase) &&
       app->database_loaded) {
        flippass_close_database(app);
    }

    if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, return_scene)) {
        scene_manager_previous_scene(app->scene_manager);
    }
}

static char*
    flippass_editor_text_buffer(App* app, FlipPassEditorTextTarget target, size_t* out_size) {
    furi_assert(app);
    furi_assert(out_size);

    switch(target) {
    case FlipPassEditorTextTargetFileName:
        *out_size = sizeof(app->editor_file_name);
        return app->editor_file_name;
    case FlipPassEditorTextTargetDatabasePassword:
        *out_size = sizeof(app->editor_database_password);
        return app->editor_database_password;
    case FlipPassEditorTextTargetGroupName:
        *out_size = sizeof(app->editor_group_name);
        return app->editor_group_name;
    case FlipPassEditorTextTargetEntryTitle:
        *out_size = sizeof(app->editor_entry_title);
        return app->editor_entry_title;
    case FlipPassEditorTextTargetEntryUsername:
        *out_size = sizeof(app->editor_entry_username);
        return app->editor_entry_username;
    case FlipPassEditorTextTargetEntryPassword:
        *out_size = sizeof(app->editor_entry_password);
        return app->editor_entry_password;
    case FlipPassEditorTextTargetEntryUrl:
        *out_size = sizeof(app->editor_entry_url);
        return app->editor_entry_url;
    case FlipPassEditorTextTargetEntryNotes:
        *out_size = sizeof(app->editor_entry_notes);
        return app->editor_entry_notes;
    case FlipPassEditorTextTargetEntryAutotype:
        *out_size = sizeof(app->editor_entry_autotype);
        return app->editor_entry_autotype;
    case FlipPassEditorTextTargetCustomFieldName:
        *out_size = sizeof(app->editor_custom_field_name);
        return app->editor_custom_field_name;
    case FlipPassEditorTextTargetCustomFieldValue:
        *out_size = sizeof(app->editor_custom_field_value);
        return app->editor_custom_field_value;
    case FlipPassEditorTextTargetOtpSecret:
        *out_size = sizeof(app->editor_otp_secret);
        return app->editor_otp_secret;
    case FlipPassEditorTextTargetOtpCounter:
        *out_size = sizeof(app->editor_otp_counter);
        return app->editor_otp_counter;
    case FlipPassEditorTextTargetNone:
    default:
        *out_size = 0U;
        return NULL;
    }
}

static const char* flippass_editor_text_header(const App* app, FlipPassEditorTextTarget target) {
    switch(target) {
    case FlipPassEditorTextTargetFileName:
        return "Database Name";
    case FlipPassEditorTextTargetDatabasePassword:
        return "Database Password";
    case FlipPassEditorTextTargetGroupName:
        return app->editor_mode == FlipPassEditorModeNewDirectory ? "Directory Name" :
                                                                    "Group Name";
    case FlipPassEditorTextTargetEntryTitle:
        return "Entry Title";
    case FlipPassEditorTextTargetEntryUsername:
        return "Entry Username";
    case FlipPassEditorTextTargetEntryPassword:
        return "Entry Password";
    case FlipPassEditorTextTargetEntryUrl:
        return "Entry URL";
    case FlipPassEditorTextTargetEntryNotes:
        return "Entry Notes";
    case FlipPassEditorTextTargetEntryAutotype:
        return "AutoType";
    case FlipPassEditorTextTargetCustomFieldName:
        return "Field Name";
    case FlipPassEditorTextTargetCustomFieldValue:
        return "Field Value";
    case FlipPassEditorTextTargetOtpSecret:
        return "OTP Secret";
    case FlipPassEditorTextTargetOtpCounter:
        return "HOTP Counter";
    case FlipPassEditorTextTargetNone:
    default:
        return "Edit";
    }
}

static bool flippass_editor_text_target_can_generate_password(const App* app) {
    return app != NULL && (app->editor_text_target == FlipPassEditorTextTargetEntryPassword ||
                           (app->editor_text_target == FlipPassEditorTextTargetCustomFieldValue &&
                            app->editor_custom_field_protected));
}

static bool flippass_editor_text_target_select_default(const App* app) {
    return app != NULL && app->editor_mode == FlipPassEditorModeModifyDatabase &&
           app->editor_return_scene == FlipPassScene_FileBrowser &&
           app->editor_text_target == FlipPassEditorTextTargetDatabasePassword &&
           app->editor_database_password[0] != '\0';
}

static FlipPassPasswordGenTarget flippass_editor_password_gen_target(const App* app) {
    if(app == NULL) {
        return FlipPassPasswordGenTargetNone;
    }

    if(app->editor_text_target == FlipPassEditorTextTargetEntryPassword) {
        return FlipPassPasswordGenTargetEntryPassword;
    }

    if(app->editor_text_target == FlipPassEditorTextTargetCustomFieldValue &&
       app->editor_custom_field_protected) {
        return FlipPassPasswordGenTargetProtectedCustomFieldValue;
    }

    return FlipPassPasswordGenTargetNone;
}

static void flippass_editor_text_input_done(void* context) {
    App* app = context;
    const FlipPassEditorTextTarget original_target = app->editor_text_target;
    const FlipPassPasswordGenTarget generator_target = flippass_editor_password_gen_target(app);
    size_t accepted_size = 0U;
    const char* accepted_text = flippass_editor_text_buffer(app, original_target, &accepted_size);

    app->editor_text_target = FlipPassEditorTextTargetNone;
    if(generator_target != FlipPassPasswordGenTargetNone && accepted_text != NULL &&
       accepted_size > 0U && accepted_text[0] == '\0') {
        flippass_password_generator_prepare(app, generator_target);
        scene_manager_previous_scene(app->scene_manager);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordGenerator);
        return;
    }

    if(original_target == FlipPassEditorTextTargetCustomFieldName &&
       app->editor_mode == FlipPassEditorModeAddCustomField) {
        app->editor_selected_index = 1U;
    }

    scene_manager_previous_scene(app->scene_manager);
}

void flippass_scene_editor_on_enter(void* context) {
    App* app = context;

    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_Editor, FlipPassEditorDialogNone);
    flippass_editor_build_form(app);
    if((app->editor_mode == FlipPassEditorModeAddEntry &&
        app->editor_text_target == FlipPassEditorTextTargetEntryTitle) ||
       (app->editor_mode == FlipPassEditorModeAddGroup &&
        app->editor_text_target == FlipPassEditorTextTargetGroupName)) {
        flippass_editor_open_text_target(app, app->editor_text_target);
        return;
    }
    if(app->password_gen_auto_open_field_name &&
       app->editor_mode == FlipPassEditorModeAddCustomField &&
       app->editor_custom_field_name[0] == '\0') {
        app->password_gen_auto_open_field_name = false;
        flippass_editor_open_text_target(app, FlipPassEditorTextTargetCustomFieldName);
        return;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVariableItemList);
}

bool flippass_scene_editor_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    const FlipPassEditorDialogState dialog_state =
        scene_manager_get_scene_state(app->scene_manager, FlipPassScene_Editor);

    if(dialog_state != FlipPassEditorDialogNone) {
        if(event.type == SceneManagerEventTypeBack ||
           (event.type == SceneManagerEventTypeCustom &&
            event.event == (0x100U + DialogExResultLeft))) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_Editor, FlipPassEditorDialogNone);
            dialog_ex_reset(app->dialog_ex);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVariableItemList);
            return true;
        }

        if(event.type == SceneManagerEventTypeCustom &&
           event.event == (0x100U + DialogExResultRight)) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_Editor, FlipPassEditorDialogNone);
            dialog_ex_reset(app->dialog_ex);
            flippass_editor_execute_delete(app, dialog_state);
            return true;
        }

        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        flippass_editor_cancel(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FlipPassEditorEventOtpRebuild) {
            flippass_editor_build_form(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVariableItemList);
            return true;
        }
        flippass_editor_handle_enter(app, event.event - 1U);
        return true;
    }

    return false;
}

void flippass_scene_editor_on_exit(void* context) {
    App* app = context;
    app->editor_selected_index =
        variable_item_list_get_selected_item_index(app->variable_item_list);
    variable_item_list_reset(app->variable_item_list);
    flippass_editor_config_layout_unload_plugin(app);
    dialog_ex_reset(app->dialog_ex);
}

void flippass_scene_editor_trim_for_save(struct App* app) {
    furi_assert(app);

    app->editor_selected_index =
        variable_item_list_get_selected_item_index(app->variable_item_list);
    variable_item_list_reset(app->variable_item_list);
    flippass_editor_config_layout_unload_plugin(app);
    dialog_ex_reset(app->dialog_ex);
}

void flippass_scene_editor_text_input_on_enter(void* context) {
    App* app = context;
    size_t buffer_size = 0U;
    char* buffer = flippass_editor_text_buffer(app, app->editor_text_target, &buffer_size);

    if(buffer == NULL || buffer_size == 0U) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input, flippass_editor_text_header(app, app->editor_text_target));
    text_input_set_result_callback(
        app->text_input,
        flippass_editor_text_input_done,
        app,
        buffer,
        buffer_size,
        flippass_editor_text_target_select_default(app));
    if(flippass_editor_text_target_can_generate_password(app)) {
        text_input_set_minimum_length(app->text_input, 0U);
    }
    text_input_set_is_password(
        app->text_input,
        app->editor_text_target == FlipPassEditorTextTargetDatabasePassword ||
            app->editor_text_target == FlipPassEditorTextTargetEntryPassword ||
            app->editor_text_target == FlipPassEditorTextTargetOtpSecret ||
            (app->editor_text_target == FlipPassEditorTextTargetCustomFieldValue &&
             app->editor_custom_field_protected));
    text_input_set_for_open(app->text_input, false);
    const bool editing_directory_name = app->editor_mode == FlipPassEditorModeNewDirectory &&
                                        app->editor_text_target ==
                                            FlipPassEditorTextTargetGroupName;
    text_input_show_illegal_symbols(
        app->text_input,
        app->editor_text_target == FlipPassEditorTextTargetFileName || editing_directory_name);
    text_input_set_validator(app->text_input, NULL, NULL);

    if(app->editor_text_target == FlipPassEditorTextTargetFileName) {
        text_input_set_validator(app->text_input, flippass_editor_file_name_validator, app);
    } else if(editing_directory_name) {
        text_input_set_validator(app->text_input, flippass_editor_directory_name_validator, app);
    } else if(app->editor_text_target == FlipPassEditorTextTargetDatabasePassword) {
        text_input_set_validator(
            app->text_input, flippass_editor_database_password_validator, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewPasswordEntry);
}

bool flippass_scene_editor_text_input_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        app->editor_text_target = FlipPassEditorTextTargetNone;
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void flippass_scene_editor_text_input_on_exit(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL);
    text_input_set_is_password(app->text_input, false);
    text_input_set_for_open(app->text_input, false);
    text_input_show_illegal_symbols(app->text_input, false);
}
