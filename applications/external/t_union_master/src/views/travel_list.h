#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../protocol/t_union_msg.h"
#include "../utils/t_union_msgext.h"
#include "../t_union_custom_events.h"

typedef struct TravelListView TravelListView;
typedef void (*TravelListViewCallback)(TUM_CustomEvent event, void* context);

TravelListView* travel_list_alloc(TUnionMessage* message, TUnionMessageExt* message_ext);
void travel_list_free(TravelListView* instance);
View* travel_list_get_view(TravelListView* instance);
void travel_list_set_callback(TravelListView* instance, TravelListViewCallback cb, void* ctx);
void travel_list_set_index(TravelListView* instance, size_t index);
uint8_t travel_list_get_index(TravelListView* instance);
void travel_list_reset(TravelListView* instance);
