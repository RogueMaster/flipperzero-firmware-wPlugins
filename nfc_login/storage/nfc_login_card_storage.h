#pragma once

#include "../nfc_login_app.h"

void app_load_cards(App* app);
bool app_save_cards(App* app);
void app_ensure_data_dir(Storage* storage);