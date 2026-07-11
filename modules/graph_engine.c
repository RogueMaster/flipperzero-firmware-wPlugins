#include "graph_engine.h"

/* Risk lost per hop when propagating along an access/control edge. */
#define RECON_RISK_DECAY 15

uint16_t
    graph_add_relation(Session* session, uint16_t from_id, uint16_t to_id, RelationType type) {
    furi_check(session);
    if(session->relation_count >= RECON_MAX_RELATIONS) return RECON_INVALID_INDEX;
    if(from_id == to_id) return RECON_INVALID_INDEX;

    /* both endpoints must exist */
    bool from_ok = false, to_ok = false;
    for(uint16_t i = 0; i < session->asset_count; i++) {
        if(session->assets[i].id == from_id) from_ok = true;
        if(session->assets[i].id == to_id) to_ok = true;
    }
    if(!from_ok || !to_ok) return RECON_INVALID_INDEX;

    uint16_t index = session->relation_count;
    Relation* rel = &session->relations[index];
    relation_init(rel, session_next_id(session), from_id, to_id);
    rel->type = type;
    session->relation_count++;
    session_touch(session);
    return index;
}

bool graph_delete_relation(Session* session, uint16_t index) {
    furi_check(session);
    if(index >= session->relation_count) return false;
    for(uint16_t i = index; i + 1 < session->relation_count; i++) {
        session->relations[i] = session->relations[i + 1];
    }
    session->relation_count--;
    session_touch(session);
    return true;
}

uint16_t graph_neighbors_out(
    const Session* session,
    uint16_t asset_id,
    uint16_t* out_ids,
    uint16_t cap) {
    furi_check(session);
    uint16_t count = 0;
    for(uint16_t i = 0; i < session->relation_count && count < cap; i++) {
        if(session->relations[i].from_id == asset_id) {
            out_ids[count++] = session->relations[i].to_id;
        }
    }
    return count;
}

uint16_t graph_degree(const Session* session, uint16_t asset_id) {
    furi_check(session);
    uint16_t count = 0;
    for(uint16_t i = 0; i < session->relation_count; i++) {
        if(session->relations[i].from_id == asset_id || session->relations[i].to_id == asset_id) {
            count++;
        }
    }
    return count;
}

/* Only these edge types carry risk downstream (an attacker who owns the source
 * gains leverage over the target). */
static bool edge_carries_risk(RelationType type) {
    return type == RelReadsBadge || type == RelControls || type == RelConnectsTo;
}

void graph_propagate_risk(const Session* session, uint8_t* out_risk) {
    furi_check(session);
    furi_check(out_risk);

    for(uint16_t i = 0; i < session->asset_count; i++) {
        out_risk[i] = session->assets[i].risk;
    }

    /* Relax over all edges up to asset_count passes (longest simple path). */
    for(uint16_t pass = 0; pass < session->asset_count; pass++) {
        bool changed = false;
        for(uint16_t e = 0; e < session->relation_count; e++) {
            const Relation* rel = &session->relations[e];
            if(!edge_carries_risk(rel->type)) continue;

            uint16_t from_idx = RECON_INVALID_INDEX;
            uint16_t to_idx = RECON_INVALID_INDEX;
            for(uint16_t i = 0; i < session->asset_count; i++) {
                if(session->assets[i].id == rel->from_id) from_idx = i;
                if(session->assets[i].id == rel->to_id) to_idx = i;
            }
            if(from_idx == RECON_INVALID_INDEX || to_idx == RECON_INVALID_INDEX) continue;

            uint8_t incoming = (out_risk[from_idx] > RECON_RISK_DECAY) ?
                                   (out_risk[from_idx] - RECON_RISK_DECAY) :
                                   0;
            if(incoming > out_risk[to_idx]) {
                out_risk[to_idx] = incoming;
                changed = true;
            }
        }
        if(!changed) break;
    }
}
