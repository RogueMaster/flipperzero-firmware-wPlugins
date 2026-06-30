#pragma once

#include <furi.h>
#include "../nfc_login_app.h"

void nfc_login_cli_register_commands(App* app);
void nfc_login_cli_unregister_commands(void);
void nfc_login_cli_set_app_instance(App* app);
void nfc_login_cli_clear_app_instance(void);

