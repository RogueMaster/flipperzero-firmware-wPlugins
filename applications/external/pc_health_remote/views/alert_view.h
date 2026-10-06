#pragma once
#include <gui/view.h>
#include "../advice.h"

typedef struct PhrAlertView PhrAlertView;
typedef void (*PhrAlertCallback)(void* context);

PhrAlertView* phr_alert_view_alloc(void);
void phr_alert_view_free(PhrAlertView* v);
View* phr_alert_view_get_view(PhrAlertView* v);
/** Invoked on OK (acknowledge / snooze). Back is handled by the scene manager. */
void phr_alert_view_set_ok_callback(PhrAlertView* v, PhrAlertCallback cb, void* ctx);
void phr_alert_view_set(
    PhrAlertView* v,
    AlertRuleId rule,
    uint8_t value,
    uint8_t threshold,
    bool critical,
    char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN],
    size_t line_count);
