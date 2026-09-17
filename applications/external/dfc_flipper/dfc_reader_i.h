#pragma once

#include "dfc_i.h"
#include "dfc_reader.h"

NfcCommand dfc_state_machine(Dfc* dfc, Iso14443_4aPoller* iso14443_4a_poller);
