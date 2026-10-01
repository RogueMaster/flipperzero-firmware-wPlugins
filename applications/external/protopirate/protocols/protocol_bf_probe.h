#pragma once

#include <lib/flipper_format/flipper_format.h>
#include <stdbool.h>

// Whether a capture still needs a bruteforce run to be solved.
//
// These are pure FlipperFormat field reads, deliberately kept out of the bruteforce plugin so a
// caller can ask the question without mapping that plugin's .fal into the heap.
bool protopirate_bf_probe_psa_needs_bruteforce(FlipperFormat* ff);
bool protopirate_bf_probe_hitag2_needs_bruteforce(FlipperFormat* ff);
bool protopirate_bf_probe_needs_bruteforce(FlipperFormat* ff);
