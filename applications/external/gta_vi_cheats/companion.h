#pragma once
#include <gui/view_dispatcher.h>
#include "companion_catalog.h"
#include "dispatch_catalog.h"
#include "exploration_catalog.h"

typedef struct GtaCompanion GtaCompanion;
GtaCompanion*
    gta_companion_alloc(ViewDispatcher* dispatcher, uint32_t list_view_id, uint32_t detail_view_id);
void gta_companion_free(GtaCompanion* companion);
/* Called after switching away from collection views. Progress remains resident. */
void gta_companion_idle(GtaCompanion* companion);
void gta_companion_open_standouts(GtaCompanion* companion, size_t category);
void gta_companion_open_list(GtaCompanion* companion, GtaCollectionId collection);
void gta_companion_open_exploration(GtaCompanion* companion, const GtaExploreSection* section);
void gta_companion_open_exploration_item(
    GtaCompanion* companion,
    const GtaExploreSection* section,
    uint16_t id);
void gta_companion_open_money_glitch(GtaCompanion* companion);
void gta_companion_open_dispatch(GtaCompanion* companion, GtaDispatchRegion region);
bool gta_companion_custom_event(GtaCompanion* companion, uint32_t event);
bool gta_companion_back(GtaCompanion* companion);
bool gta_companion_progress_summary(
    const GtaCompanion* companion,
    size_t* total_items,
    size_t* completed_items);
