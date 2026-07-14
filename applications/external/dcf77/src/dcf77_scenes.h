#ifndef DCF77_SCENES_H
#define DCF77_SCENES_H

#include "dcf77_app.h"

void dcf77_app_switch_to_menu(AppFSM* app_fsm);
void dcf77_app_switch_to_advanced_menu(AppFSM* app_fsm);
void dcf77_app_switch_to_about(AppFSM* app_fsm);
void dcf77_app_switch_to_egg(AppFSM* app_fsm);
void dcf77_app_switch_to_lf_settings(AppFSM* app_fsm);
void dcf77_app_switch_to_tx_ratio_settings(AppFSM* app_fsm);
void dcf77_app_switch_to_experimental_time_settings(AppFSM* app_fsm);
void dcf77_app_switch_to_subghz_settings(AppFSM* app_fsm);
void dcf77_app_switch_to_debug_settings(AppFSM* app_fsm);
void dcf77_app_switch_to_gpio_rf_warning(AppFSM* app_fsm, uint8_t pending_pin);
void dcf77_app_start_tx(AppFSM* app_fsm);
void dcf77_app_stop_tx(AppFSM* app_fsm);

uint32_t dcf77_tx_previous_callback(void* ctx);
uint32_t dcf77_text_previous_callback(void* ctx);
void dcf77_egg_button_callback(GuiButtonType result, InputType type, void* ctx);
bool dcf77_egg_input_callback(InputEvent* event, void* ctx);
bool dcf77_tx_input_callback(InputEvent* event, void* ctx);
void dcf77_menu_callback(void* ctx, uint32_t index);
void dcf77_advanced_menu_callback(void* ctx, uint32_t index);
bool dcf77_navigation_callback(void* ctx);
void dcf77_tick_callback(void* ctx);

void dcf77_lf_transmit_change_callback(VariableItem* item);
void dcf77_lf_frequency_change_callback(VariableItem* item);
bool dcf77_lf_settings_input_callback(InputEvent* event, void* ctx);
void dcf77_lf_settings_enter_callback(void* ctx, uint32_t index);
bool dcf77_tx_ratio_settings_input_callback(InputEvent* event, void* ctx);
bool dcf77_experimental_time_settings_input_callback(InputEvent* event, void* ctx);
void dcf77_experimental_time_settings_enter_callback(void* ctx, uint32_t index);
uint32_t dcf77_preset_time_input_previous_callback(void* ctx);
bool dcf77_subghz_settings_input_callback(InputEvent* event, void* ctx);
void dcf77_subghz_settings_enter_callback(void* ctx, uint32_t index);
uint32_t dcf77_subghz_freq_input_previous_callback(void* ctx);
void dcf77_subghz_freq_input_result_callback(void* ctx, int32_t number);
bool dcf77_debug_settings_input_callback(InputEvent* event, void* ctx);
uint32_t dcf77_gpio_rf_warning_previous_callback(void* ctx);
bool dcf77_gpio_rf_warning_input_callback(InputEvent* event, void* ctx);

#endif
