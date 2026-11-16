#pragma once

#include <furi.h>

typedef struct Battery Battery;

Battery* battery_alloc(FuriMessageQueue* charge_queue, FuriMessageQueue* charging_queue);

void battery_free(Battery* battery);
