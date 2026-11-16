#pragma once

#include <furi.h>

#include "../core/link.h"
#include "../core/transport.h"

Transport* transport_hid_alloc(FuriMessageQueue* link_queue, Link link);

void transport_hid_free(Transport* transport);

void transport_hid_set_link(Transport* transport, Link link);
