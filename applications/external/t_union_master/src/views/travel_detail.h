#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../protocol/t_union_msg.h"
#include "../t_union_custom_events.h"

#include "../utils/t_union_msgext.h"
#include "../utils/logo.h"

typedef struct TravelDetailView TravelDetailView;
typedef void (*TravelDetailViewCallback)(TUM_CustomEvent event, void* context);

TravelDetailView* travel_detail_alloc(TUnionMessage* message, TUnionMessageExt* message_ext);
void travel_detail_free(TravelDetailView* instance);
View* travel_detail_get_view(TravelDetailView* instance);
void travel_detail_set_callback(TravelDetailView* instance, TravelDetailViewCallback cb, void* ctx);
void travel_detail_set_index(TravelDetailView* instance, size_t index);
uint8_t travel_detail_get_index(TravelDetailView* instance);
void travel_detail_reset(TravelDetailView* instance);
