#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include "views/flipp_pomodoro_timer_view.h"
#include "views/flipp_pomodoro_info_view.h"
#include "views/flipp_pomodoro_config_view.h"

#include "modules/flipp_pomodoro.h"
#include "modules/flipp_pomodoro_statistics.h"
#include "modules/flipp_pomodoro_settings.h"
#include "helpers/notification_manager.h"

typedef enum
{
    // Reserve first 100 events for button types and indexes, starting from 0
    FlippPomodoroAppCustomEventStageSkip = 100,
    FlippPomodoroAppCustomEventStageComplete, // By Expiration
    FlippPomodoroAppCustomEventTimerTick,
    FlippPomodoroAppCustomEventTimerAskHint,
    FlippPomodoroAppCustomEventStateUpdated,
    FlippPomodoroAppCustomEventResumeTimer,
} FlippPomodoroAppCustomEvent;

typedef struct
{
    SceneManager *scene_manager;
    ViewDispatcher *view_dispatcher;
    Gui *gui;
    NotificationApp *notification_app;
    NotificationManager *notification_manager;
    FlippPomodoroTimerView *timer_view;
    FlippPomodoroInfoView *info_view;
    FlippPomodoroConfigView *config_view;
    FlippPomodoroState *state;
    FlippPomodoroStatistics *statistics;
    uint32_t paused_at_timestamp;
    FlippPomodoroSettings settings_before;
} FlippPomodoroApp;

typedef enum
{
    FlippPomodoroAppViewTimer,
    FlippPomodoroAppViewInfo,
    FlippPomodoroAppViewConfig,
} FlippPomodoroAppView;