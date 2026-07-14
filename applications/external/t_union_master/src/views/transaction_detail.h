#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../protocol/t_union_msg.h"
#include "../utils/t_union_msgext.h"
#include "../t_union_custom_events.h"

typedef struct TransactionDetailView TransactionDetailView;
typedef void (*TransactionDetailViewCallback)(TUM_CustomEvent event, void* context);

TransactionDetailView*
    transaction_detail_alloc(TUnionMessage* message, TUnionMessageExt* message_ext);
void transaction_detail_free(TransactionDetailView* instance);
View* transaction_detail_get_view(TransactionDetailView* instance);
void transaction_detail_set_callback(
    TransactionDetailView* instance,
    TransactionDetailViewCallback cb,
    void* ctx);
void transaction_detail_set_index(TransactionDetailView* instance, size_t index);
uint8_t transaction_detail_get_index(TransactionDetailView* instance);
void transaction_detail_reset(TransactionDetailView* instance);
