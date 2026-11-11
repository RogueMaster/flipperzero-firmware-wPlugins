#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include "u2f.h"

typedef struct U2fNfc U2fNfc;

U2fNfc* u2f_nfc_start(U2fData* u2f_inst);

void u2f_nfc_stop(U2fNfc* u2f_nfc);

#ifdef __cplusplus
}
#endif
