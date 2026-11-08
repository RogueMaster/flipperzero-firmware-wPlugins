#pragma once

#include <furi.h>

#include "../core/transport.h"

Transport* transport_bluetooth_alloc(FuriMessageQueue* link_queue);

void transport_bluetooth_free(Transport* transport);
