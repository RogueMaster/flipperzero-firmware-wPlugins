#include "include/domain/sensors_codec.h"
#include <string.h>

size_t sensors_codec_encode(const SensorsRecord* rec, uint8_t* buf, size_t buf_size) {
    if(rec->count > SENSOR_MAX_COUNT) {
        return 0;
    }
    const SensorsFileHeader head = {
        .magic = SENSORS_FILE_MAGIC,
        .version = SENSORS_FILE_VERSION,
        .count = rec->count,
        .active_index = rec->active_index,
    };
    const size_t body_size = (size_t)rec->count * sizeof(SensorData);
    const size_t total = sizeof(head) + body_size;
    if(total > buf_size) {
        return 0;
    }
    memcpy(buf, &head, sizeof(head));
    memcpy(buf + sizeof(head), rec->sensors, body_size);
    return total;
}

bool sensors_codec_decode(const uint8_t* buf, size_t len, SensorsRecord* rec) {
    SensorsFileHeader head;
    if(len < sizeof(head)) {
        return false;
    }
    memcpy(&head, buf, sizeof(head));
    if(head.magic != SENSORS_FILE_MAGIC || head.version != SENSORS_FILE_VERSION ||
       head.count > SENSOR_MAX_COUNT || head.active_index >= head.count) {
        return false;
    }
    const size_t body_size = (size_t)head.count * sizeof(SensorData);
    if(len < sizeof(head) + body_size) {
        return false;
    }
    rec->count = head.count;
    rec->active_index = head.active_index;
    memcpy(rec->sensors, buf + sizeof(head), body_size);
    return true;
}
