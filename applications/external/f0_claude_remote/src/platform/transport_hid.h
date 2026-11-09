#pragma once

#include <furi.h>

#include "../core/transport.h"

Transport* transport_hid_alloc(FuriMessageQueue* link_queue);

void transport_hid_free(Transport* transport);
