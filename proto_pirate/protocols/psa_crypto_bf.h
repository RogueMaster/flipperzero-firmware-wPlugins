#pragma once

#include "psa_bf_types.h"

void psa_brute_force_run(PsaBfState* state);
int32_t psa_brute_force_thread_entry(void* arg);
