#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>

#include "flo_data.h"

/* View IDs */
typedef enum {
    FloViewStatus,
    FloViewCalendar,
    FloViewLogPeriod,
    FloViewSettings,
    FloViewMenu,
} FloView;

/* Custom event IDs */
typedef enum {
    FloEventMenuStatus,
    FloEventMenuCalendar,
    FloEventMenuLogPeriod,
    FloEventMenuDeleteLast,
    FloEventMenuSettings,
    FloEventLogSave,
    FloEventLogCancel,
} FloEvent;

typedef struct {
    /* Core */
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    /* Views */
    Submenu* submenu;
    View* status_view;
    View* calendar_view;
    View* log_view;
    VariableItemList* settings_list;

    /* Data */
    FloData data;
} FloApp;

/* Status view */
typedef struct {
    FloData* data;
} FloStatusModel;

/* Calendar view */
typedef struct {
    FloData* data;
    uint16_t view_year;
    uint8_t view_month;
    uint8_t cursor_day;
} FloCalendarModel;

/* Log period view */
typedef struct {
    FloData* data;
    ViewDispatcher* view_dispatcher;
    FloDate date;
    uint8_t duration;
    uint8_t field; /* 0=year, 1=month, 2=day, 3=duration, 4=save, 5=cancel */
} FloLogModel;
