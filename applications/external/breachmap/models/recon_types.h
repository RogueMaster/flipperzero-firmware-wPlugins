#pragma once

#include <furi.h>
#include <furi_hal_rtc.h>

/* Fixed capacities. All records live inside a single Session allocation,
 * so there is no per-record dynamic memory. */
#define RECON_MAX_ASSETS    48
#define RECON_MAX_EVIDENCE  96
#define RECON_MAX_RELATIONS 64

#define RECON_NAME_LEN  32
#define RECON_NOTE_LEN  96
#define RECON_PATH_LEN  128
#define RECON_LABEL_LEN 24

#define RECON_INVALID_INDEX 0xFFFF

/* Max capture files listed in the quick-import picker. */
#define RECON_MAX_IMPORT 24

/* Sentinel graph coordinate meaning "auto-place this node". */
#define RECON_COORD_AUTO 0xFF

/* On-disk locations (APP_DATA_PATH expands to /ext/apps_data/breach_map). */
#define RECON_SESSION_DIR APP_DATA_PATH("sessions")
#define RECON_EXPORT_DIR  APP_DATA_PATH("exports")
#define RECON_SESSION_EXT ".recon"

typedef enum {
    AssetTypeDoor,
    AssetTypeReader,
    AssetTypeCamera,
    AssetTypeBle,
    AssetTypeRf,
    AssetTypeUnknown,
    AssetTypeCount,
} AssetType;

typedef enum {
    EvidenceNote,
    EvidenceRf,
    EvidenceNfc,
    EvidenceBle,
    EvidenceTypeCount,
} EvidenceType;

/* Directed relationships. Example chain: Badge -> Reader -> Door -> Camera. */
typedef enum {
    RelReadsBadge,
    RelControls,
    RelObserves,
    RelConnectsTo,
    RelNearby,
    RelTypeCount,
} RelationType;

typedef enum {
    SeverityInfo,
    SeverityLow,
    SeverityMedium,
    SeverityHigh,
    SeverityCritical,
    SeverityCount,
} Severity;

const char* asset_type_name(AssetType type);
const char* evidence_type_name(EvidenceType type);
const char* relation_type_name(RelationType type);
const char* severity_name(Severity severity);
