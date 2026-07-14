#include "../app.h"
#include "../csv/csv.h"
#include "../structs.h"
#include <storage/storage.h>

#define TAG "tracker_app"

void storage_init(App* app) {
    if(!app) {
        FURI_LOG_E(TAG, "App is NULL");
        return;
    }

    // Open storage
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Allocate file
    app->file = storage_file_alloc(storage);

    // Check if the file exists
    if(storage_file_exists(storage, APP_DATA_PATH("data.csv"))) {
        FURI_LOG_I(TAG, "File exists, reading tasks from file");

        if(!read_tasks_from_csv(app)) {
            FURI_LOG_E(TAG, "Failed to read tasks from CSV file");
        }
    }
}
