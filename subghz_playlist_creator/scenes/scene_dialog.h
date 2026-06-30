#pragma once
#include "../subghz_playlist_creator.h"
#include <gui/modules/dialog_ex.h>

void scene_dialog_show(SubGhzPlaylistCreator* app);
void scene_dialog_init_view(SubGhzPlaylistCreator* app);
void scene_dialog_show_custom(
    SubGhzPlaylistCreator* app,
    const char* header,
    const char* text,
    const char* left_btn,
    const char* right_btn,
    DialogExResultCallback callback,
    void* context
); 