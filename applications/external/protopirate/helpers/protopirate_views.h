#pragma once

#include <stdbool.h>

typedef struct ProtoPirateApp ProtoPirateApp;

bool protopirate_ensure_variable_item_list(ProtoPirateApp* app);
bool protopirate_ensure_widget(ProtoPirateApp* app);
bool protopirate_ensure_text_input(ProtoPirateApp* app);
bool protopirate_ensure_view_about(ProtoPirateApp* app);
bool protopirate_ensure_receiver_view(ProtoPirateApp* app);
void protopirate_views_free(ProtoPirateApp* app);
