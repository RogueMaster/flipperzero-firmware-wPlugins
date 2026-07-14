#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../t_union_custom_events.h"
#include "../protocol/t_union_msg.h"
#include "../utils/t_union_msgext.h"

typedef struct CardInfoView CardInfoView;
typedef void (*CardInfoViewCallback)(TUM_CustomEvent event, void* context);

CardInfoView* card_info_alloc(TUnionMessage* message, TUnionMessageExt* message_ext);
void card_info_free(CardInfoView* instance);
View* card_info_get_view(CardInfoView* instance);
void card_info_set_callback(CardInfoView* instance, CardInfoViewCallback cb, void* ctx);
