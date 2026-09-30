#include "include/app/avocado_session.h"
#include "include/domain/avocado_rules.h"
#include "include/domain/avocado_store_status.h"
#include "include/platform/storage_helper.h"

void avocado_session_bootstrap(AvocadoData* data, uint32_t now_ts) {
    AvocadoStoreLoadStatus status = avocado_data_load(data);
    /* Saves from before OK reset max roots: heal so play can continue without game over. */
    if(data->roots_length >= AVOCADO_ROOTS_MAX && data->victory_seen != 0) {
        data->roots_length = 0;
        data->dirty_level = 0;
        data->last_timestamp = now_ts;
    }
    avocado_rules_apply_elapsed_days(data, now_ts);
    // A Missing or Corrupt load must not be written back at launch, before any user action.
    if(avocado_store_autosave_allowed(status)) {
        avocado_data_save(data);
    }
}
