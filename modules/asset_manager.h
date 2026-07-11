#pragma once

#include "../models/session.h"

/* Create a new asset in the session. Returns its index, or RECON_INVALID_INDEX
 * if the session is full. */
uint16_t asset_manager_add(Session* session);

/* Look up an asset index by its stable id. Returns RECON_INVALID_INDEX if none. */
uint16_t asset_manager_index_by_id(const Session* session, uint16_t id);

/* Remove the asset at the given index together with its evidence and relations. */
bool asset_manager_delete(Session* session, uint16_t index);

/* Attach a piece of evidence to an asset. Returns the evidence index or
 * RECON_INVALID_INDEX when the evidence store is full. */
uint16_t asset_manager_add_evidence(
    Session* session,
    uint16_t asset_id,
    EvidenceType type,
    const char* label,
    const char* path);

/* Count evidence items belonging to an asset. */
uint16_t asset_manager_evidence_count(const Session* session, uint16_t asset_id);
