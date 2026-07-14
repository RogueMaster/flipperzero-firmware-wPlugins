#include "asset_manager.h"

uint16_t asset_manager_add(Session* session) {
    furi_check(session);
    if(session->asset_count >= RECON_MAX_ASSETS) return RECON_INVALID_INDEX;

    uint16_t index = session->asset_count;
    Asset* asset = &session->assets[index];
    asset_init(asset, session_next_id(session));
    asset->created = furi_hal_rtc_get_timestamp();
    asset->modified = asset->created;
    session->asset_count++;
    session_touch(session);
    return index;
}

uint16_t asset_manager_index_by_id(const Session* session, uint16_t id) {
    furi_check(session);
    for(uint16_t i = 0; i < session->asset_count; i++) {
        if(session->assets[i].id == id) return i;
    }
    return RECON_INVALID_INDEX;
}

static void remove_evidence_of_asset(Session* session, uint16_t asset_id) {
    uint16_t w = 0;
    for(uint16_t r = 0; r < session->evidence_count; r++) {
        if(session->evidence[r].asset_id == asset_id) continue;
        if(w != r) session->evidence[w] = session->evidence[r];
        w++;
    }
    session->evidence_count = w;
}

static void remove_relations_of_asset(Session* session, uint16_t asset_id) {
    uint16_t w = 0;
    for(uint16_t r = 0; r < session->relation_count; r++) {
        Relation* rel = &session->relations[r];
        if(rel->from_id == asset_id || rel->to_id == asset_id) continue;
        if(w != r) session->relations[w] = session->relations[r];
        w++;
    }
    session->relation_count = w;
}

bool asset_manager_delete(Session* session, uint16_t index) {
    furi_check(session);
    if(index >= session->asset_count) return false;

    uint16_t asset_id = session->assets[index].id;
    remove_evidence_of_asset(session, asset_id);
    remove_relations_of_asset(session, asset_id);

    for(uint16_t i = index; i + 1 < session->asset_count; i++) {
        session->assets[i] = session->assets[i + 1];
    }
    session->asset_count--;
    session_touch(session);
    return true;
}

uint16_t asset_manager_add_evidence(
    Session* session,
    uint16_t asset_id,
    EvidenceType type,
    const char* label,
    const char* path) {
    furi_check(session);
    if(session->evidence_count >= RECON_MAX_EVIDENCE) return RECON_INVALID_INDEX;

    uint16_t index = session->evidence_count;
    Evidence* evidence = &session->evidence[index];
    evidence_init(evidence, session_next_id(session), asset_id);
    evidence->type = type;
    if(label) strncpy(evidence->label, label, RECON_NAME_LEN - 1);
    if(path) strncpy(evidence->path, path, RECON_PATH_LEN - 1);
    evidence->created = furi_hal_rtc_get_timestamp();
    session->evidence_count++;
    session_touch(session);
    return index;
}

uint16_t asset_manager_evidence_count(const Session* session, uint16_t asset_id) {
    furi_check(session);
    uint16_t count = 0;
    for(uint16_t i = 0; i < session->evidence_count; i++) {
        if(session->evidence[i].asset_id == asset_id) count++;
    }
    return count;
}
