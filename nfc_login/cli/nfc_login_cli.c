#include <furi.h>
#include <storage/storage.h>
#include <ctype.h>
#include <stdio.h>

#include <cli/cli.h>
#if __has_include(<toolbox/cli/cli_registry.h>)
#include <toolbox/cli/cli_registry.h>
#else
#include <cli/cli_registry.h>
#endif
#include "nfc_login_cli.h"

#include "../nfc_login_app.h"
#include "../crypto/nfc_login_crypto.h"
#include "../crypto/nfc_login_passcode.h"
#include "../settings/nfc_login_settings.h"
#include "../storage/nfc_login_card_storage.h"
#include "../hid/nfc_login_hid.h"


#undef TAG
#define TAG "nfc_login_cli"

static App* g_running_app = NULL;

void nfc_login_cli_set_app_instance(App* app) {
    g_running_app = app;
}

void nfc_login_cli_clear_app_instance(void) {
    g_running_app = NULL;
}
static bool hex_to_bytes(const char* hex_str, uint8_t* bytes, size_t max_len, size_t* out_len) {
    size_t hex_len = strlen(hex_str);
    if(hex_len == 0 || hex_len % 2 != 0 || hex_len / 2 > max_len) {
        return false;
    }
    
    *out_len = hex_len / 2;
    for(size_t i = 0; i < *out_len; i++) {
        char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
        unsigned int byte_val = 0;
        if(sscanf(hex_byte, "%2x", &byte_val) != 1) {
            return false;
        }
        bytes[i] = (uint8_t)byte_val;
    }
    return true;
}

static void bytes_to_hex(const uint8_t* bytes, size_t len, char* hex_out, size_t hex_out_size) {
    if(hex_out_size < len * 2 + 1) return;
    
    for(size_t i = 0; i < len; i++) {
        snprintf(hex_out + i * 2, 3, "%02X", bytes[i]);
    }
    hex_out[len * 2] = '\0';
}

static App* cli_get_app(void) {
    if(g_running_app) {
        return g_running_app;
    }
    App* app = malloc(sizeof(App));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate App structure");
        return NULL;
    }
    
    memset(app, 0, sizeof(App));
    app->card_count = 0;
    app->selected_card = 0;
    app->has_active_selection = false;
    app->active_card_index = 0;
    app->append_enter = true;
    app->input_delay_ms = 10;
    app->layout_loaded = false;
    app->selecting_keyboard_layout = false;
    app->passcode_disabled = false;
    app->hid_mode = HidModeUsb;
    
    strncpy(app->keyboard_layout, "en-US.kl", sizeof(app->keyboard_layout) - 1);
    app->keyboard_layout[sizeof(app->keyboard_layout) - 1] = '\0';
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        File* file = storage_file_alloc(storage);
        if(file) {
            if(storage_file_open(file, NFC_SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
                char line[128];
                size_t len = 0;
                uint8_t ch = 0;
                
                while(true) {
                    len = 0;
                    while(len < sizeof(line) - 1) {
                        uint16_t rd = storage_file_read(file, &ch, 1);
                        if(rd == 0 || ch == '\n') break;
                        line[len++] = (char)ch;
                    }
                    if(len == 0) break;
                    line[len] = '\0';
                    
                    if(strncmp(line, "append_enter=", 13) == 0) {
                        app->append_enter = (atoi(line + 13) != 0);
                    } else if(strncmp(line, "input_delay=", 12) == 0) {
                        int value = atoi(line + 12);
                        if(value == 10 || value == 50 || value == 100 || value == 200) {
                            app->input_delay_ms = (uint16_t)value;
                        }
                    } else if(strncmp(line, "active_card_index=", 18) == 0) {
                        app->active_card_index = (size_t)atoi(line + 18);
                    } else if(strncmp(line, "passcode_disabled=", 18) == 0) {
                        app->passcode_disabled = (atoi(line + 18) != 0);
                    } else if(strncmp(line, "hid_mode=", 9) == 0) {
                        #if HAS_BLE_HID_API
                        int value = atoi(line + 9);
                        if(value == HidModeUsb || value == HidModeBle) {
                            app->hid_mode = (HidMode)value;
                        }
                        #endif
                    }
                    
                    if(storage_file_tell(file) == storage_file_size(file)) break;
                }
                storage_file_close(file);
            }
            storage_file_free(file);
        }
        furi_record_close(RECORD_STORAGE);
    }
    
    if(ensure_crypto_key()) {
        app_load_cards(app);
    }
    
    return app;
}

static void cli_free_app(App* app) {
    if(!app) return;
    if(app == g_running_app) {
        return;
    }
    free(app);
}
static void nfc_login_cli_list(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    
    App* app = cli_get_app();
    if(!app) {
        printf("Error: Failed to initialize app\r\n");
        return;
    }
    
    if(app->card_count == 0) {
        printf("No cards stored.\r\n");
    } else {
        printf("Stored cards:\r\n");
        for(size_t i = 0; i < app->card_count && i < MAX_CARDS; i++) {
            size_t uid_len = app->cards[i].uid_len;
            if(uid_len > MAX_UID_LEN) uid_len = MAX_UID_LEN;
            
            char uid_hex[MAX_UID_LEN * 2 + 1];
            bytes_to_hex(app->cards[i].uid, uid_len, uid_hex, sizeof(uid_hex));
            
            const char* active_marker = (app->has_active_selection && app->selected_card == i) ? "*" : " ";
            const char* card_name = (app->cards[i].name[0] != '\0') ? app->cards[i].name : "(unnamed)";
            printf("  [%zu]%s %s (UID: %s)\r\n", i, active_marker, card_name, uid_hex);
        }
        printf("Total: %zu card(s)\r\n", app->card_count);
    }
    
    cli_free_app(app);
}

static void nfc_login_cli_add(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);
    
    App* app = cli_get_app();
    if(!app) {
        printf("Error: Failed to initialize app\r\n");
        printf("Usage: nfc_login add <name> <uid_hex> <password>\r\n");
        return;
    }
    
    if(app->card_count >= MAX_CARDS) {
        printf( "Error: Maximum card limit (%d) reached.\r\n", MAX_CARDS);
        cli_free_app(app);
        return;
    }
    
    FuriString* name_str = furi_string_alloc();
    FuriString* uid_str = furi_string_alloc();
    FuriString* password_str = furi_string_alloc();
    
    const char* args_cstr = furi_string_get_cstr(args);
    size_t args_len = furi_string_size(args);
    
    if(args_len == 0) {
        printf( "Error: Missing arguments\r\n");
        printf( "Usage: nfc_login add <name> <uid_hex> <password>\r\n");
        furi_string_free(name_str);
        furi_string_free(uid_str);
        furi_string_free(password_str);
        cli_free_app(app);
        return;
    }
    int arg_count = 0;
    const char* start = args_cstr;
    const char* end = args_cstr;
    
    while(*end != '\0' && arg_count < 3) {
        while(*end != '\0' && *end != ' ') end++;
        
        if(arg_count == 0) {
            furi_string_set_strn(name_str, start, end - start);
        } else if(arg_count == 1) {
            furi_string_set_strn(uid_str, start, end - start);
        } else if(arg_count == 2) {
            furi_string_set_strn(password_str, start, strlen(start));
            break;
        }
        
        arg_count++;
        if(*end == ' ') {
            end++;
            start = end;
        }
    }
    
    if(arg_count < 3) {
        printf( "Error: Missing arguments\r\n");
        printf( "Usage: nfc_login add <name> <uid_hex> <password>\r\n");
        furi_string_free(name_str);
        furi_string_free(uid_str);
        furi_string_free(password_str);
        cli_free_app(app);
        return;
    }
    
    const char* name = furi_string_get_cstr(name_str);
    const char* uid_hex = furi_string_get_cstr(uid_str);
    const char* password = furi_string_get_cstr(password_str);
    
    size_t name_len = strlen(name);
    if(name_len == 0 || name_len >= sizeof(app->cards[0].name)) {
        printf( "Error: Invalid name (max %zu chars).\r\n", sizeof(app->cards[0].name) - 1);
        furi_string_free(name_str);
        furi_string_free(uid_str);
        furi_string_free(password_str);
        cli_free_app(app);
        return;
    }
    
    NfcCard* new_card = &app->cards[app->card_count];
    if(!hex_to_bytes(uid_hex, new_card->uid, MAX_UID_LEN, &new_card->uid_len)) {
        printf( "Error: Invalid UID format (must be hex, even number of chars).\r\n");
        furi_string_free(name_str);
        furi_string_free(uid_str);
        furi_string_free(password_str);
        cli_free_app(app);
        return;
    }
    
    size_t password_len = strlen(password);
    if(password_len == 0 || password_len >= MAX_PASSWORD_LEN) {
        printf( "Error: Invalid password (max %d chars).\r\n", MAX_PASSWORD_LEN - 1);
        furi_string_free(name_str);
        furi_string_free(uid_str);
        furi_string_free(password_str);
        cli_free_app(app);
        return;
    }
    
    strncpy(new_card->name, name, sizeof(new_card->name) - 1);
    new_card->name[sizeof(new_card->name) - 1] = '\0';
    strncpy(new_card->password, password, sizeof(new_card->password) - 1);
    new_card->password[sizeof(new_card->password) - 1] = '\0';
    
    app->card_count++;
    
    if(app_save_cards(app)) {
        printf( "Card added successfully: %s\r\n", name);
    } else {
        printf( "Error: Failed to save card.\r\n");
        app->card_count--; // Rollback
    }
    
    furi_string_free(name_str);
    furi_string_free(uid_str);
    furi_string_free(password_str);
    cli_free_app(app);
}

static void nfc_login_cli_delete(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);
    
    App* app = cli_get_app();
    if(!app) {
        printf( "Error: Failed to initialize app\r\n");
        return;
    }
    
    const char* args_cstr = furi_string_get_cstr(args);
    if(strlen(args_cstr) == 0) {
        printf( "Error: Missing index argument\r\n");
        printf( "Usage: nfc_login delete <index>\r\n");
        cli_free_app(app);
        return;
    }
    
    size_t index = (size_t)atoi(args_cstr);
    if(index >= app->card_count) {
        printf( "Error: Invalid index. Use 'nfc_login list' to see available cards.\r\n");
        cli_free_app(app);
        return;
    }
    
    for(size_t i = index; i < app->card_count - 1; i++) {
        app->cards[i] = app->cards[i + 1];
    }
    app->card_count--;
    
    if(app->has_active_selection) {
        if(app->selected_card == index) {
            // Deleted the active card
            app->has_active_selection = false;
            app->active_card_index = 0;
        } else if(app->selected_card > index) {
            app->selected_card--;
            app->active_card_index = app->selected_card;
        }
    }
    
    if(app_save_cards(app)) {
        printf( "Card deleted successfully.\r\n");
        app_save_settings(app);
    } else {
        printf( "Error: Failed to save changes.\r\n");
    }
    
    cli_free_app(app);
}

static void nfc_login_cli_select(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);
    
    App* app = cli_get_app();
    if(!app) {
        printf( "Error: Failed to initialize app\r\n");
        return;
    }
    
    const char* args_cstr = furi_string_get_cstr(args);
    if(strlen(args_cstr) == 0) {
        printf( "Error: Missing index argument\r\n");
        printf( "Usage: nfc_login select <index>\r\n");
        cli_free_app(app);
        return;
    }
    
    size_t index = (size_t)atoi(args_cstr);
    if(index >= app->card_count) {
        printf( "Error: Invalid index. Use 'nfc_login list' to see available cards.\r\n");
        cli_free_app(app);
        return;
    }
    
    app->has_active_selection = true;
    app->selected_card = index;
    app->active_card_index = index;
    
    app_save_settings(app);
    printf("Card selected: %s\r\n", app->cards[index].name);
    
    cli_free_app(app);
}

static void nfc_login_cli_settings(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    
    App* app = cli_get_app();
    if(!app) {
        printf( "Error: Failed to initialize app\r\n");
        return;
    }
    
    printf( "NFC Login Settings:\r\n");
    printf( "  HID Mode: %s\r\n", app->hid_mode == HidModeUsb ? "USB" : "BLE");
    printf( "  Append Enter: %s\r\n", app->append_enter ? "Yes" : "No");
    printf( "  Input Delay: %d ms\r\n", app->input_delay_ms);
    printf( "  Keyboard Layout: %s\r\n", app->keyboard_layout[0] ? app->keyboard_layout : "default");
    printf( "  Passcode Disabled: %s\r\n", app->passcode_disabled ? "Yes" : "No");
    
    if(app->has_active_selection && app->selected_card < app->card_count) {
        printf( "  Active Card: %s\r\n", app->cards[app->selected_card].name);
    } else {
        printf( "  Active Card: None\r\n");
    }
    
    cli_free_app(app);
}

static void nfc_login_cli_set(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);
    
    App* app = cli_get_app();
    if(!app) {
        printf( "Error: Failed to initialize app\r\n");
        return;
    }
    
    const char* args_cstr = furi_string_get_cstr(args);
    if(strlen(args_cstr) == 0) {
        printf( "Error: Missing arguments\r\n");
        printf( "Usage: nfc_login set <setting> <value>\r\n");
        printf( "Settings: hid_mode, append_enter, input_delay, keyboard_layout, passcode_disabled\r\n");
        cli_free_app(app);
        return;
    }
    
    const char* setting_start = args_cstr;
    while(*setting_start == ' ' || *setting_start == '\t') setting_start++;
    
    const char* setting_end = setting_start;
    while(*setting_end != '\0' && *setting_end != ' ' && *setting_end != '\t') {
        setting_end++;
    }
    
    size_t setting_len = setting_end - setting_start;
    char setting[64] = {0};
    if(setting_len > 0 && setting_len < sizeof(setting)) {
        strncpy(setting, setting_start, setting_len);
        setting[setting_len] = '\0';
    }
    
    const char* value_start = setting_end;
    while(*value_start == ' ' || *value_start == '\t') value_start++;
    
    if(*value_start == '\0') {
        printf( "Error: Missing value argument\r\n");
        printf( "Usage: nfc_login set <setting> <value>\r\n");
        cli_free_app(app);
        return;
    }
    
    bool setting_changed = false;
    bool error = false;
    char error_msg[128] = {0};
    
    if(strcmp(setting, "hid_mode") == 0) {
        if(strcmp(value_start, "usb") == 0 || strcmp(value_start, "USB") == 0) {
            app->hid_mode = HidModeUsb;
            setting_changed = true;
        } else if(strcmp(value_start, "ble") == 0 || strcmp(value_start, "BLE") == 0) {
            #if HAS_BLE_HID_API
            app->hid_mode = HidModeBle;
            setting_changed = true;
            #else
            snprintf(error_msg, sizeof(error_msg), "BLE HID not available in this firmware");
            error = true;
            #endif
        } else {
            snprintf(error_msg, sizeof(error_msg), "Invalid value. Use 'usb' or 'ble'");
            error = true;
        }
    } else if(strcmp(setting, "append_enter") == 0) {
        int value = atoi(value_start);
        if(value == 0 || value == 1) {
            app->append_enter = (value == 1);
            setting_changed = true;
        } else {
            snprintf(error_msg, sizeof(error_msg), "Invalid value. Use 0 or 1");
            error = true;
        }
    } else if(strcmp(setting, "input_delay") == 0) {
        int value = atoi(value_start);
        if(value == 10 || value == 50 || value == 100 || value == 200) {
            app->input_delay_ms = (uint16_t)value;
            setting_changed = true;
        } else {
            snprintf(error_msg, sizeof(error_msg), "Invalid value. Use 10, 50, 100, or 200");
            error = true;
        }
    } else if(strcmp(setting, "keyboard_layout") == 0) {
        size_t layout_len = strlen(value_start);
        if(layout_len > 0 && layout_len < sizeof(app->keyboard_layout)) {
            strncpy(app->keyboard_layout, value_start, sizeof(app->keyboard_layout) - 1);
            app->keyboard_layout[sizeof(app->keyboard_layout) - 1] = '\0';
            setting_changed = true;
        } else {
            snprintf(error_msg, sizeof(error_msg), "Invalid layout name (max %zu chars)", sizeof(app->keyboard_layout) - 1);
            error = true;
        }
    } else if(strcmp(setting, "passcode_disabled") == 0) {
        int value = atoi(value_start);
        if(value == 0 || value == 1) {
            app->passcode_disabled = (value == 1);
            setting_changed = true;
        } else {
            snprintf(error_msg, sizeof(error_msg), "Invalid value. Use 0 or 1");
            error = true;
        }
    } else {
        snprintf(error_msg, sizeof(error_msg), "Unknown setting: %s", setting);
        error = true;
    }
    
    if(error) {
        printf( "Error: %s\r\n", error_msg);
        printf( "Usage: nfc_login set <setting> <value>\r\n");
        printf( "Settings: hid_mode, append_enter, input_delay, keyboard_layout, passcode_disabled\r\n");
    } else if(setting_changed) {
        app_save_settings(app);
        printf("Setting '%s' updated successfully\r\n", setting);
    }
    
    cli_free_app(app);
}

static void nfc_login_cli_help(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(args);
    UNUSED(context);
    
    printf("NFC Login CLI Commands:\r\n");
    printf("  list                    - List all stored cards\r\n");
    printf("  add <name> <uid> <pwd>  - Add a new card\r\n");
    printf("  delete <index>          - Delete a card by index\r\n");
    printf("  select <index>          - Select a card as active\r\n");
    printf("  settings                - Show current settings\r\n");
    printf("  set <setting> <value>   - Set a setting\r\n");
    printf("  help                    - Show this help message\r\n");
    printf("\r\n");
    printf("Settings:\r\n");
    printf("  hid_mode <usb|ble>      - Set HID mode (USB or BLE)\r\n");
    printf("  append_enter <0|1>      - Append Enter key after password\r\n");
    printf("  input_delay <ms>        - Set input delay (10, 50, 100, 200)\r\n");
    printf("  keyboard_layout <name>  - Set keyboard layout file name\r\n");
    printf("  passcode_disabled <0|1> - Disable passcode for NFC Login app\r\n");
    printf("\r\n");
    printf("Examples:\r\n");
    printf("  login list\r\n");
    printf("  login add \"Work Badge\" 04A1B2C3D4 MyPassword123\r\n");
    printf("  login delete 0\r\n");
    printf("  login select 1\r\n");
    printf("  login set hid_mode ble\r\n");
    printf("  login set input_delay 50\r\n");
    printf("  login set append_enter 1\r\n");
}

static void nfc_login_cli_command(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);
    
    FuriString* remaining_args = NULL;
    
    if(!args) {
        nfc_login_cli_help(pipe, NULL, NULL);
        return;
    }
    
    const char* args_cstr = furi_string_get_cstr(args);
    if(!args_cstr) {
        nfc_login_cli_help(pipe, NULL, NULL);
        return;
    }
    
    size_t args_len = furi_string_size(args);
    
    while(args_len > 0 && (*args_cstr == ' ' || *args_cstr == '\t')) {
        args_cstr++;
        args_len--;
    }
    
    if(args_len == 0) {
        nfc_login_cli_help(pipe, NULL, NULL);
        return;
    }
    
    const char* cmd_end = args_cstr;
    while(*cmd_end != '\0' && *cmd_end != ' ' && *cmd_end != '\t') {
        cmd_end++;
    }
    
    size_t cmd_len = cmd_end - args_cstr;
    char cmd[32] = {0};
    if(cmd_len > 0 && cmd_len < sizeof(cmd)) {
        strncpy(cmd, args_cstr, cmd_len);
        cmd[cmd_len] = '\0';
    }
    
    remaining_args = furi_string_alloc();
    if(!remaining_args) {
        FURI_LOG_E(TAG, "Failed to allocate remaining_args");
        printf("Error: Memory allocation failed\r\n");
        return;
    }
    
    while(*cmd_end == ' ' || *cmd_end == '\t') cmd_end++;
    if(*cmd_end != '\0') {
        furi_string_set_str(remaining_args, cmd_end);
    }
    if(strcmp(cmd, "list") == 0) {
        nfc_login_cli_list(pipe, remaining_args, NULL);
    } else if(strcmp(cmd, "add") == 0) {
        nfc_login_cli_add(pipe, remaining_args, NULL);
    } else if(strcmp(cmd, "delete") == 0) {
        nfc_login_cli_delete(pipe, remaining_args, NULL);
    } else if(strcmp(cmd, "select") == 0) {
        nfc_login_cli_select(pipe, remaining_args, NULL);
    } else if(strcmp(cmd, "settings") == 0) {
        nfc_login_cli_settings(pipe, remaining_args, NULL);
    } else if(strcmp(cmd, "set") == 0) {
        nfc_login_cli_set(pipe, remaining_args, NULL);
    } else if(strcmp(cmd, "help") == 0) {
        nfc_login_cli_help(pipe, remaining_args, NULL);
    } else {
        printf("Unknown command: %s\r\n", cmd);
        printf("Use 'login help' for usage information.\r\n");
    }
    
    if(remaining_args) {
        furi_string_free(remaining_args);
    }
}

void nfc_login_cli_register_commands(App* app) {
    UNUSED(app);
    
    CliRegistry* cli = furi_record_open(RECORD_CLI);
    if(!cli) {
        FURI_LOG_E(TAG, "Failed to open CLI record");
        return;
    }
    
    cli_registry_add_command(cli, "login", CliCommandFlagParallelSafe, nfc_login_cli_command, NULL);
    
    furi_record_close(RECORD_CLI);
}

void nfc_login_cli_unregister_commands(void) {
    CliRegistry* cli = furi_record_open(RECORD_CLI);
    if(!cli) {
        FURI_LOG_E(TAG, "Failed to open CLI record for unregister");
        return;
    }
    
    cli_registry_delete_command(cli, "login");
    
    furi_record_close(RECORD_CLI);
}

