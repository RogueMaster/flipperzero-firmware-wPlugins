#include "include/domain/hf_store_status.h"

bool hf_store_autosave_allowed(HfStoreLoadStatus status) {
    return status == HfStoreLoadOk;
}

int hf_store_backup_slot(const bool taken[HF_STORE_BACKUP_SLOTS]) {
    for(int i = 0; i < HF_STORE_BACKUP_SLOTS; i++) {
        if(!taken[i]) {
            return i;
        }
    }
    return -1;
}
