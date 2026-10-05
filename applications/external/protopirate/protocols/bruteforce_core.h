#pragma once

#include "bruteforce_types.h"
#include "psa_crypto_bf.h"
#include <flipper_format/flipper_format.h>

bool bruteforce_state_from_flipper_format(BruteForceState* state, FlipperFormat* ff);
