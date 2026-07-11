#pragma once

#include "../models/session.h"

/* Add a directed relation between two assets (by id). Returns the relation
 * index, or RECON_INVALID_INDEX if the store is full or the ids are invalid. */
uint16_t graph_add_relation(Session* session, uint16_t from_id, uint16_t to_id, RelationType type);

/* Remove the relation at the given index. */
bool graph_delete_relation(Session* session, uint16_t index);

/* Fill "out_ids" with the ids of assets reachable by one outgoing edge from
 * "asset_id". Returns the neighbour count (capped at "cap"). */
uint16_t
    graph_neighbors_out(const Session* session, uint16_t asset_id, uint16_t* out_ids, uint16_t cap);

/* Total number of edges touching an asset (in + out). */
uint16_t graph_degree(const Session* session, uint16_t asset_id);

/* Compute a propagated risk value for every asset into "out_risk" (indexed like
 * session->assets). Risk flows along control/access edges with a fixed decay, so
 * a vulnerable reader raises the effective risk of the door it controls.
 * Does not mutate stored asset risk. */
void graph_propagate_risk(const Session* session, uint8_t* out_risk);
