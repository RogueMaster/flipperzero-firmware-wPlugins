#include "include/domain/avocado_store_status.h"

bool avocado_store_autosave_allowed(AvocadoStoreLoadStatus status) {
    return status == AvocadoStoreLoadOk;
}

int avocado_store_backup_slot(const bool taken[AVOCADO_STORE_BACKUP_SLOTS]) {
    for(int i = 0; i < AVOCADO_STORE_BACKUP_SLOTS; i++) {
        if(!taken[i]) {
            return i;
        }
    }
    return -1;
}
