#pragma once

#include "include/ports/hf_store_port.h"
#include <storage/storage.h>

// Wraps an already-open Storage record as an HfStorePort. `storage` must outlive the port.
HfStorePort hf_storage_port(Storage* storage);
