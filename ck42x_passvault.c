#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ck42x_passvault_icons.h>

#define CK_TAG          "CK42XPassVault"
#define CK_MAX_ENTRIES  20
#define CK_ACCOUNT_LEN  32
#define CK_USERNAME_LEN 48
#define CK_PASSWORD_LEN 72
#define CK_INPUT_LEN    72
#define CK_LINE_MAX     180
#define CK_VAULT_FILE   APP_DATA_PATH("vault.tsv")

typedef struct {
    char account[CK_ACCOUNT_LEN];
    char username[CK_USERNAME_LEN];
    char password[CK_PASSWORD_LEN];
} CkVaultEntry;

typedef enum {
    CkViewMain = 0,
    CkViewTextInput,
    CkViewWidget,
    CkViewDialog,
} CkView;

typedef enum {
    CkMenuModeMain = 0,
    CkMenuModeGenerateOrCustom,
    CkMenuModePreset,
} CkMenuMode;

typedef enum {
    CkInputAccount = 0,
    CkInputUsername,
    CkInputCustomPassword,
} CkInputStage;

typedef enum {
    CkDialogSave = 0,
    CkDialogInjectConfirm,
} CkDialogPurpose;

typedef enum {
    CkEventAdd = 1,
    CkEventAbout = 2,
    CkEventSavedBase = 100,
    CkEventTextDone = 300,
    CkEventChooseGenerate = 400,
    CkEventChooseCustom = 401,
    CkEventPresetMemorable = 500,
    CkEventPresetStrict = 501,
    CkEventPresetLong = 502,
    CkEventPresetNoSymbol = 503,
    CkEventWidgetBack = 600,
    CkEventWidgetInject = 601,
    CkEventDialogRight = 700,
    CkEventDialogLeft = 701,
} CkEvent;

typedef enum {
    CkPresetMemorable = 0,
    CkPresetStrict,
    CkPresetLong,
    CkPresetNoSymbol,
} CkPreset;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    DialogEx* dialog;
    Storage* storage;

    CkVaultEntry entries[CK_MAX_ENTRIES];
    uint8_t entry_count;
    int8_t selected;

    CkVaultEntry draft;
    char input[CK_INPUT_LEN];
    CkView current_view;
    CkMenuMode menu_mode;
    CkInputStage input_stage;
    CkDialogPurpose dialog_purpose;
    FuriHalUsbInterface* previous_usb;
} CkApp;

static void ck_show_main(CkApp* app);
static void ck_show_text_input(CkApp* app, CkInputStage stage, const char* header, char* initial);
static void ck_show_save_dialog(CkApp* app);
static void ck_show_entry_widget(CkApp* app);
static void ck_show_about(CkApp* app);
static void ck_show_inject_confirm(CkApp* app);
static void ck_handle_event(CkApp* app, uint32_t event);

static void ck_copy(char* dst, size_t dst_size, const char* src) {
    if(dst_size == 0) return;
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static void ck_sanitize(char* text) {
    for(char* p = text; *p; p++) {
        if(*p == '\t' || *p == '\r' || *p == '\n') *p = ' ';
    }
}

static uint32_t ck_random_index(uint32_t max) {
    if(max == 0) return 0;
    return furi_hal_random_get() % max;
}

static const char* const ck_adjs[] = {
    "Amber",
    "Atomic",
    "Black",
    "Bright",
    "Cyber",
    "Drift",
    "Echo",
    "Iron",
    "Lunar",
    "Neon",
    "Nova",
    "Obsidian",
    "Rapid",
    "Solar",
    "Stone",
    "Velvet"};

static const char* const ck_nouns[] = {
    "Badger",
    "Falcon",
    "Harbor",
    "Mantis",
    "Otter",
    "Pioneer",
    "Raven",
    "River",
    "Rocket",
    "Signal",
    "Tiger",
    "Vector",
    "Wolf",
    "Anchor",
    "Forge",
    "Summit"};

static const char* const ck_symbols = "!@#$%&*?";
static const char* const ck_chars_upper = "ABCDEFGHJKLMNPQRSTUVWXYZ";
static const char* const ck_chars_lower = "abcdefghijkmnopqrstuvwxyz";
static const char* const ck_chars_digits = "23456789";

static char ck_pick_from(const char* alphabet) {
    return alphabet[ck_random_index(strlen(alphabet))];
}

static void ck_generate_password(CkPreset preset, char* out, size_t out_size) {
    const char* a1 = ck_adjs[ck_random_index(COUNT_OF(ck_adjs))];
    const char* a2 = ck_adjs[ck_random_index(COUNT_OF(ck_adjs))];
    const char* n1 = ck_nouns[ck_random_index(COUNT_OF(ck_nouns))];
    const char* n2 = ck_nouns[ck_random_index(COUNT_OF(ck_nouns))];
    uint32_t num = 10 + ck_random_index(90);
    char sym = ck_symbols[ck_random_index(strlen(ck_symbols))];

    if(preset == CkPresetMemorable) {
        snprintf(out, out_size, "%s%s-%s%lu%c", a1, n1, n2, (unsigned long)num, sym);
    } else if(preset == CkPresetLong) {
        snprintf(
            out,
            out_size,
            "%s-%s-%s%lu%c",
            a1,
            n1,
            n2,
            (unsigned long)(100 + ck_random_index(900)),
            sym);
    } else if(preset == CkPresetNoSymbol) {
        snprintf(
            out, out_size, "%s%s%s%lu", a1, n1, a2, (unsigned long)(100 + ck_random_index(900)));
    } else {
        /* Strict mixed 16+ while still chunked enough to read aloud. */
        char tail[9];
        for(size_t i = 0; i < sizeof(tail) - 1; i++) {
            switch(i % 4) {
            case 0:
                tail[i] = ck_pick_from(ck_chars_upper);
                break;
            case 1:
                tail[i] = ck_pick_from(ck_chars_lower);
                break;
            case 2:
                tail[i] = ck_pick_from(ck_chars_digits);
                break;
            default:
                tail[i] = ck_pick_from(ck_symbols);
                break;
            }
        }
        tail[sizeof(tail) - 1] = '\0';
        snprintf(out, out_size, "%s%s-%s", a1, n1, tail);
    }
}

static FuriString* ck_vault_path(CkApp* app) {
    FuriString* path = furi_string_alloc_set(CK_VAULT_FILE);
    storage_common_resolve_path_and_ensure_app_directory(app->storage, path);
    return path;
}

static void ck_load_entries(CkApp* app) {
    app->entry_count = 0;
    FuriString* path = ck_vault_path(app);
    File* file = storage_file_alloc(app->storage);

    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size > 0 && size < 4096) {
            char* buf = malloc(size + 1);
            if(buf) {
                size_t read = storage_file_read(file, buf, size);
                buf[read] = '\0';
                char* line = buf;
                while(line && *line && app->entry_count < CK_MAX_ENTRIES) {
                    char* next = strchr(line, '\n');
                    if(next) {
                        *next = '\0';
                        next++;
                    }
                    char* tab1 = strchr(line, '\t');
                    if(tab1) {
                        *tab1 = '\0';
                        char* tab2 = strchr(tab1 + 1, '\t');
                        if(tab2) {
                            *tab2 = '\0';
                            CkVaultEntry* e = &app->entries[app->entry_count++];
                            ck_copy(e->account, sizeof(e->account), line);
                            ck_copy(e->username, sizeof(e->username), tab1 + 1);
                            ck_copy(e->password, sizeof(e->password), tab2 + 1);
                        }
                    }
                    line = next;
                }
                free(buf);
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
}

static bool ck_save_entries(CkApp* app) {
    FuriString* path = ck_vault_path(app);
    File* file = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = true;
        for(uint8_t i = 0; i < app->entry_count; i++) {
            char line[CK_LINE_MAX];
            int len = snprintf(
                line,
                sizeof(line),
                "%s\t%s\t%s\n",
                app->entries[i].account,
                app->entries[i].username,
                app->entries[i].password);
            if(len <= 0 || storage_file_write(file, line, strlen(line)) != strlen(line)) {
                ok = false;
                break;
            }
        }
        storage_file_sync(file);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    return ok;
}

static void ck_submenu_callback(void* context, uint32_t index) {
    CkApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, index);
}

static void ck_text_input_callback(void* context) {
    CkApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, CkEventTextDone);
}

static void ck_widget_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    CkApp* app = context;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventWidgetBack);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventWidgetInject);
    }
}

static void ck_dialog_callback(DialogExResult result, void* context) {
    CkApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventDialogRight);
    } else if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventDialogLeft);
    }
}

static bool ck_custom_event_callback(void* context, uint32_t event) {
    ck_handle_event(context, event);
    return true;
}

static bool ck_navigation_callback(void* context) {
    CkApp* app = context;
    if(app->current_view == CkViewMain && app->menu_mode == CkMenuModeMain) {
        view_dispatcher_stop(app->dispatcher);
    } else {
        ck_show_main(app);
    }
    return true;
}

static void ck_show_main(CkApp* app) {
    app->menu_mode = CkMenuModeMain;
    app->selected = -1;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "CK42X PassVault");
    submenu_add_item(app->submenu, "+ Add New Password", CkEventAdd, ck_submenu_callback, app);
    submenu_add_item(app->submenu, "About / ck42x.com", CkEventAbout, ck_submenu_callback, app);
    for(uint8_t i = 0; i < app->entry_count; i++) {
        submenu_add_item(
            app->submenu, app->entries[i].account, CkEventSavedBase + i, ck_submenu_callback, app);
    }
    app->current_view = CkViewMain;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewMain);
}

static void ck_show_generate_or_custom(CkApp* app) {
    app->menu_mode = CkMenuModeGenerateOrCustom;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Password Source");
    submenu_add_item(
        app->submenu, "Generate Password", CkEventChooseGenerate, ck_submenu_callback, app);
    submenu_add_item(app->submenu, "Enter Custom", CkEventChooseCustom, ck_submenu_callback, app);
    app->current_view = CkViewMain;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewMain);
}

static void ck_show_preset_menu(CkApp* app) {
    app->menu_mode = CkMenuModePreset;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Requirement Preset");
    submenu_add_item(
        app->submenu, "Memorable 16+ mix", CkEventPresetMemorable, ck_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Strict 16+ A/a/0/!", CkEventPresetStrict, ck_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Long 20+ passphrase", CkEventPresetLong, ck_submenu_callback, app);
    submenu_add_item(
        app->submenu, "No special char", CkEventPresetNoSymbol, ck_submenu_callback, app);
    app->current_view = CkViewMain;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewMain);
}

static void ck_show_text_input(CkApp* app, CkInputStage stage, const char* header, char* initial) {
    app->input_stage = stage;
    text_input_reset(app->text_input);
    memset(app->input, 0, sizeof(app->input));
    if(initial) ck_copy(app->input, sizeof(app->input), initial);
    text_input_set_header_text(app->text_input, header);
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input, ck_text_input_callback, app, app->input, sizeof(app->input), false);
    app->current_view = CkViewTextInput;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewTextInput);
}

static void ck_show_entry_widget(CkApp* app) {
    if(app->selected < 0 || app->selected >= app->entry_count) {
        ck_show_main(app);
        return;
    }
    CkVaultEntry* e = &app->entries[app->selected];
    char text[320];
    snprintf(
        text,
        sizeof(text),
        "\e#%s\n\e#User: %s\n\e*Pass: %s\e*\n\nRight = HID type password",
        e->account,
        e->username,
        e->password);

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, text);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", ck_widget_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Inject", ck_widget_button_callback, app);
    app->current_view = CkViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewWidget);
}

static void ck_show_save_dialog(CkApp* app) {
    char text[240];
    snprintf(
        text,
        sizeof(text),
        "Acct: %s\nUser: %s\nPass: %s",
        app->draft.account,
        app->draft.username,
        app->draft.password);
    app->dialog_purpose = CkDialogSave;
    dialog_ex_reset(app->dialog);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, ck_dialog_callback);
    dialog_ex_set_header(app->dialog, "Save CK42X Entry?", 64, 6, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, text, 4, 18, AlignLeft, AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "No");
    dialog_ex_set_right_button_text(app->dialog, "Save");
    app->current_view = CkViewDialog;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewDialog);
}

static void ck_show_inject_confirm(CkApp* app) {
    if(app->selected < 0 || app->selected >= app->entry_count) {
        ck_show_main(app);
        return;
    }
    app->dialog_purpose = CkDialogInjectConfirm;
    dialog_ex_reset(app->dialog);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, ck_dialog_callback);
    dialog_ex_set_header(app->dialog, "HID Inject?", 64, 8, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog,
        "Focus the password field.\nRight types password only.\nLeft cancels.",
        4,
        20,
        AlignLeft,
        AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "Cancel");
    dialog_ex_set_right_button_text(app->dialog, "Type");
    app->current_view = CkViewDialog;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewDialog);
}

static void ck_show_status(CkApp* app, const char* header, const char* message) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, header);
    widget_add_text_box_element(
        app->widget, 4, 20, 120, 30, AlignCenter, AlignCenter, message, false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", ck_widget_button_callback, app);
    app->current_view = CkViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewWidget);
}

static void ck_show_about(CkApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "CK42X PassVault");
    widget_add_text_box_element(
        app->widget,
        4,
        20,
        120,
        32,
        AlignCenter,
        AlignCenter,
        "Field password vault\nBuild. Code. Transmute.\nck42x.com",
        false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", ck_widget_button_callback, app);
    app->current_view = CkViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewWidget);
}

static void ck_hid_type_string(const char* text) {
    for(const char* p = text; *p; p++) {
        uint16_t key = HID_ASCII_TO_KEY(*p);
        if(key == HID_KEYBOARD_NONE) continue;
        furi_hal_hid_kb_press(key);
        furi_delay_ms(18);
        furi_hal_hid_kb_release_all();
        furi_delay_ms(12);
    }
}

static bool ck_inject_selected(CkApp* app) {
    if(app->selected < 0 || app->selected >= app->entry_count) return false;
    app->previous_usb = furi_hal_usb_get_config();
    if(app->previous_usb != &usb_hid) {
        if(!furi_hal_usb_set_config(&usb_hid, NULL)) return false;
        furi_delay_ms(800);
    }

    ck_hid_type_string(app->entries[app->selected].password);
    furi_hal_hid_kb_release_all();
    furi_delay_ms(100);

    if(app->previous_usb && app->previous_usb != &usb_hid) {
        furi_hal_usb_set_config(app->previous_usb, NULL);
    }
    return true;
}

static void ck_handle_text_done(CkApp* app) {
    ck_sanitize(app->input);
    if(app->input_stage == CkInputAccount) {
        memset(&app->draft, 0, sizeof(app->draft));
        ck_copy(app->draft.account, sizeof(app->draft.account), app->input);
        ck_show_text_input(app, CkInputUsername, "Username", NULL);
    } else if(app->input_stage == CkInputUsername) {
        ck_copy(app->draft.username, sizeof(app->draft.username), app->input);
        ck_show_generate_or_custom(app);
    } else if(app->input_stage == CkInputCustomPassword) {
        ck_copy(app->draft.password, sizeof(app->draft.password), app->input);
        ck_show_save_dialog(app);
    }
}

static void ck_save_draft(CkApp* app) {
    if(app->entry_count >= CK_MAX_ENTRIES) {
        ck_show_status(app, "Vault Full", "Max entries reached.\nDelete by editing vault.tsv.");
        return;
    }
    app->entries[app->entry_count++] = app->draft;
    bool ok = ck_save_entries(app);
    if(ok) {
        ck_show_status(app, "Saved", "Entry saved to\nCK42X PassVault.");
    } else {
        ck_show_status(app, "Save Failed", "SD/app data write failed.");
    }
}

static void ck_handle_event(CkApp* app, uint32_t event) {
    if(event == CkEventAdd) {
        ck_show_text_input(app, CkInputAccount, "Account Name", NULL);
    } else if(event == CkEventAbout) {
        ck_show_about(app);
    } else if(event >= CkEventSavedBase && event < CkEventSavedBase + CK_MAX_ENTRIES) {
        uint32_t idx = event - CkEventSavedBase;
        if(idx < app->entry_count) {
            app->selected = idx;
            ck_show_entry_widget(app);
        }
    } else if(event == CkEventTextDone) {
        ck_handle_text_done(app);
    } else if(event == CkEventChooseGenerate) {
        ck_show_preset_menu(app);
    } else if(event == CkEventChooseCustom) {
        ck_show_text_input(app, CkInputCustomPassword, "Custom Password", NULL);
    } else if(
        event == CkEventPresetMemorable || event == CkEventPresetStrict ||
        event == CkEventPresetLong || event == CkEventPresetNoSymbol) {
        CkPreset preset = CkPresetMemorable;
        if(event == CkEventPresetStrict) preset = CkPresetStrict;
        if(event == CkEventPresetLong) preset = CkPresetLong;
        if(event == CkEventPresetNoSymbol) preset = CkPresetNoSymbol;
        ck_generate_password(preset, app->draft.password, sizeof(app->draft.password));
        ck_show_save_dialog(app);
    } else if(event == CkEventWidgetBack) {
        ck_show_main(app);
    } else if(event == CkEventWidgetInject) {
        ck_show_inject_confirm(app);
    } else if(event == CkEventDialogLeft) {
        if(app->dialog_purpose == CkDialogInjectConfirm)
            ck_show_entry_widget(app);
        else
            ck_show_main(app);
    } else if(event == CkEventDialogRight) {
        if(app->dialog_purpose == CkDialogSave) {
            ck_save_draft(app);
        } else if(app->dialog_purpose == CkDialogInjectConfirm) {
            bool ok = ck_inject_selected(app);
            ck_show_status(
                app,
                ok ? "Typed" : "HID Failed",
                ok ? "Password HID typed." : "Could not switch USB HID.");
        }
    }
}

static CkApp* ck_app_alloc(void) {
    CkApp* app = malloc(sizeof(CkApp));
    furi_assert(app);
    memset(app, 0, sizeof(CkApp));
    app->selected = -1;

    app->storage = furi_record_open(RECORD_STORAGE);
    ck_load_entries(app);

    app->dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->widget = widget_alloc();
    app->dialog = dialog_ex_alloc();

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, ck_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, ck_navigation_callback);

    view_dispatcher_add_view(app->dispatcher, CkViewMain, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->dispatcher, CkViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->dispatcher, CkViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->dispatcher, CkViewDialog, dialog_ex_get_view(app->dialog));

    return app;
}

static void ck_app_free(CkApp* app) {
    if(!app) return;
    view_dispatcher_remove_view(app->dispatcher, CkViewMain);
    view_dispatcher_remove_view(app->dispatcher, CkViewTextInput);
    view_dispatcher_remove_view(app->dispatcher, CkViewWidget);
    view_dispatcher_remove_view(app->dispatcher, CkViewDialog);
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_STORAGE);
    free(app);
}

int32_t ck42x_passvault_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(CK_TAG, "Starting CK42X PassVault");

    CkApp* app = ck_app_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    ck_show_main(app);
    view_dispatcher_run(app->dispatcher);

    furi_record_close(RECORD_GUI);
    ck_app_free(app);
    FURI_LOG_I(CK_TAG, "Stopped CK42X PassVault");
    return 0;
}
