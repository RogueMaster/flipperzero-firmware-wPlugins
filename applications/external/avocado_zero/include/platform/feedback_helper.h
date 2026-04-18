/**
 * Infrastructure: LED and vibration feedback.
 */
#pragma once
#include <furi.h>
#include <notification/notification.h>

typedef struct {
    NotificationApp *notifications;
} AvocadoFeedback;

void avocado_feedback_init(AvocadoFeedback *feedback);
void avocado_feedback_deinit(AvocadoFeedback *feedback);
void avocado_feedback_play(AvocadoFeedback *feedback, bool is_critical);
