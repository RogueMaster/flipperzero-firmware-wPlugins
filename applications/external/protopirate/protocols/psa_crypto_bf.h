#pragma once

#include "bruteforce_types.h"

void psa_brute_force_run(BruteForceState* state);
int32_t psa_brute_force_thread_entry(void* arg);
