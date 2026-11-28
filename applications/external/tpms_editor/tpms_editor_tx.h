#pragma once

#include <stdbool.h>
#include "tpms_encoder.h"

typedef struct TPMSApp TPMSApp;

bool tpms_editor_send(TPMSApp* app, const TpmsEditValues* values);
