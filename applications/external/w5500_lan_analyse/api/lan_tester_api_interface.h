#pragma once

#include <flipper_application/api_hashtable/api_hashtable.h>

/*
 * Resolver interface exposing the LAN Tester app's private symbols
 * (app helpers + W5500/ioLibrary) to embedded .fal plugins.
 * Implementation is in lan_tester_api_table.cpp.
 */
extern const ElfApiInterface* const lan_tester_api_interface;
