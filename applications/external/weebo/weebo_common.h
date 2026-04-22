#pragma once

#include <furi.h>
#include <weebo_i.h>

void weebo_calculate_pwd(uint8_t* uid, uint8_t* pwd);
void weebo_remix(Weebo* weebo);
bool weebo_load_figure(Weebo* weebo, FuriString* path, bool show_dialog);
bool weebo_scan_nfc_files(Weebo* weebo, const char* directory);
void weebo_free_nfc_file_list(Weebo* weebo);
bool weebo_load_current_file(Weebo* weebo);
bool weebo_cycle_to_next_file(Weebo* weebo);
bool weebo_cycle_to_prev_file(Weebo* weebo);
