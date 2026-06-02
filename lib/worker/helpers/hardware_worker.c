#include "hardware_worker.h"
#include "furi.h"

#if defined(RFID_125_PROTOCOL)

#include <lib/lfrfid/lfrfid_dict_file.h>
#include <lib/lfrfid/lfrfid_worker.h>

#include <furi_hal_rfid.h>
#include <toolbox/level_duration.h>
#include <lib/toolbox/pulse_protocols/pulse_glue.h>

// Direct-HAL emulation for RFID.
//
// The stock lfrfid_worker can't restart emulation faster than ~0.10s: since
// firmware PR #3507 (2024-03) lfrfid_worker_emulate_start() does a hard
// furi_check(mode == LFRFIDWorkerIdle), and lfrfid_worker_stop() is async with
// up to a ~100ms return-to-idle latency, so a faster restart crashes.
//
// We bypass the worker entirely and drive furi_hal_rfid_tim_emulate_dma_*
// ourselves on a private thread (same primitives the firmware worker uses
// internally, all exported in the public API). furi_hal_rfid_tim_emulate_dma_stop()
// is synchronous, so stop/restart costs microseconds and we hit the real
// hardware timing floor with no firmware fork required.
#define FUZZER_RFID_EMULATE_BUFFER_SIZE (1024)
// How long the refill loop blocks before re-checking the stop flag (ms). Bounds
// the stop latency; kept small so a fast TD never outruns teardown.
#define FUZZER_RFID_EMU_RECEIVE_TIMEOUT (2)

typedef struct {
    uint32_t duration[FUZZER_RFID_EMULATE_BUFFER_SIZE];
    uint32_t pulse[FUZZER_RFID_EMULATE_BUFFER_SIZE];
} HwRfidEmulateBuffer;

typedef enum {
    HwRfidHalfTransfer,
    HwRfidTransferComplete,
} HwRfidEmulateDMAEvent;

#else

#include <lib/ibutton/ibutton_worker.h>
#include <lib/ibutton/ibutton_key.h>

#endif

#define TAG "Fuzzer HW worker"

struct HardwareWorker {
#if defined(RFID_125_PROTOCOL)
    LFRFIDWorker* proto_worker;
    ProtocolId protocol_id;
    ProtocolDict* protocols_items;
    FuriThread* emu_thread; // private direct-HAL emulation thread (NULL when idle)
    volatile bool emu_stop; // request the emulation thread to stop
#else
    iButtonWorker* proto_worker;
    iButtonProtocolId protocol_id;
    iButtonProtocols* protocols_items;
    iButtonKey* key;
#endif
};

HardwareWorker* hardware_worker_alloc() {
    HardwareWorker* instance = malloc(sizeof(HardwareWorker));
#if defined(RFID_125_PROTOCOL)
    instance->protocols_items = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);

    instance->proto_worker = lfrfid_worker_alloc(instance->protocols_items);
    instance->emu_thread = NULL;
    instance->emu_stop = false;
#else
    instance->protocols_items = ibutton_protocols_alloc();
    instance->key =
        ibutton_key_alloc(ibutton_protocols_get_max_data_size(instance->protocols_items));

    instance->proto_worker = ibutton_worker_alloc(instance->protocols_items);
#endif
    return instance;
}

void hardware_worker_free(HardwareWorker* instance) {
#if defined(RFID_125_PROTOCOL)
    lfrfid_worker_free(instance->proto_worker);

    protocol_dict_free(instance->protocols_items);
#else
    ibutton_worker_free(instance->proto_worker);

    ibutton_key_free(instance->key);
    ibutton_protocols_free(instance->protocols_items);
#endif
    free(instance);
}

void hardware_worker_start_thread(HardwareWorker* instance) {
#if defined(RFID_125_PROTOCOL)
    lfrfid_worker_start_thread(instance->proto_worker);
#else
    ibutton_worker_start_thread(instance->proto_worker);
#endif
}

void hardware_worker_stop_thread(HardwareWorker* instance) {
#if defined(RFID_125_PROTOCOL)
    lfrfid_worker_stop(instance->proto_worker);
    lfrfid_worker_stop_thread(instance->proto_worker);
#else
    ibutton_worker_stop(instance->proto_worker);
    ibutton_worker_stop_thread(instance->proto_worker);
#endif
}

#if defined(RFID_125_PROTOCOL)

// DMA half/complete interrupt: forwards the refill request to the worker thread.
static void hardware_worker_rfid_emulate_dma_isr(bool half, void* context) {
    FuriStreamBuffer* stream = context;
    uint32_t flag = half ? HwRfidHalfTransfer : HwRfidTransferComplete;
    furi_stream_buffer_send(stream, &flag, sizeof(uint32_t), 0);
}

// Refill one half of the DMA buffer from the protocol encoder.
static void hardware_worker_rfid_fill_half(
    HardwareWorker* instance,
    HwRfidEmulateBuffer* buffer,
    PulseGlue* pulse_glue,
    size_t start) {
    for(size_t i = 0; i < (FUZZER_RFID_EMULATE_BUFFER_SIZE / 2); i++) {
        bool pulse_pop = false;
        while(!pulse_pop) {
            LevelDuration level_duration =
                protocol_dict_encoder_yield(instance->protocols_items, instance->protocol_id);
            pulse_pop = pulse_glue_push(
                pulse_glue,
                level_duration_get_level(level_duration),
                level_duration_get_duration(level_duration));
        }
        uint32_t duration, pulse;
        pulse_glue_pop(pulse_glue, &duration, &pulse);
        buffer->duration[start + i] = duration - 1;
        buffer->pulse[start + i] = pulse;
    }
}

// Private emulation thread: a port of lfrfid_worker_mode_emulate_process, but
// owned by us so stop is synchronous (see header comment near the buffer defs).
static int32_t hardware_worker_rfid_emulate_thread(void* context) {
    HardwareWorker* instance = context;

    HwRfidEmulateBuffer* buffer = malloc(sizeof(HwRfidEmulateBuffer));
    FuriStreamBuffer* stream = furi_stream_buffer_alloc(sizeof(uint32_t), sizeof(uint32_t));
    PulseGlue* pulse_glue = pulse_glue_alloc();

    protocol_dict_encoder_start(instance->protocols_items, instance->protocol_id);

    // Prefill the whole buffer before starting the DMA.
    hardware_worker_rfid_fill_half(instance, buffer, pulse_glue, 0);
    hardware_worker_rfid_fill_half(instance, buffer, pulse_glue, FUZZER_RFID_EMULATE_BUFFER_SIZE / 2);

    furi_hal_rfid_tim_emulate_dma_start(
        buffer->duration,
        buffer->pulse,
        FUZZER_RFID_EMULATE_BUFFER_SIZE,
        hardware_worker_rfid_emulate_dma_isr,
        stream);

    while(!instance->emu_stop) {
        uint32_t flag = 0;
        size_t size = furi_stream_buffer_receive(
            stream, &flag, sizeof(uint32_t), FUZZER_RFID_EMU_RECEIVE_TIMEOUT);

        if(size == sizeof(uint32_t)) {
            size_t start = (flag == HwRfidTransferComplete) ? (FUZZER_RFID_EMULATE_BUFFER_SIZE / 2) : 0;
            hardware_worker_rfid_fill_half(instance, buffer, pulse_glue, start);
        }
    }

    furi_hal_rfid_tim_emulate_dma_stop();

    free(buffer);
    furi_stream_buffer_free(stream);
    pulse_glue_free(pulse_glue);
    return 0;
}

#endif

void hardware_worker_emulate_start(HardwareWorker* instance) {
#if defined(RFID_125_PROTOCOL)
    if(instance->emu_thread) {
        // Already emulating; nothing to do.
        return;
    }
    instance->emu_stop = false;
    instance->emu_thread = furi_thread_alloc_ex(
        "FuzzerRfidEmu", 2048, hardware_worker_rfid_emulate_thread, instance);
    furi_thread_start(instance->emu_thread);
#else
    ibutton_worker_emulate_start(instance->proto_worker, instance->key);
#endif
}

void hardware_worker_stop(HardwareWorker* instance) {
#if defined(RFID_125_PROTOCOL)
    if(instance->emu_thread) {
        instance->emu_stop = true;
        // Synchronous: the refill loop notices emu_stop within
        // FUZZER_RFID_EMU_RECEIVE_TIMEOUT ms, then stops the DMA and exits.
        furi_thread_join(instance->emu_thread);
        furi_thread_free(instance->emu_thread);
        instance->emu_thread = NULL;
    }
#else
    ibutton_worker_stop(instance->proto_worker);
#endif
}

void hardware_worker_set_protocol_data(
    HardwareWorker* instance,
    uint8_t* payload,
    uint8_t payload_size) {
#if defined(RFID_125_PROTOCOL)
    protocol_dict_set_data(
        instance->protocols_items, instance->protocol_id, payload, payload_size);
#else
    ibutton_key_set_protocol_id(instance->key, instance->protocol_id);
    iButtonEditableData data;
    ibutton_protocols_get_editable_data(instance->protocols_items, instance->key, &data);

    furi_check(payload_size >= data.size);
    memcpy(data.ptr, payload, data.size);
#endif
}

void hardware_worker_get_protocol_data(
    HardwareWorker* instance,
    uint8_t* payload,
    uint8_t payload_size) {
#if defined(RFID_125_PROTOCOL)
    protocol_dict_get_data(
        instance->protocols_items, instance->protocol_id, payload, payload_size);
#else
    iButtonEditableData data;
    ibutton_protocols_get_editable_data(instance->protocols_items, instance->key, &data);
    furi_check(payload_size >= data.size);
    memcpy(payload, data.ptr, data.size);
#endif
}

static bool hardware_worker_protocol_is_valid(HardwareWorker* instance) {
#if defined(RFID_125_PROTOCOL)
    if(instance->protocol_id != PROTOCOL_NO) {
        return true;
    }
#else
    if(instance->protocol_id != iButtonProtocolIdInvalid) {
        return true;
    }
#endif
    return false;
}

bool hardware_worker_set_protocol_id_by_name(HardwareWorker* instance, const char* protocol_name) {
#if defined(RFID_125_PROTOCOL)
    instance->protocol_id =
        protocol_dict_get_protocol_by_name(instance->protocols_items, protocol_name);
    return (instance->protocol_id != PROTOCOL_NO);
#else
    instance->protocol_id =
        ibutton_protocols_get_id_by_name(instance->protocols_items, protocol_name);
    return (instance->protocol_id != iButtonProtocolIdInvalid);
#endif
}

HwProtocolID hardware_worker_get_protocol_id(HardwareWorker* instance) {
    if(hardware_worker_protocol_is_valid(instance)) {
        return instance->protocol_id;
    }
    return -1;
}

bool hardware_worker_load_key_from_file(HardwareWorker* instance, const char* filename) {
    bool res = false;

#if defined(RFID_125_PROTOCOL)
    ProtocolId loaded_proto_id = lfrfid_dict_file_load(instance->protocols_items, filename);
    if(loaded_proto_id == PROTOCOL_NO) {
        // Err Cant load file
        FURI_LOG_W(TAG, "Cant load file");
    } else {
        instance->protocol_id = loaded_proto_id;
        res = true;
    }
#else

    if(!ibutton_protocols_load(instance->protocols_items, instance->key, filename)) {
        // Err Cant load file
        FURI_LOG_W(TAG, "Cant load file");
    } else {
        instance->protocol_id = ibutton_key_get_protocol_id(instance->key);
        res = true;
    }

#endif

    return res;
}

bool hardware_worker_save_key(HardwareWorker* instance, const char* path) {
    furi_assert(instance);
    bool res;

#if defined(RFID_125_PROTOCOL)
    res = lfrfid_dict_file_save(instance->protocols_items, instance->protocol_id, path);
#else

    res = ibutton_protocols_save(instance->protocols_items, instance->key, path);
#endif

    return res;
}