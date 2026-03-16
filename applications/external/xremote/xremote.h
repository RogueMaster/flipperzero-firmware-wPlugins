/*!
 *  @file flipper-xremote/xremote.h
    @license This project is released under the GNU GPLv3 License
 *  @copyright (c) 2023 Sandro Kalatozishvili (s.kalatoz@gmail.com)
 *
 * @brief Entrypoint and factory of the XRemote main app.
 */

#include "xremote_app.h"

#define XREMOTE_VERSION_MAJ  1
#define XREMOTE_VERSION_MIN  4
#define XREMOTE_BUILD_NUMBER 1

/* Returns FAP_VERSION + XREMOTE_BUILD_NUMBER */
void xremote_get_version(char* version, size_t length);

#define INFRARED_SETTINGS_PATH    EXT_PATH("infrared/.infrared.settings")
#define INFRARED_SETTINGS_VERSION (1)
#define INFRARED_SETTINGS_MAGIC   (0x1F)

typedef struct {
    FuriHalInfraredTxPin tx_pin;
    bool otg_enabled;
} InfraredSettings;
