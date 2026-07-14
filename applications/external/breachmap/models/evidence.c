#include "evidence.h"

void evidence_init(Evidence* evidence, uint16_t id, uint16_t asset_id) {
    furi_check(evidence);
    memset(evidence, 0, sizeof(Evidence));
    evidence->id = id;
    evidence->asset_id = asset_id;
    evidence->type = EvidenceNote;
}
