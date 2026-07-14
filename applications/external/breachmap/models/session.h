#pragma once

#include "asset.h"
#include "evidence.h"
#include "relation.h"

/* The whole working set of an engagement. Allocated exactly once; all records
 * are stored inline in fixed arrays, so there is no per-record allocation. */
typedef struct {
    char name[RECON_NAME_LEN];
    char client[RECON_NAME_LEN];
    char location[RECON_NAME_LEN];
    uint32_t created;
    uint32_t modified;
    uint16_t next_id; /* monotonic id generator */
    uint16_t asset_count;
    uint16_t evidence_count;
    uint16_t relation_count;
    bool dirty; /* in-memory: unsaved changes since last save/load */
    Asset assets[RECON_MAX_ASSETS];
    Evidence evidence[RECON_MAX_EVIDENCE];
    Relation relations[RECON_MAX_RELATIONS];
} Session;

Session* session_alloc(void);
void session_free(Session* session);

/* Reset to an empty engagement with the given name. */
void session_reset(Session* session, const char* name);

/* Allocate the next stable id. */
uint16_t session_next_id(Session* session);

/* Refresh the modified timestamp from the RTC and mark the session dirty. */
void session_touch(Session* session);

/* Clear the dirty flag after a successful save or load. */
void session_mark_clean(Session* session);
