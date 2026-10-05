#pragma once
#include "dnd_storage.h"
/* Existing Inventory only: deferred until normal equipment creates the sidecar. */
bool dnd_extra_items_grant(Storage* storage, uint32_t profile, const DndCharacter* owner);
