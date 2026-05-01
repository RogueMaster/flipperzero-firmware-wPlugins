#pragma once

#include <furi.h>
#include "weebo_file_list.h"

typedef struct Weebo Weebo;

void weebo_calculate_pwd(uint8_t* uid, uint8_t* pwd);
void weebo_remix(Weebo* weebo);
bool weebo_load_figure(Weebo* weebo, FuriString* path, bool show_dialog);
bool weebo_scan_nfc_files(Weebo* weebo, const char* directory);
void weebo_free_nfc_file_list(Weebo* weebo);
bool weebo_load_current_file(Weebo* weebo);

size_t weebo_get_file_count(Weebo* weebo);
size_t weebo_get_current_file_index(Weebo* weebo);
bool weebo_set_current_file_path(Weebo* weebo, const char* path);

bool weebo_cycle_file(Weebo* weebo, WeeboCycleDirection direction);
