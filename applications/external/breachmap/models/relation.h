#pragma once

#include "recon_types.h"

/* A directed edge between two assets. */
typedef struct {
    uint16_t id;
    uint16_t from_id;
    uint16_t to_id;
    RelationType type;
} Relation;

void relation_init(Relation* relation, uint16_t id, uint16_t from_id, uint16_t to_id);
