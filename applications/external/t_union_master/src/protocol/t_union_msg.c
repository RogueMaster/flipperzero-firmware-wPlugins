#include "t_union_msg.h"
#include <furi.h>

TUnionMessage* t_union_msg_alloc() {
    TUnionMessage* msg = malloc(sizeof(TUnionMessage));

    return msg;
}

void t_union_msg_free(TUnionMessage* msg) {
    t_union_msg_reset(msg);

    free(msg);
}

void t_union_msg_reset(TUnionMessage* msg) {
    furi_assert(msg);

    msg->app_version = 0;
    msg->type = TunionCardTypeNone;
    msg->balance = 0;
    msg->transaction_cnt = 0;
    msg->travel_cnt = 0;
}
