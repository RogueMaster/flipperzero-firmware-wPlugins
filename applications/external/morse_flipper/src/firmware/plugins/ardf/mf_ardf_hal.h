#pragma once

#include "mf_ardf_core.h"

const MfArdfHardwareOps* mf_ardf_hal_ops(void);
void mf_ardf_hal_init(void);
void mf_ardf_hal_rtc_sample(MfArdfState* state);
void mf_ardf_hal_deinit(void);
