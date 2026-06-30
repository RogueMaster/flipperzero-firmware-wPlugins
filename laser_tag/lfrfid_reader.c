#include "lfrfid_reader.h"
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>
#include <lib/lfrfid/lfrfid_worker.h>

#define TAG "LfRfid_Reader"

typedef enum {
    LFRFIDReaderEventTagRead = (1 << 0),
    LFRFIDReaderEventStopThread = (1 << 1),
    LFRFIDReaderEventAll = (LFRFIDReaderEventTagRead | LFRFIDReaderEventStopThread),
} LFRFIDReaderEventType;

struct LFRFIDReader {
    char* requested_protocol;
    ProtocolId protocol;
    ProtocolDict* dict;
    LFRFIDWorker* worker;
    FuriThread* thread;
    LFRFIDReaderTagCallback callback;
    void* callback_context;
};

static void lfrfid_cli_read_callback(LFRFIDWorkerReadResult result, ProtocolId proto, void* ctx) {
    furi_assert(ctx);
    LFRFIDReader* context = ctx;
    if(result == LFRFIDWorkerReadDone) {
        context->protocol = proto;
        furi_thread_flags_set(furi_thread_get_id(context->thread), LFRFIDReaderEventTagRead);
    }
}

LFRFIDReader* lfrfid_reader_alloc() {
    LFRFIDReader* reader = malloc(sizeof(LFRFIDReader));
    reader->protocol = PROTOCOL_NO;
    reader->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    reader->worker = lfrfid_worker_alloc(reader->dict);

    return reader;
}

void lfrfid_reader_set_tag_callback(
    LFRFIDReader* reader,
    char* requested_protocol,
    LFRFIDReaderTagCallback callback,
    void* context) {
    furi_assert(reader);
    furi_assert(requested_protocol);
    reader->requested_protocol = requested_protocol;
    reader->callback = callback;
    reader->callback_context = context;
}

static int32_t lfrfid_reader_start_thread(void* ctx) {
    LFRFIDReader* reader = (LFRFIDReader*)ctx;
    furi_thread_flags_clear(LFRFIDReaderEventAll);
    lfrfid_worker_start_thread(reader->worker);
    lfrfid_worker_read_start(
        reader->worker, LFRFIDWorkerReadTypeASKOnly, lfrfid_cli_read_callback, reader);
    while(true) {
        uint32_t flags = furi_thread_flags_wait(LFRFIDReaderEventAll, FuriFlagWaitAny, 100);

        if(flags != (unsigned)FuriFlagErrorTimeout) {
            if((flags & LFRFIDReaderEventTagRead) == LFRFIDReaderEventTagRead) {
                furi_thread_flags_clear(LFRFIDReaderEventTagRead);
                if(reader->protocol != PROTOCOL_NO) {
                    const char* protocol_name =
                        protocol_dict_get_name(reader->dict, reader->protocol);
                    if(strcmp(protocol_name, reader->requested_protocol) == 0) {
                        size_t size = protocol_dict_get_data_size(reader->dict, reader->protocol);
                        uint8_t* data = malloc(size);
                        protocol_dict_get_data(reader->dict, reader->protocol, data, size);
                        if(reader->callback) {
                            FURI_LOG_D(TAG, "Tag %s detected", protocol_name);
                            reader->callback(data, size, reader->callback_context);
                        } else {
                            FURI_LOG_W(TAG, "No callback set for tag %s", protocol_name);
                        }
                        free(data);
                    } else {
                        FURI_LOG_W(TAG, "Unsupported tag %s, expected EM4100", protocol_name);
                    }
                }
                reader->protocol = PROTOCOL_NO;
                lfrfid_worker_read_start(
                    reader->worker, LFRFIDWorkerReadTypeASKOnly, lfrfid_cli_read_callback, reader);
            } else if((flags & LFRFIDReaderEventStopThread) == LFRFIDReaderEventStopThread) {
                break;
            }
        }
    }
    lfrfid_worker_stop(reader->worker);
    lfrfid_worker_stop_thread(reader->worker);
    FURI_LOG_D(TAG, "LfRfidReader thread exiting");
    return 0;
}

void lfrfid_reader_start(LFRFIDReader* reader) {
    reader->thread =
        furi_thread_alloc_ex("lfrfid_reader", 2048, lfrfid_reader_start_thread, reader);
    furi_thread_start(reader->thread);
}

void lfrfid_reader_stop(LFRFIDReader* reader) {
    if(reader->thread) {
        furi_thread_flags_set(furi_thread_get_id(reader->thread), LFRFIDReaderEventStopThread);
        furi_thread_join(reader->thread);
        reader->thread = NULL;
    }
}

void lfrfid_reader_free(LFRFIDReader* reader) {
    lfrfid_reader_stop(reader);
    protocol_dict_free(reader->dict);
    lfrfid_worker_free(reader->worker);
    free(reader);
}
