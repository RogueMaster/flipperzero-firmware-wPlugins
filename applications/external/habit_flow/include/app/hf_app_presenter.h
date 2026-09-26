#pragma once

#include "include/app/hf_app_state.h"
#include <stddef.h>

void hf_switch(HabitFlowApp* app, HfViewId v);
void hf_request_redraw(HabitFlowApp* app);

// Each wrapper opens the storage record, runs the matching habit_store_* call through the
// furi-backed port, and closes it again. All return false without changing anything on
// disk when the store currently refuses to save (see HabitStore.save_blocked) or the
// underlying write/rename failed.
bool hf_app_save(HabitFlowApp* app);
bool hf_app_add_habit(HabitFlowApp* app, const Habit* habit);
bool hf_app_replace_habit(HabitFlowApp* app, size_t index, const Habit* habit);

// Tells the user their last change wasn't written, via the same notice screen used for a
// failed load, instead of the UI silently moving on as if it had saved.
void hf_app_show_save_failed(HabitFlowApp* app);

void hf_dialog_rebind(HabitFlowApp* app);
void hf_build_credits(HabitFlowApp* app);
void text_input_ok_cb(void* context);
