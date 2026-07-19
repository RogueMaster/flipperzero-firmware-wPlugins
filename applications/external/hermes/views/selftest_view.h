#pragma once

#include <gui/view.h>
#include "../helpers/selftest.h"

typedef struct SelfTestView SelfTestView;

/** Fires when the user asks to start (or re-run) the test. */
typedef void (*SelfTestViewCallback)(void* context);

SelfTestView* selftest_view_alloc(void);
void selftest_view_free(SelfTestView* sv);
View* selftest_view_get_view(SelfTestView* sv);

void selftest_view_set_callback(SelfTestView* sv, SelfTestViewCallback cb, void* context);

void selftest_view_reset(SelfTestView* sv);
void selftest_view_set_pins(SelfTestView* sv, uint8_t rx_pin, uint8_t tx_pin);
void selftest_view_set_running(SelfTestView* sv, bool running, uint8_t percent);
void selftest_view_set_result(SelfTestView* sv, const SelfTestResult* result);
