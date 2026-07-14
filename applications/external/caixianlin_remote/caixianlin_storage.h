#ifndef CAIXIANLIN_STORAGE_H
#define CAIXIANLIN_STORAGE_H

#include "caixianlin_types.h"

// Load Station ID and channel from persistent storage
// Returns true if loaded successfully
bool caixianlin_storage_load(CaixianlinRemoteApp* app);

// Save current Station ID and channel to persistent storage
void caixianlin_storage_save(CaixianlinRemoteApp* app);

#endif // CAIXIANLIN_STORAGE_H
