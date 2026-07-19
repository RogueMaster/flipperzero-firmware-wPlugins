#pragma once

// Persist TridentSettings across runs. Both calls are best-effort: a missing,
// corrupt or version-mismatched file simply leaves the passed-in defaults.

struct TridentSettings; // forward decl (defined in trident_i.h)

void trident_settings_load(struct TridentSettings* settings);
void trident_settings_save(const struct TridentSettings* settings);
