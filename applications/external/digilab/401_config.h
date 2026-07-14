/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401_CONFIG_H_
#define _401_CONFIG_H_

#include "cJSON/cJSON.h"
#include <storage/storage.h>
#include <toolbox/path.h>

#define DIGILAB_VERSION                        "1.0"
#define DIGILAB_DEFAULT_SCOPE_SOUND            0
#define DIGILAB_DEFAULT_SCOPE_VIBRO            0
#define DIGILAB_DEFAULT_SCOPE_LED              0
#define DIGILAB_DEFAULT_SCOPE_ALERT            0
#define DIGILAB_DEFAULT_SCOPE_BRIDGEFACTOR     4.39
#define DIGILAB_DEFAULT_SCOPE_BRIDGEFACTOR_MIN 4.00
#define DIGILAB_DEFAULT_SCOPE_BRIDGEFACTOR_MAX 5.00

#include <401_err.h>
#include "app_params.h"

typedef enum {
    DigiLab_ScopeSoundOff = 0,
    DigiLab_ScopeSoundAlert,
    Digilab_ScopeSoundOn,
} DigiLab_ScopeSound;

typedef enum {
    DigiLab_ScopeVibroOff = 0,
    DigiLab_ScopeVibroAlert,
} DigiLab_ScopeVibro;

typedef enum {
    DigiLab_ScopeLedOff = 0,
    DigiLab_ScopeLedAlert,
    DigiLab_ScopeLedFollow,
    DigiLab_ScopeLedVariance,
    DigiLab_ScopeLedTrigger,
} DigiLab_ScopeLed;

typedef enum {
    DigiLab_ScopeAlert_lt3V = 0,
    DigiLab_ScopeAlert_gt3V,
    DigiLab_ScopeAlert_lt5V,
    DigiLab_ScopeAlert_gt5V,
    DigiLab_ScopeAlert_osc,
    DigiLab_ScopeAlert_maxV,
    DigiLab_ScopeAlert_0V,
} DigiLab_ScopeAlert;

typedef struct {
    char* version;
    DigiLab_ScopeSound ScopeSound;
    DigiLab_ScopeVibro ScopeVibro;
    DigiLab_ScopeLed ScopeLed;
    DigiLab_ScopeAlert ScopeAlert;
    double BridgeFactor;
} Configuration;

void debug_config(Configuration* config);
l401_err config_alloc(Configuration** config);
void config_default_init(Configuration* config);
l401_err config_to_json(Configuration* config, char** jsontxt);
l401_err json_to_config(char* jsontxt, Configuration* config);
l401_err config_save_json(const char* filename, Configuration* config);
l401_err config_read_json(const char* filename, Configuration* config);
l401_err config_init_dir(const char* filename);
l401_err config_load_json(const char* filename, Configuration* config);

l401_err config_reset_calibration(void);
l401_err config_set_calibration(void);
l401_err config_check_calibration(void);
#endif /* end of include guard: 401_CONFIG_H_ */
