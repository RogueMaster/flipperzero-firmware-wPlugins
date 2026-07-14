#include "../../fire_string.h"

typedef struct __attribute__((packed)) {
    uint16_t offset;
    uint8_t length;
} WordSpan;

static int32_t word_list_worker(void* context) {
    FURI_LOG_T(TAG, "word_list_worker");
    furi_check(context);

    FireString* app = context;
    FURI_LOG_I(TAG, "word_list_worker %p starting", furi_thread_get_id(app->thread));

    // Free existing word list if present
    if(app->dict->word_list != NULL) {
        for(uint16_t i = 0; i < app->dict->len; i++) {
            furi_string_free(app->dict->word_list[i]);
            app->dict->word_list[i] = NULL;
        }
        free(app->dict->word_list);
        app->dict->word_list = NULL;
    }
    app->dict->len = 0;

    // Declare all variables up front — required for goto to jump over them safely
    File* word_file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    uint8_t* file_buf = NULL;
    WordSpan* spans = NULL;
    int32_t result = -1;
    uint16_t buf_size = 0;
    uint16_t read_count = 0;
    uint16_t word_count = 0;
    uint16_t span_index = 0;
    uint16_t word_start = 0;

    if(!storage_file_open(word_file, APP_ASSETS_PATH(DICT_FILE), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "File open error");
        goto cleanup;
    }

    buf_size = storage_file_size(word_file);
    if(buf_size == 0) {
        FURI_LOG_E(TAG, "File empty");
        goto cleanup;
    }
    if(buf_size > memmgr_get_free_heap()) {
        FURI_LOG_E(TAG, "File too large");
        goto cleanup;
    }

    file_buf = malloc(buf_size);
    if(file_buf == NULL) {
        FURI_LOG_E(TAG, "file_buf alloc failed");
        goto cleanup;
    }

    read_count = storage_file_read(word_file, file_buf, buf_size);
    if(read_count != buf_size) {
        FURI_LOG_E(TAG, "Partial read: expected %zu got %zu", buf_size, read_count);
        goto cleanup;
    }

    // Pass 1: count words
    for(uint16_t i = 0; i < read_count; i++) {
        if(file_buf[i] == '\n') word_count++;
    }
    if(word_count < DICT_MAX_SIZE) {
        FURI_LOG_E(TAG, "Word count %u less than DICT_MAX_SIZE %u", word_count, DICT_MAX_SIZE);
        goto cleanup;
    }

    // Pass 2: record offset and length of each word
    spans = malloc(sizeof(WordSpan) * word_count);
    if(spans == NULL) {
        FURI_LOG_E(TAG, "spans alloc failed");
        goto cleanup;
    }
    for(uint16_t i = 0; i < read_count; i++) {
        if(file_buf[i] == '\n') {
            spans[span_index].offset = (uint16_t)word_start;
            spans[span_index].length = (uint8_t)(i - word_start);
            span_index++;
            word_start = i + 1;
        }
    }

    // Pass 3: partial Fisher-Yates shuffle directly on spans
    for(uint16_t i = 0; i < DICT_MAX_SIZE; i++) {
        uint16_t j = i + (furi_hal_random_get() % (word_count - i));
        WordSpan tmp = spans[i];
        spans[i] = spans[j];
        spans[j] = tmp;
    }

    // Pass 4: materialise only the DICT_MAX_SIZE selected words as FuriStrings
    app->dict->word_list = calloc(DICT_MAX_SIZE, sizeof(FuriString*));
    if(app->dict->word_list == NULL) {
        FURI_LOG_E(TAG, "word_list alloc failed");
        goto cleanup;
    }
    for(uint16_t i = 0; i < DICT_MAX_SIZE; i++) {
        FuriString* s = furi_string_alloc();
        furi_string_set_strn(s, (const char*)(file_buf + spans[i].offset), spans[i].length);
        app->dict->word_list[i] = s;
    }

    app->dict->len = DICT_MAX_SIZE;
    result = 0;

cleanup:
    free(spans);
    free(file_buf);
    storage_file_close(word_file);
    storage_file_free(word_file);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "word_list_worker %p stopping", furi_thread_get_id(app->thread));
    return result;
}

void fire_string_scene_on_enter_loading_word_list(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_loading_word_list");
    furi_check(context);

    FireString* app = context;

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Loading);

    app->thread = furi_thread_alloc_ex("WordListWorker", 4096, word_list_worker, app);
    furi_thread_start(app->thread);
}

bool fire_string_scene_on_event_loading_word_list(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "fire_string_scene_on_event_loading_word_list");

    FireString* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeTick:
        if(furi_thread_get_state(app->thread) == FuriThreadStateStopped) {
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, FireStringScene_Generate)) {
                scene_manager_next_scene(app->scene_manager, FireStringScene_Generate);
            }
        }
        break;
    default:
        break;
    }

    return consumed;
}

void fire_string_scene_on_exit_loading_word_list(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_loading_word_list");
    furi_check(context);

    FireString* app = context;

    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
    app->thread = NULL;
}
