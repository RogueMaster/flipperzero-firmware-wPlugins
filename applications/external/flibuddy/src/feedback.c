#include "feedback.h"

FeedbackPlan feedback_plan(AppCue cue, const AppSettings* settings) {
    FeedbackPlan p = {FeedbackLedNone, FeedbackVibroNone, FeedbackSoundNone};
    switch(cue) {
    case AppCueConnected:
    case AppCueRead:
        p = (FeedbackPlan){FeedbackLedBlue, FeedbackVibroShort, FeedbackSoundNone};
        break;
    case AppCueSuccess:
        p = (FeedbackPlan){FeedbackLedGreen, FeedbackVibroShort, FeedbackSoundSuccess};
        break;
    case AppCueError:
        p = (FeedbackPlan){FeedbackLedRed, FeedbackVibroLong, FeedbackSoundError};
        break;
    case AppCueNone:
        break;
    }
    switch(app_settings_feedback(settings)) {
    case AppFeedbackOff:
        p.led = FeedbackLedNone;
        /* fall through */
    case AppFeedbackLed:
        p.vibro = FeedbackVibroNone;
        break;
    case AppFeedbackVibroLed:
        break;
    }
    if(!app_settings_sound(settings)) p.sound = FeedbackSoundNone;
    return p;
}

bool feedback_plan_empty(FeedbackPlan p) {
    return p.led == FeedbackLedNone && p.vibro == FeedbackVibroNone &&
           p.sound == FeedbackSoundNone;
}
