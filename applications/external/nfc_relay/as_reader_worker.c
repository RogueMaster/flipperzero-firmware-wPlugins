#include "as_reader_worker.h"
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <storage/storage.h>
#include <furi.h>
#include <datetime.h>
#include <furi_hal_rtc.h>

#define TAG                "AsReaderWorker"
#define WORKERSTOP         (1UL << 0UL)
#define POLLERFINISH       (1UL << 1UL)
#define POLLERTRXFINISH    (1UL << 2UL)
#define APDU_LOG_DIRECTORY "/ext/apps_data/nfc_relay"

// 确保日志目录存在
static void ensure_apdu_log_directory_exists() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_dir_exists(storage, APDU_LOG_DIRECTORY)) {
        FURI_LOG_I(TAG, "Creating directory: %s", APDU_LOG_DIRECTORY);
        storage_simply_mkdir(storage, APDU_LOG_DIRECTORY);
    }
    furi_record_close(RECORD_STORAGE);
}

// 获取当前时间字符串，格式为YYYYMMDD_HHMMSS
static void get_current_datetime(FuriString* datetime_str) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    furi_string_printf(
        datetime_str,
        "%04d%02d%02d_%02d%02d%02d",
        datetime.year,
        datetime.month,
        datetime.day,
        datetime.hour,
        datetime.minute,
        datetime.second);
}

// 将APDU命令和响应添加到缓存中
static void cache_apdu_transaction(
    AsReaderWorker* as_reader_worker,
    BitBuffer* command,
    BitBuffer* response) {
    FuriString* log_entry = furi_string_alloc();

    // 写入命令
    furi_string_printf(log_entry, "Command[%lu]: ", as_reader_worker->transaction_count);
    for(size_t i = 0; i < bit_buffer_get_size_bytes(command); i++) {
        furi_string_cat_printf(log_entry, "%02X ", bit_buffer_get_byte(command, i));
    }
    furi_string_cat(log_entry, "\n");

    // 写入响应
    furi_string_cat_printf(log_entry, "Response[%lu]: ", as_reader_worker->transaction_count);
    for(size_t i = 0; i < bit_buffer_get_size_bytes(response); i++) {
        furi_string_cat_printf(log_entry, "%02X ", bit_buffer_get_byte(response, i));
    }
    furi_string_cat(log_entry, "\n\n");

    // 添加到缓存
    furi_string_cat(as_reader_worker->apdu_log_buffer, furi_string_get_cstr(log_entry));
    as_reader_worker->has_apdu_logs = true;
    as_reader_worker->transaction_count++;

    furi_string_free(log_entry);
}

// 保存APDU日志到文件
static void save_apdu_logs(AsReaderWorker* as_reader_worker) {
    if(!as_reader_worker->has_apdu_logs) {
        FURI_LOG_D(TAG, "No APDU logs to save");
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* filename = furi_string_alloc();
    FuriString* datetime = furi_string_alloc();

    // 确保目录存在
    ensure_apdu_log_directory_exists();

    // 生成文件名
    get_current_datetime(datetime);
    furi_string_printf(
        filename, "%s/apdu_%s.apdulog", APDU_LOG_DIRECTORY, furi_string_get_cstr(datetime));

    // 打开文件进行写入
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // 写入缓存的日志
        storage_file_write(
            file,
            furi_string_get_cstr(as_reader_worker->apdu_log_buffer),
            furi_string_size(as_reader_worker->apdu_log_buffer));

        FURI_LOG_I(TAG, "APDU logs saved to %s", furi_string_get_cstr(filename));
    } else {
        FURI_LOG_E(TAG, "Failed to open file for writing: %s", furi_string_get_cstr(filename));
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_string_free(filename);
    furi_string_free(datetime);
    furi_record_close(RECORD_STORAGE);

    // 清空缓存
    furi_string_reset(as_reader_worker->apdu_log_buffer);
    as_reader_worker->has_apdu_logs = false;
}

void as_reader_worker_change_state(AsReaderWorker* as_reader_worker, AsReaderWorkerState state) {
    as_reader_worker->state = state;
    if(as_reader_worker->callback) as_reader_worker->callback(state, as_reader_worker->nfc_relay);
}

AsReaderWorker* as_reader_worker_alloc(NfcRelay* nfc_relay) {
    furi_assert(nfc_relay);
    FURI_LOG_D(TAG, "as_reader_worker_alloc");

    AsReaderWorker* as_reader_worker = malloc(sizeof(AsReaderWorker));
    as_reader_worker->thread = furi_thread_alloc_ex(
        "AsReaderWorkerThread", 2048, as_reader_worker_task, as_reader_worker);
    as_reader_worker->running = false;
    as_reader_worker->comm = comm_alloc(nfc_relay->config);
    as_reader_worker->nfc = nfc_alloc();
    as_reader_worker->bitbuffer_rx = bit_buffer_alloc(256);
    as_reader_worker->bitbuffer_tx = bit_buffer_alloc(256);
    as_reader_worker->nfc_relay = nfc_relay;
    as_reader_worker->callback = NULL;
    as_reader_worker->transaction_count = 0;

    // 初始化APDU日志缓存
    as_reader_worker->apdu_log_buffer = furi_string_alloc();
    as_reader_worker->has_apdu_logs = false;

    return as_reader_worker;
}

void as_reader_worker_free(AsReaderWorker* as_reader_worker) {
    furi_assert(as_reader_worker);
    FURI_LOG_D(TAG, "as_reader_worker_free");

    comm_free(as_reader_worker->comm);
    nfc_free(as_reader_worker->nfc);
    bit_buffer_free(as_reader_worker->bitbuffer_rx);
    bit_buffer_free(as_reader_worker->bitbuffer_tx);
    furi_thread_free(as_reader_worker->thread);

    // 释放APDU日志缓存
    furi_string_free(as_reader_worker->apdu_log_buffer);

    free(as_reader_worker);
}

void as_reader_worker_stop(AsReaderWorker* as_reader_worker) {
    furi_assert(as_reader_worker);
    FURI_LOG_D(TAG, "as_reader_worker_stop");

    as_reader_worker->running = false;
    furi_thread_flags_set(furi_thread_get_id(as_reader_worker->thread), WORKERSTOP);
    furi_thread_join(as_reader_worker->thread);
    comm_deinit(as_reader_worker->comm);

    // 保存APDU日志
    if(as_reader_worker->has_apdu_logs) {
        FURI_LOG_D(TAG, "Saving APDU logs");
        save_apdu_logs(as_reader_worker);
    }

    as_reader_worker->callback = NULL;
}

void as_reader_worker_start(AsReaderWorker* as_reader_worker, AsReaderWorkerCallback callback) {
    FURI_LOG_D(TAG, "as_reader_worker_start");
    furi_assert(as_reader_worker);
    as_reader_worker->running = true;

    comm_init(as_reader_worker->comm);
    as_reader_worker->callback = callback;
    furi_thread_start(as_reader_worker->thread);
}

static NfcCommand as_reader_worker_poller_trx_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    furi_assert(context);

    AsReaderWorker* as_reader_worker = context;
    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        if(as_reader_worker->state == AsReaderWorkerStateCardSearch) {
            //FURI_LOG_D(TAG, "Card Found");
            as_reader_worker_change_state(as_reader_worker, AsReaderWorkerStateCardFound);
            furi_thread_flags_set(furi_thread_get_id(as_reader_worker->thread), POLLERFINISH);
            return NfcCommandContinue;
        } else if(as_reader_worker->state == AsReaderWorkerStateInteractive) {
            if(as_reader_worker->apdu_ready) {
                FURI_LOG_D(TAG, "Card Found, Send TX");
                Iso14443_4aError err = iso14443_4a_poller_send_block(
                    event.instance,
                    as_reader_worker->bitbuffer_tx,
                    as_reader_worker->bitbuffer_rx);
                trace_bit_buffer_hexdump(TAG, "Emu Reader TX", as_reader_worker->bitbuffer_tx);
                if(err == Iso14443_4aErrorNone) {
                    trace_bit_buffer_hexdump(TAG, "Emu Reader RX", as_reader_worker->bitbuffer_rx);
                    FURI_LOG_D(TAG, "Card Found, Recv RX");

                    // 将APDU命令和响应添加到缓存
                    cache_apdu_transaction(
                        as_reader_worker,
                        as_reader_worker->bitbuffer_tx,
                        as_reader_worker->bitbuffer_rx);

                    NfcRelayPacket* packet = packet_alloc_data(
                        NfcRelayPacketApduResp,
                        bit_buffer_get_size_bytes(as_reader_worker->bitbuffer_rx),
                        bit_buffer_get_data(as_reader_worker->bitbuffer_rx));
                    comm_write_packet(as_reader_worker->comm, packet);
                    free(packet);
                    FURI_LOG_D(TAG, "Card Found, Sent Relay Packet");
                } else {
                    FURI_LOG_E(TAG, "NfcError: %d", err);
                }
                as_reader_worker->apdu_ready = false;
                furi_thread_flags_set(
                    furi_thread_get_id(as_reader_worker->thread), POLLERTRXFINISH);
                return NfcCommandContinue;
            } else {
                //FURI_LOG_T(TAG, "Card Ready, but apdu data not");
                furi_delay_ms(1);
                return NfcCommandContinue;
            }
        } else if(as_reader_worker->state == AsReaderWorkerStateCardFound) {
            FURI_LOG_D(TAG, "still in AsReaderWorkerStateCardFound");
            furi_delay_ms(1000);
            return NfcCommandContinue;
        } else {
            FURI_LOG_D(TAG, "WHAT?");
            return NfcCommandStop;
        }
    }
    furi_thread_flags_set(furi_thread_get_id(as_reader_worker->thread), POLLERTRXFINISH);
    return NfcCommandContinue;
}

int32_t as_reader_worker_task(void* context) {
    furi_assert(context);
    AsReaderWorker* as_reader_worker = context;

    FURI_LOG_D(TAG, "Send Ping Packet");
    comm_send_pingpong(as_reader_worker->comm, NfcRelayPacketPing, NfcRelayAsReader);
    NfcRelayPacket* packet = NULL;

    while(1) {
        if(!as_reader_worker->running) {
            FURI_LOG_D(TAG, "as_reader_worker stop running, break");
            break;
        }
        if(as_reader_worker->state == AsReaderWorkerStateWaitPong) {
            if(comm_wait_pong(as_reader_worker->comm, NfcRelayAsCard, NfcRelayAsReader)) {
                FURI_LOG_D(TAG, "comm_wait_pong succ, change state");
                as_reader_worker_change_state(as_reader_worker, AsReaderWorkerStateCardSearch);
                as_reader_worker->poller =
                    nfc_poller_alloc(as_reader_worker->nfc, NfcProtocolIso14443_4a);
                nfc_poller_start(
                    as_reader_worker->poller,
                    as_reader_worker_poller_trx_callback,
                    as_reader_worker);
            } else {
                furi_delay_ms(100);
                continue;
            }
        } else if(as_reader_worker->state == AsReaderWorkerStateCardSearch) {
            FURI_LOG_D(TAG, "AsReaderWorkerStateCardSearch");
            furi_thread_flags_wait(POLLERFINISH || WORKERSTOP, FuriFlagWaitAny, 1000);
            continue;
        } else if(as_reader_worker->state == AsReaderWorkerStateCardFound) {
            FURI_LOG_D(TAG, "Card Found");
            const Iso14443_4aData* dev_data = nfc_poller_get_data(as_reader_worker->poller);
            SerializedIso14443_4a* serialized = iso14443_4a_serialize(dev_data);
            packet = packet_alloc_data(
                NfcRelayPacketNfcDevData, sizeof(*serialized) + serialized->len_t1_tk, serialized);
            FURI_LOG_D(TAG, "packet->len: %d", packet->len);
            comm_write_packet(as_reader_worker->comm, packet);
            free(packet);
            packet = NULL;
            free(serialized);
            serialized = NULL;
            as_reader_worker_change_state(as_reader_worker, AsReaderWorkerStateInteractive);
            continue;
        } else if(as_reader_worker->state == AsReaderWorkerStateInteractive) {
            NfcRelayPacket* recv_packet;
            recv_packet = comm_wait_packet(as_reader_worker->comm, NfcRelayPacketApduReq);
            if(!recv_packet) {
                continue;
            }
            FURI_LOG_D(TAG, "Recv NfcRelayPacketApduReq");
            // drop PCB
            bit_buffer_copy_bytes(
                as_reader_worker->bitbuffer_tx, &recv_packet->buf[1], recv_packet->len - 1);
            trace_bit_buffer_hexdump(TAG, "Reader apdu recv", as_reader_worker->bitbuffer_tx);
            free(recv_packet);
            recv_packet = NULL;
            as_reader_worker->apdu_ready = true;
            furi_thread_flags_wait(POLLERTRXFINISH || WORKERSTOP, FuriFlagWaitAny, 100);
        }
    }
    if(as_reader_worker->poller) {
        nfc_poller_stop(as_reader_worker->poller);
        nfc_poller_free(as_reader_worker->poller);
        as_reader_worker->poller = NULL;
    }
    FURI_LOG_D(TAG, "as_reader_worker stop running, poller freed");

    return 0;
}
