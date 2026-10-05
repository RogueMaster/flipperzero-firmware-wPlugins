#pragma once

#include <input/input.h>

#include "app_data.h"

// Hidden debug feature, gated to one specific device by its user-set name so it can never
// activate by accident on anyone else's Flipper.
bool is_debug_device(void);

// True once the secret button combo has been entered this session, on the debug device. Debug
// features stay unreachable - even on the debug device - until this unlocks.
bool is_debug_mode_active(void);

// Feed one button press (while a shift is active) into the combo tracker. Returns true exactly
// once: the moment the combo completes correctly, and only on the debug device. Does nothing
// (and always returns false) on any other device.
bool debug_feed_combo_key(InputKey key);

// Debug-only: slide the whole recorded shift (its start and every logged break) by `minutes` as
// one solid block - positive fast-forwards it, negative rewinds it - preserving every segment's
// own duration exactly. While running, that time also counts as worked (moving the prediction
// with it); while paused, it only ages the current break, leaving the prediction untouched, since
// break time was never worked time. Bounded so it can never rewind past "now" or fast-forward far
// enough to wrap around a full day. Only reachable via is_debug_mode_active().
void debug_time_travel(AppData* app, int32_t minutes);
