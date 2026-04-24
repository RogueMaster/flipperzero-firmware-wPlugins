#pragma once

#include "../aprs.h"
#include "../flipperham.h"
#include "../rf_gen.h"
#include "ui.h"

extern FlipperHamApp *gapp;

uint32_t flipperham_exit_callback(void *context);
uint32_t flipperham_send_exit_callback(void *context);
uint32_t flipperham_settings_exit_callback(void *context);
uint32_t flipperham_bulletin_exit_callback(void *context);
uint32_t flipperham_status_exit_callback(void *context);
uint32_t flipperham_message_exit_callback(void *context);
uint32_t flipperham_message_edit_exit_callback(void *context);
uint32_t flipperham_position_exit_callback(void *context);
uint32_t flipperham_ssid_exit_callback(void *context);
uint32_t flipperham_call_exit_callback(void *context);
uint32_t flipperham_freq_exit_callback(void *context);
uint32_t flipperham_freq_edit_exit_callback(void *context);
uint32_t flipperham_pos_edit_exit_callback(void *context);
uint32_t flipperham_pos_action_exit_callback(void *context);
uint32_t flipperham_ham_exit_callback(void *context);
uint32_t flipperham_ham_tx_exit_callback(void *context);
uint32_t flipperham_readme_exit_callback(void *context);
uint32_t book_exit(void *context);
uint32_t book_action_exit(void *context);
uint32_t flipperham_text_exit_callback(void *context);

void flipperham_menu_callback(void *context, uint32_t index);
void flipperham_send_callback(void *context, uint32_t index);
void readme_back(GuiButtonType result, InputType type, void *context);

void bulletin_menu_build(FlipperHamApp *app);
void status_menu_build(FlipperHamApp *app);
void message_menu_build(FlipperHamApp *app);
void position_menu_build(FlipperHamApp *app);
void call_menu_build(FlipperHamApp *app);
void book_menu_build(FlipperHamApp *app);
void book_action_menu_build(FlipperHamApp *app);
void ssid_menu_build(FlipperHamApp *app);
void settings_menu_build(FlipperHamApp *app);
void ham_menu_build(FlipperHamApp *app);
void ham_tx_menu_build(FlipperHamApp *app);
void freq_menu_build(FlipperHamApp *app);
void freq_edit_menu_build(FlipperHamApp *app);
void pos_edit_menu_build(FlipperHamApp *app);

void ssidfix(FlipperHamApp *app);
void ssid_change(VariableItem *item);
void baud_change(VariableItem *item);
void profile_change(VariableItem *item);
void deviation_change(VariableItem *item);
void repeat_change(VariableItem *item);
void leadin_change(VariableItem *item);
void preamble_change(VariableItem *item);
void ham_call_change(VariableItem *item);
void ham_ssid_change(VariableItem *item);
void freq_change(VariableItem *item);

void ssid_enter(void *context, uint32_t index);
void settings_enter(void *context, uint32_t index);
void ham_enter(void *context, uint32_t index);
void ham_tx_enter(void *context, uint32_t index);
void freq_edit_enter(void *context, uint32_t index);
void pos_edit_enter(void *context, uint32_t index);

uint32_t freq_step(uint32_t a, int8_t d);
void freq_show(char *o, uint16_t n, uint32_t a);
void fsh2(char *o, uint16_t n, uint32_t a);

void flipperham_bulletin_callback(void *context, uint32_t index);
void bulletin_pick(void *context, InputType input_type, uint32_t index);
void st(void *context, uint32_t index);
void status_pick(void *context, InputType input_type, uint32_t index);
void message_pick(void *context, InputType input_type, uint32_t index);
void m(void *context, uint32_t index);
void p(void *context, uint32_t index);
void position_pick(void *context, InputType input_type, uint32_t index);
void call_pick(void *context, InputType input_type, uint32_t index);
void cl(void *context, uint32_t index);
void cb(void *context, uint32_t index);
void freq_pick(void *context, InputType input_type, uint32_t index);
void bulletin_save(void *context);
void status_save(void *context);
void message_save(void *context);
void position_save(void *context);
void call_save(void *context);
void freq_save(void *context);

void flipperham_draw_callback(Canvas *canvas, void *context);
void flipperham_status_view_alloc(FlipperHamApp *app);
void flipperham_status_view_free(FlipperHamApp *app);
void flipperham_menu_free(FlipperHamApp *app);
uint32_t repeat_scale(FlipperHamApp *app);

FlipperHamApp *flipperham_app_alloc(void);
void flipperham_app_free(FlipperHamApp *app);
void flipperham_send_hardcoded_message(FlipperHamApp *app);
