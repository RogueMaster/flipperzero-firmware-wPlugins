#include "protocol_items.h"

const SubGhzProtocol* tpms_protocol_registry_items[] = {
    &tpms_protocol_schrader_gg4,
    &tpms_protocol_schrader_smd3ma4,
    &tpms_protocol_schrader_eg53ma4,
    &tpms_protocol_abarth_124,
};

const SubGhzProtocolRegistry tpms_protocol_registry = {
    .items = tpms_protocol_registry_items,
    .size = COUNT_OF(tpms_protocol_registry_items)};
