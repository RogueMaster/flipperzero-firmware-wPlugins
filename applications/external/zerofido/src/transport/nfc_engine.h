/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <nfc/protocols/nfc_generic_event.h>

NfcCommand zf_transport_nfc_event_callback(NfcGenericEvent event, void *context);
