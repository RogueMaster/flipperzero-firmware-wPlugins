#pragma once

#include "subghz_editor_model.h"

bool subghz_editor_file_load(SubGhzEditorApp* app, const char* path);
bool subghz_editor_file_save(SubGhzEditorApp* app, const char* target_path);
bool subghz_editor_file_save_copy(SubGhzEditorApp* app);
