#pragma once
#include <gui/gui.h>
#include <gui/view_port.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_port.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/dialog_ex.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

// TODO: VOID HARDCODED STUFF
#define MENU_ITEMS     3
#define PASS_SAVE_PATH APP_DATA_PATH("passwordManager.txt")

// Define view IDs
typedef enum {
    ViewMainMenu,
    ViewSavedPasswords,
    ViewDeletePassword,
    ViewTextInputCredentialName,
    ViewTextInputUsername,
    ViewTextInputPassword
} ViewID;

typedef struct {
    char name[100];
    char username[100];
    char password[100];
} Credential;

typedef struct {
    const char* items[MENU_ITEMS];
    size_t selected;
    size_t scroll_offset;
    bool running;

    // For view management
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* main_menu_view;
    View* saved_passwords_view;
    View* delete_password_view;

    Credential credentials[100];
    size_t credentials_number;

    char tmp_credential_name[100];
    char tmp_username[100];
    char tmp_password[100];
    TextInput* textInput_credential_name;
    TextInput* textInput_username;
    TextInput* textInput_password;

    bool confirm_delete;

} AppContext;
