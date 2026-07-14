#pragma once

#include "recon_types.h"

typedef struct {
    uint16_t id;
    uint16_t asset_id; /* owning asset */
    EvidenceType type;
    char label[RECON_NAME_LEN];
    char path[RECON_PATH_LEN]; /* file on SD, empty for plain notes */
    char info[RECON_NAME_LEN]; /* metadata extracted from the linked file */
    uint32_t created;
} Evidence;

void evidence_init(Evidence* evidence, uint16_t id, uint16_t asset_id);
