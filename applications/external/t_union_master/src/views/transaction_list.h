#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../protocol/t_union_msg.h"
#include "../utils/t_union_msgext.h"
#include "../t_union_custom_events.h"

typedef struct TransactionListView TransactionListView;
typedef void (*TransactionListViewCallback)(TUM_CustomEvent event, void* context);

TransactionListView* transaction_list_alloc(TUnionMessage* message, TUnionMessageExt* message_ext);
void transaction_list_free(TransactionListView* instance);
View* transaction_list_get_view(TransactionListView* instance);
void transaction_list_set_callback(
    TransactionListView* instance,
    TransactionListViewCallback cb,
    void* ctx);
void transaction_list_set_index(TransactionListView* instance, size_t index);
uint8_t transaction_list_get_index(TransactionListView* instance);
void transaction_list_reset(TransactionListView* instance);
