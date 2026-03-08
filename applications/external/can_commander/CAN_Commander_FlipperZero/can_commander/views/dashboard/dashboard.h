#pragma once

#include "../../can_commander.h"

size_t dashboard_model_size(void);
void dashboard_view_draw(Canvas* canvas, void* model);
bool dashboard_view_input(InputEvent* event, void* context);

AppDashboardMode dashboard_mode_for_tool(CcToolId tool_id);
void dashboard_set_mode(App* app, AppDashboardMode mode);
bool dashboard_handle_event(App* app, const CcEvent* event);
