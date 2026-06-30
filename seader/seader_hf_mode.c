#include "seader_i.h"

// The first RAM-focused step keeps HF mode state small and scratch-backed.
// Additional HF-specific session state can move here later without changing host ownership.
