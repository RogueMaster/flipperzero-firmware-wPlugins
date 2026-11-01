#include "fdxb_sensor.h"

#include <furi.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <bit_lib/bit_lib.h>

#define TAG "FdxbSensor"

/* Decoded FDX-B data of the firmware decoder:
 *  0..37  national code, 38..47 country code (both LSB first)
 * 48      data block status flag
 * 63      animal application indicator
 * 64..87  extended data block, here the temperature */
#define FDXB_DATA_SIZE (11)

/* The reader needs about 0.6 s per reading (450 ms settling + 4 identical frames),
 * report the chip as gone after missing a few */
#define FDXB_LOST_TIMEOUT_MS (2500)

typedef enum {
    FdxbFlagDone = (1 << 0), /**< The LF RFID worker accepted a frame */
    FdxbFlagStop = (1 << 1),
} FdxbFlag;

#define FDXB_FLAGS_ALL (FdxbFlagDone | FdxbFlagStop)

struct FdxbSensor {
    const ProtocolBase* protocol_list[1]; /**< The dictionary keeps a pointer to this */
    ProtocolDict* protocols;
    LFRFIDWorker* worker;
    FuriThread* thread;

    TempSensorCallback callback;
    void* context;
};

/* The dictionary only holds FDX-B, so other LF tags are not decoded and cannot match by
 * accident. Protocol IDs are indices into it. */
#define FDXB_PROTOCOL_INDEX (0)

/* Same extraction as the stock firmware: 8 bit value, odd parity, 0.2 F per step from 74 F */
static bool fdxb_sensor_get_temperature(const uint8_t* data, float* celsius) {
    uint32_t extended = bit_lib_get_bits_32(data, 64, 24) << 8;
    bit_lib_reverse_bits((uint8_t*)&extended, 0, 32);

    const uint8_t parity = (extended >> 8) & 0x01;
    const uint8_t value = extended & 0xFF;
    const bool calculated_parity = bit_lib_test_parity_32(value, BitLibParityOdd);

    if((calculated_parity != parity) || (extended & 0xE00)) {
        return false;
    }

    const float fahrenheit = 74.0f + value * 0.2f;
    *celsius = (fahrenheit - 32.0f) / 1.8f;
    return true;
}

static void fdxb_sensor_format_id(const uint8_t* data, char* id, size_t size) {
    uint64_t national_code = bit_lib_get_bits_32(data, 0, 32);
    national_code = national_code << 32;
    national_code |= (uint64_t)bit_lib_get_bits_32(data, 32, 6) << (32 - 6);
    bit_lib_reverse_bits((uint8_t*)&national_code, 0, 64);

    uint16_t country_code = bit_lib_get_bits_16(data, 38, 10) << 6;
    bit_lib_reverse_bits((uint8_t*)&country_code, 0, 16);

    snprintf(id, size, "%03u-%012llu", country_code, national_code);
}

/* LF RFID worker thread */
static void
    fdxb_sensor_read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    FuriThread* thread = context;

    if(result == LFRFIDWorkerReadDone && protocol == FDXB_PROTOCOL_INDEX) {
        furi_thread_flags_set(furi_thread_get_id(thread), FdxbFlagDone);
    }
}

static void fdxb_sensor_report(FdxbSensor* instance) {
    uint8_t data[FDXB_DATA_SIZE];
    TempSensorReading reading = {.status = TempSensorStatusOk};

    protocol_dict_get_data(instance->protocols, FDXB_PROTOCOL_INDEX, data, sizeof(data));
    fdxb_sensor_format_id(data, reading.id, sizeof(reading.id));

    if(!fdxb_sensor_get_temperature(data, &reading.temperature)) {
        reading.status = TempSensorStatusNotSensor;
    }

    instance->callback(&reading, instance->context);
}

/* Restarts the one-shot read of the LF RFID worker after every result */
static int32_t fdxb_sensor_thread(void* context) {
    FdxbSensor* instance = context;
    FuriThread* thread = furi_thread_get_current();
    bool present = false;

    lfrfid_worker_start_thread(instance->worker);
    lfrfid_worker_read_start(
        instance->worker, LFRFIDWorkerReadTypeASKOnly, fdxb_sensor_read_callback, thread);

    while(true) {
        const uint32_t flags =
            furi_thread_flags_wait(FDXB_FLAGS_ALL, FuriFlagWaitAny, FDXB_LOST_TIMEOUT_MS);

        if(flags == (uint32_t)FuriFlagErrorTimeout) {
            // The read keeps running, only tell the UI that nothing answers
            if(present) {
                present = false;
                const TempSensorReading reading = {.status = TempSensorStatusWaiting};
                instance->callback(&reading, instance->context);
            }
            continue;
        }

        if(flags & FuriFlagError) {
            continue;
        }

        if(flags & FdxbFlagStop) {
            break;
        }

        if(flags & FdxbFlagDone) {
            // The worker is idle after a result, its data stays until the next read
            present = true;
            fdxb_sensor_report(instance);
            lfrfid_worker_read_start(
                instance->worker, LFRFIDWorkerReadTypeASKOnly, fdxb_sensor_read_callback, thread);
        }
    }

    // StopMode ends a running read, StopThread then ends the worker thread
    lfrfid_worker_stop(instance->worker);
    lfrfid_worker_stop_thread(instance->worker);

    return 0;
}

FdxbSensor* fdxb_sensor_alloc(void) {
    FdxbSensor* instance = malloc(sizeof(FdxbSensor));

    instance->protocol_list[FDXB_PROTOCOL_INDEX] = lfrfid_protocols[LFRFIDProtocolFDXB];
    instance->protocols =
        protocol_dict_alloc(instance->protocol_list, COUNT_OF(instance->protocol_list));
    instance->worker = lfrfid_worker_alloc(instance->protocols);
    instance->thread = furi_thread_alloc_ex(TAG, 1024, fdxb_sensor_thread, instance);

    return instance;
}

void fdxb_sensor_free(FdxbSensor* instance) {
    furi_check(instance);

    furi_thread_free(instance->thread);
    lfrfid_worker_free(instance->worker);
    protocol_dict_free(instance->protocols);

    free(instance);
}

void fdxb_sensor_start(FdxbSensor* instance, TempSensorCallback callback, void* context) {
    furi_check(instance);
    furi_check(callback);

    instance->callback = callback;
    instance->context = context;

    const TempSensorReading reading = {.status = TempSensorStatusWaiting};
    callback(&reading, context);

    furi_thread_start(instance->thread);
}

void fdxb_sensor_stop(FdxbSensor* instance) {
    furi_check(instance);

    furi_thread_flags_set(furi_thread_get_id(instance->thread), FdxbFlagStop);
    furi_thread_join(instance->thread);
}
