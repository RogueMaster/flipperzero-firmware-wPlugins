#include "include/platform/feedback_helper.h"
#include <notification/notification_messages.h>

void avocado_feedback_init(AvocadoFeedback *feedback) {
    feedback->notifications = furi_record_open(RECORD_NOTIFICATION);
}

void avocado_feedback_deinit(AvocadoFeedback *feedback) {
    notification_message(feedback->notifications, &sequence_reset_red);
    notification_message(feedback->notifications, &sequence_reset_green);
    notification_message(feedback->notifications, &sequence_reset_blue);
    furi_record_close(RECORD_NOTIFICATION);
}

void avocado_feedback_play(AvocadoFeedback *feedback, bool is_critical) {
    if (is_critical) {
        notification_message(feedback->notifications, &sequence_set_red_255);
        notification_message(feedback->notifications, &sequence_single_vibro);
    } else {
        notification_message(feedback->notifications, &sequence_set_green_255);
        notification_message(feedback->notifications, &sequence_single_vibro);
    }
}
