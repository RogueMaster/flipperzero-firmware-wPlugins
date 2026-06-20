#pragma once

#include "psa_bf_types.h"
#include "psa_crypto_bf.h"
#include <flipper_format/flipper_format.h>

bool psa_bf_state_from_flipper_format(PsaBfState* state, FlipperFormat* ff);
