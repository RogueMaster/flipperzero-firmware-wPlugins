#include "asset.h"

void asset_init(Asset* asset, uint16_t id) {
    furi_check(asset);
    memset(asset, 0, sizeof(Asset));
    asset->id = id;
    asset->type = AssetTypeUnknown;
    asset->risk = 0;
    snprintf(asset->name, RECON_NAME_LEN, "Asset %u", (unsigned)id);
}
