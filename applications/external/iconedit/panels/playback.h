#pragma once

#include <strings.h> // for size_t
#include "../icon.h"

void playback_start(IEIcon* icon);
void playback_set_update_callback(IEIconAnimationCallback callback, void* context);
