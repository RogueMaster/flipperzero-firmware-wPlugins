#pragma once

/*
 * Shared constants and small types used across RF Studio modules.
 * Kept free of module dependencies so any module can include it.
 */

#include <furi.h>
#include <storage/storage.h>

#define RFS_TAG "RfStudio"

/* Text limits (characters, excluding the terminator). */
#define RFS_NAME_LEN     32
#define RFS_ID_LEN       12 /* buffer size: prefix + 8 hex digits + terminator, with room */
#define RFS_PRESET_LEN   24
#define RFS_PROTOCOL_LEN 24

/* Library locations (Specification Section 12.2). */
#define RFS_DATA_DIR      EXT_PATH("apps_data/rf_studio")
#define RFS_CONFIG_DIR    RFS_DATA_DIR "/config"
#define RFS_REMOTES_DIR   RFS_DATA_DIR "/remotes"
#define RFS_SIGNALS_DIR   RFS_DATA_DIR "/signals"
#define RFS_BACKUPS_DIR   RFS_DATA_DIR "/backups"
#define RFS_EXPORTS_DIR   RFS_DATA_DIR "/exports"
#define RFS_LOGS_DIR      RFS_DATA_DIR "/logs"
#define RFS_TMP_DIR       RFS_DATA_DIR "/tmp"
#define RFS_SETTINGS_PATH RFS_CONFIG_DIR "/settings.conf"
#define RFS_INDEX_PATH    RFS_SIGNALS_DIR "/signals.index"
#define RFS_LOG_PATH      RFS_LOGS_DIR "/rf_studio.log"

/* Exported signals land in the firmware's own Sub-GHz folder so the stock app can use them. */
#define RFS_SUBGHZ_EXPORT_DIR EXT_PATH("subghz/RF Studio")

#define RFS_REMOTE_EXT ".remote"
#define RFS_SIGNAL_EXT ".sub"

/* Firmware Sub-GHz settings file holding frequency and preset lists. */
#define RFS_SUBGHZ_SETTING_PATH EXT_PATH("subghz/assets/setting_user")

/* Minimum free heap required before growing a remote or the signal index. */
#define RFS_MIN_FREE_HEAP (6 * 1024)

typedef enum {
    RfsRadioModeAuto,
    RfsRadioModeInternal,
    RfsRadioModeExternal,
    RfsRadioModeCount,
} RfsRadioMode;

typedef enum {
    RfsSortName,
    RfsSortCategory,
    RfsSortLastUsed,
    RfsSortCount,
} RfsSort;

typedef enum {
    RfsDiscoverCommon, /* firmware hopper list */
    RfsDiscoverAll, /* every frequency in the firmware settings */
    RfsDiscoverCount,
} RfsDiscoverScope;

/* How a signal was recognised (Specification Section 8.3). */
typedef enum {
    RfsKindUnknown, /* protocol not known to this firmware */
    RfsKindStatic, /* decoded fixed code */
    RfsKindDynamic, /* rolling or encrypted code */
    RfsKindRaw, /* raw timing recording */
    RfsKindBinRaw, /* firmware BinRAW recording */
    RfsKindCount,
} RfsSignalKind;

typedef enum {
    RfsSourceCapture,
    RfsSourceImport,
    RfsSourceCount,
} RfsSignalSource;

const char* rfs_radio_mode_name(RfsRadioMode mode);
const char* rfs_sort_name(RfsSort sort);
const char* rfs_discover_scope_name(RfsDiscoverScope scope);
