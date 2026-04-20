#pragma once

#include "include/app/hf_app_state.h"

void hf_switch(HabitFlowApp *app, HfViewId v);
void hf_app_save(HabitFlowApp *app);
void hf_dialog_rebind(HabitFlowApp *app);
void hf_build_credits(HabitFlowApp *app);
void text_input_ok_cb(void *context);
