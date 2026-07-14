#include "../subghz_i.h" // IWYU pragma: keep
#include "../views/subghz_frequency_analyzer.h"

#define TAG "SubGhzSceneFrequencyAnalyzer"

void subghz_scene_frequency_analyzer_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

void subghz_scene_frequency_analyzer_on_enter(void* context) {
    SubGhz* subghz = context;
    subghz_frequency_analyzer_set_callback(
        subghz->subghz_frequency_analyzer, subghz_scene_frequency_analyzer_callback, subghz);
    subghz_frequency_analyzer_feedback_level(
        subghz->subghz_frequency_analyzer,
        subghz->last_settings->frequency_analyzer_feedback_level,
        true);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdFrequencyAnalyzer);
}

bool subghz_scene_frequency_analyzer_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventSceneAnalyzerLock) {
            notification_message(subghz->notifications, &sequence_set_green_255);
            switch(subghz_frequency_analyzer_feedback_level(
                subghz->subghz_frequency_analyzer,
                SubGHzFrequencyAnalyzerFeedbackLevelAll,
                false)) {
            case SubGHzFrequencyAnalyzerFeedbackLevelAll:
                notification_message(subghz->notifications, &sequence_success);
                break;
            case SubGHzFrequencyAnalyzerFeedbackLevelVibro:
                notification_message(subghz->notifications, &sequence_single_vibro);
                break;
            case SubGHzFrequencyAnalyzerFeedbackLevelMute:
                break;
            }
            notification_message(subghz->notifications, &sequence_display_backlight_on);
            return true;
        } else if(event.event == SubGhzCustomEventSceneAnalyzerUnlock) {
            notification_message(subghz->notifications, &sequence_reset_rgb);
            return true;
        } else if(event.event == SubGhzCustomEventViewFreqAnalOkShort) {
            uint32_t frequency =
                subghz_frequency_analyzer_get_frequency_to_save(subghz->subghz_frequency_analyzer);
            if(frequency > 0) {
                FURI_LOG_I(TAG, "Frequency selected: %ld", frequency);
            }

            return true;
        }
    }
    return false;
}

void subghz_scene_frequency_analyzer_on_exit(void* context) {
    SubGhz* subghz = context;
    notification_message(subghz->notifications, &sequence_reset_rgb);
}
