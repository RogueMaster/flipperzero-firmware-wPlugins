#pragma once

#include <gui/view.h>
#include "settings.h"

typedef struct QuizWidget QuizWidget;
typedef void (*QuizWidgetNavigationCallback)(void* context);
typedef void (*QuizStartCallback)(void* context);

QuizWidget* quiz_widget_alloc();
void quiz_widget_free(QuizWidget* widget);
View* quiz_widget_get_view(QuizWidget* widget);
View* quiz_widget_get_settings_view(QuizWidget* widget);
void quiz_widget_set_settings(QuizWidget* widget, const StrataHeroSettings* settings);
void quiz_widget_set_start_callback(QuizWidget* widget, QuizStartCallback callback, void* context);
void quiz_widget_set_navigation_callback(
    QuizWidget* widget,
    QuizWidgetNavigationCallback callback,
    void* context
);

int quiz_widget_get_score(QuizWidget* widget);
