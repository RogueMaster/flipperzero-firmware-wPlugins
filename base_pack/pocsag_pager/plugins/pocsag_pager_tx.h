#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PCSGTxView PCSGTxView;
typedef void (*PCSGTxViewCallback)(uint32_t event, void* context);

typedef enum {
    PCSGTxViewEventBack = 1,
    PCSGTxViewEventResend = 2,
} PCSGTxViewEvent;

typedef enum {
    PCSGTxViewStateSending,
    PCSGTxViewStateDone,
    PCSGTxViewStateBlocked,
    PCSGTxViewStateError,
} PCSGTxViewState;

PCSGTxView* pcsg_view_tx_alloc(void);
void pcsg_view_tx_free(PCSGTxView* instance);
View* pcsg_view_tx_get_view(PCSGTxView* instance);
void pcsg_view_tx_set_callback(PCSGTxView* instance, PCSGTxViewCallback callback, void* context);

void pcsg_view_tx_set_info(
    PCSGTxView* instance,
    uint32_t frequency,
    uint32_t ric,
    uint32_t baud,
    const char* type_name,
    bool inverted,
    bool is_external);

void pcsg_view_tx_set_state(
    PCSGTxView* instance,
    PCSGTxViewState state,
    float progress,
    const char* status_text);

#ifdef __cplusplus
}
#endif
