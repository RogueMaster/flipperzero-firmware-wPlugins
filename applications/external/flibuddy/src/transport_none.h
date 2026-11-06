/* A transport that never opens: no host can connect and nothing is sent.
 * The app runs it while the chosen backend is not available. One shared
 * instance; it needs no free.
 */
#pragma once

#include "transport.h"

Transport* transport_none(void);
