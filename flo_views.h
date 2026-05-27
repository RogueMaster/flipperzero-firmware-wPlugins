#pragma once

#include "flo_app.h"

View* flo_status_view_alloc(FloData* data);
View* flo_calendar_view_alloc(FloData* data);
View* flo_log_view_alloc(FloData* data, ViewDispatcher* view_dispatcher);
