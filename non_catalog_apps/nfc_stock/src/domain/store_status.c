#include "include/store_status.h"

int store_status_backup_slot(const bool taken[STORE_BACKUP_SLOTS]) {
    for(int i = 0; i < STORE_BACKUP_SLOTS; i++) {
        if(!taken[i]) {
            return i;
        }
    }
    return -1;
}
