#pragma once
#include "../app.h"

uint32_t uhf_reader_navigation_configure_callback(void* context);
uint32_t uhf_reader_navigation_config_submenu_callback(void* context);

void uhf_reader_setting_1_change(VariableItem* Item);
void uhf_reader_power_setting_change(VariableItem* Item);
void uhf_reader_baud_setting_change(VariableItem* Item);
void uhf_reader_region_setting_change(VariableItem* Item);
void uhf_reader_session_setting_change(VariableItem* Item);
void uhf_reader_target_setting_change(VariableItem* Item);
void uhf_reader_save_setting_change(VariableItem* Item);
void uhf_reader_multi_full_setting_change(VariableItem* Item);
void uhf_reader_auto_save_multi_setting_change(VariableItem* Item);
void uhf_reader_tag_profile_setting_change(VariableItem* Item);
void uhf_reader_full_dump_attempts_setting_change(VariableItem* Item);
void uhf_reader_clone_attempts_setting_change(VariableItem* Item);
void uhf_reader_forever_delay_setting_change(VariableItem* Item);
void uhf_reader_sound_setting_change(VariableItem* Item);
void uhf_reader_vibration_setting_change(VariableItem* Item);

void uhf_reader_setting_6_text_updated(void* context);
void uhf_reader_setting_item_clicked(void* context, uint32_t index);
void uhf_reader_delete_all_confirm_callback(DialogExResult result, void* context);

void ap_menu_alloc(UHFReaderApp* App);
void view_config_alloc(UHFReaderApp* App);
void view_config_free(UHFReaderApp* App);

bool uhf_reader_auto_connect(UHFReaderApp* App);
