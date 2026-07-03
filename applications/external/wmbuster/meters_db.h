#pragma once
#include "wmbus_app_i.h"

/* Find or insert a meter row by (M, ID). Returns pointer to row in table.
 * Implements simple LRU eviction when full. Caller must hold app->lock. */
WmbusMeter* meters_db_upsert(WmbusApp* app, uint16_t m, uint32_t id);

/* Reset table. */
void meters_db_clear(WmbusApp* app);
