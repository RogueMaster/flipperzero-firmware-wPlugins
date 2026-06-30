#include "../../metroflip_i.h"
#include <lib/nfc/protocols/felica/felica.h>
#include "suica_structs_i.h"

static void suica_model_initialize(SuicaHistoryViewModel* model, size_t initial_capacity) {
    model->travel_history =
        (uint8_t*)malloc(initial_capacity * FELICA_DATA_BLOCK_SIZE); // Each entry is 16 bytes
    model->size = 0;
    model->capacity = initial_capacity;
}

static void suica_add_entry(SuicaHistoryViewModel* model, const uint8_t* entry) {
    if(model->size <= 0) {
        model->travel_history =
            (uint8_t*)malloc(20 * FELICA_DATA_BLOCK_SIZE); // Each entry is 16 bytes
        model->size = 0;
        model->capacity = 20;
    }
    // Check if resizing is needed
    if(model->size == model->capacity) {
        size_t new_capacity = model->capacity * 2; // Double the capacity
        uint8_t* new_data =
            (uint8_t*)realloc(model->travel_history, new_capacity * FELICA_DATA_BLOCK_SIZE);
        model->travel_history = new_data;
        model->capacity = new_capacity;
    }

    // Copy the 16-byte entry to the next slot
    for(size_t i = 0; i < FELICA_DATA_BLOCK_SIZE; i++) {
        model->travel_history[(model->size * FELICA_DATA_BLOCK_SIZE) + i] = entry[i];
    }

    model->size++;
}

static void load_suica_data(void* context, FlipperFormat* format, bool is_metroflip_file) {
    Metroflip* app = (Metroflip*)context;
    app->suica_context = malloc(sizeof(SuicaContext));
    app->suica_context->view_history = view_alloc();
    view_set_context(app->suica_context->view_history, app);
    view_allocate_model(
        app->suica_context->view_history, ViewModelTypeLockFree, sizeof(SuicaHistoryViewModel));
    SuicaHistoryViewModel* model = view_get_model(app->suica_context->view_history);
    suica_model_initialize(model, 20); // Initialize with a capacity of 20 entries
    if(!is_metroflip_file) {
        return;
    }
    
    uint8_t* byte_array_buffer = (uint8_t*)malloc(FELICA_DATA_BLOCK_SIZE);
    FuriString* entry_preamble = furi_string_alloc();
    // Read the travel history entries
    for(uint8_t i = 0; i < SUICA_MAX_HISTORY_ENTRIES; i++) {
        furi_string_printf(entry_preamble, "Log %02X", i);
        // For every line in the flipper format file
        // We read the entire line's hex and store it in the byte_array_buffer
        if(!flipper_format_read_hex(
               format,
               furi_string_get_cstr(entry_preamble),
               byte_array_buffer,
               FELICA_DATA_BLOCK_SIZE))
            break;
        uint8_t block_data[16] = {0};
        for(size_t j = 0; j < FELICA_DATA_BLOCK_SIZE; j++) {
            block_data[j] = byte_array_buffer[j];
        }
        suica_add_entry(model, block_data);
    }
    furi_string_free(entry_preamble);
    free(byte_array_buffer);
}
