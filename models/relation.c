#include "relation.h"

void relation_init(Relation* relation, uint16_t id, uint16_t from_id, uint16_t to_id) {
    furi_check(relation);
    memset(relation, 0, sizeof(Relation));
    relation->id = id;
    relation->from_id = from_id;
    relation->to_id = to_id;
    relation->type = RelConnectsTo;
}
