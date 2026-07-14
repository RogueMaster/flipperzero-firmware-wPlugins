#include "query_worker.h"
#include "storage/storage.h"
#include "utils/database.h"

#define TAG "QueryWorker"

struct TUM_QueryWorker {
    bool flag;
    FuriThread* thread;
    Storage* storage;

    TUM_QueryWorkerCallback cb;
    void* ctx;

    TUnionMessage* msg;
    TUnionMessageExt* msg_ext;
};

static int32_t tum_query_worker_thread_cb(void* ctx) {
    TUM_QueryWorker* instance = ctx;
    TUnionMessage* msg = instance->msg;
    TUnionMessageExt* msg_ext = instance->msg_ext;
    uint8_t progress = 0;
    const uint8_t total_progress = msg->travel_cnt * 4 + 2;
    FuriString* line_label = furi_string_alloc();
    bool status;

    furi_assert(msg);
    furi_assert(msg_ext);

    FURI_LOG_D(TAG, "start query!");
    instance->cb(TUM_QueryWorkerEventStart, total_progress, instance->ctx);
    tum_db_get_card_type_name(msg->type, msg_ext->type_name);
    status = tum_db_query_card_name(instance->storage, msg->card_number, msg_ext->card_name);
    instance->cb(TUM_QueryWorkerEventProgress, ++progress, instance->ctx);
    if(!status) {
        furi_string_reset(msg_ext->card_name);
    }
    status = tum_db_query_city_name(instance->storage, msg->city_id, msg_ext->city_name);
    instance->cb(TUM_QueryWorkerEventProgress, ++progress, instance->ctx);
    if(!status) {
        furi_string_reset(msg_ext->city_name);
    }

    for(uint8_t index = 0; index < msg->travel_cnt; index++) {
        TUnionTravel* travel = &msg->travels[index];
        TUnionTravelExt* travel_ext = &msg_ext->travels_ext[index];

        // 获取城市id
        char city_id[5] = {0};
        memcpy(city_id, &travel->institution_id[4], 4);

        // 是否漫游
        if(strncmp(city_id, msg->city_id, 4)) travel_ext->roaming = true;

        // 是否夜间
        if(travel->hour >= 23 || travel->hour < 6) travel_ext->night = true;

        // 查询城市名
        status = tum_db_query_city_name(instance->storage, city_id, travel_ext->city_name);
        instance->cb(TUM_QueryWorkerEventProgress, ++progress, instance->ctx);
        if(!status) furi_string_reset(travel_ext->city_name);

        // 查询线路标签
        status =
            tum_db_query_line_label(instance->storage, city_id, travel->station_id, line_label);
        instance->cb(TUM_QueryWorkerEventProgress, ++progress, instance->ctx);
        if(status) {
            // 查询线路名+类型
            status = tum_db_query_line_name(
                instance->storage,
                city_id,
                furi_string_get_cstr(line_label),
                travel_ext->line_name,
                &travel_ext->line_type);
            instance->cb(TUM_QueryWorkerEventProgress, ++progress, instance->ctx);
            if(status) {
                // 查询站台名
                status = tum_db_query_station_name(
                    instance->storage,
                    city_id,
                    furi_string_get_cstr(line_label),
                    travel->station_id,
                    travel_ext->station_name);
                instance->cb(TUM_QueryWorkerEventProgress, ++progress, instance->ctx);
                if(!status) {
                    furi_string_reset(travel_ext->station_name);
                }
            } else {
                instance->cb(TUM_QueryWorkerEventProgress, ++progress, instance->ctx);
                furi_string_reset(travel_ext->line_name);
            }
        } else {
            progress += 2;
            instance->cb(TUM_QueryWorkerEventProgress, progress, instance->ctx);
        }

        furi_string_reset(line_label);

        // 是否换乘
        if(index > 0 && travel->sub_type == 0x01 && travel->type == 0x03 &&
           furi_string_size(travel_ext->line_name) != 0) {
            TUnionTravel* travel_fore = &msg->travels[index - 1];
            TUnionTravelExt* travel_ext_fore = &msg_ext->travels_ext[index - 1];

            if(furi_string_equal(travel_ext->line_name, travel_ext_fore->line_name)) {
                // 同名线路跨城市换乘
                if(strncmp(travel->city_id, travel_fore->city_id, 4)) travel_ext->transfer = true;
            } else {
                travel_ext->transfer = true;
            }
        }
    }
    instance->cb(TUM_QueryWorkerEventFinish, progress, instance->ctx);
    furi_string_free(line_label);
    FURI_LOG_D(TAG, "query ok!");
    return 0;
}

void tum_query_worker_start(TUM_QueryWorker* instance) {
    furi_assert(instance->cb);
    furi_thread_start(instance->thread);
}

void tum_query_worker_set_cb(TUM_QueryWorker* instance, TUM_QueryWorkerCallback cb, void* ctx) {
    instance->cb = cb;
    instance->ctx = ctx;
}

TUM_QueryWorker* tum_query_worker_alloc(TUnionMessage* msg, TUnionMessageExt* msg_ext) {
    TUM_QueryWorker* instance = malloc(sizeof(TUM_QueryWorker));

    instance->thread =
        furi_thread_alloc_ex("TUM_QueryWorker", 1024, tum_query_worker_thread_cb, instance);
    instance->storage = furi_record_open(RECORD_STORAGE);
    instance->msg = msg;
    instance->msg_ext = msg_ext;

    return instance;
}

void tum_query_worker_free(TUM_QueryWorker* instance) {
    furi_thread_join(instance->thread);
    furi_thread_free(instance->thread);
    furi_record_close(RECORD_STORAGE);

    free(instance);
}
