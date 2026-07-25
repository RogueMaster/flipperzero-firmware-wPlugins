#include "mf_passive_core.h"

bool mf_passive_settings_enter(MfPassiveState* state, const MfPassiveEnterArgs* args) {
    (void)state;
    (void)args;
    return false;
}

void mf_passive_settings_leave(MfPassiveState* state) {
    (void)state;
}
