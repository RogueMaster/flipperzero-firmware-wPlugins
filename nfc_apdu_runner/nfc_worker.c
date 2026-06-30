#include <furi.h>
#include <furi_hal.h>
#include "nfc_apdu_runner.h"
#include <lib/nfc/nfc.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#define TAG "NfcWorker"

// APDU上下文结构体，用于在回调中传递APDU命令和结果
typedef struct {
    BitBuffer* tx_buffer; // 发送缓冲区
    BitBuffer* rx_buffer; // 接收缓冲区
    bool ready; // 是否准备好发送
    bool is_last; // 是否是最后一条指令
    FuriThreadId thread_id; // 线程ID，用于发送标志
} ApduContext;

// 定义线程标志
#define APDU_POLLER_DONE (1 << 0)
#define APDU_POLLER_ERR  (1 << 1)

// NFC Worker结构体定义
struct NfcWorker {
    FuriThread* thread;
    FuriStreamBuffer* stream;

    NfcWorkerState state;
    NfcWorkerCallback callback;
    void* context;

    Nfc* nfc;
    NfcPoller* poller;
    NfcApduScript* script;
    NfcApduResponse* responses;
    uint32_t response_count;

    bool running;
    bool card_detected;
    bool card_lost;

    char* error_message;
};

// 记录APDU数据的辅助函数
static void
    nfc_worker_log_apdu_data(const char* cmd, const uint8_t* response, uint16_t response_len) {
    FURI_LOG_I(TAG, "APDU命令: %s", cmd);
    if(response && response_len > 0) {
        FuriString* resp_str = furi_string_alloc();
        for(size_t i = 0; i < response_len; i++) {
            furi_string_cat_printf(resp_str, "%02X", response[i]);
        }
        FURI_LOG_I(TAG, "APDU响应: %s", furi_string_get_cstr(resp_str));
        furi_string_free(resp_str);
    } else {
        FURI_LOG_I(TAG, "APDU响应: 无");
    }
}

// NFC轮询器回调函数
static NfcCommand nfc_worker_poller_callback(NfcGenericEvent event, void* context) {
    NfcWorker* worker = context;

    // 检查事件类型
    if(event.protocol == NfcProtocolIso14443_4a) {
        const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;

        if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
            FURI_LOG_I(TAG, "ISO14443-4A卡检测成功");
            // 只有当脚本中指定的卡类型是ISO14443-4A时，才认为检测到了卡片
            if(worker->script && worker->script->card_type == CardTypeIso14443_4a &&
               !worker->card_detected) {
                worker->card_detected = true;
                worker->callback(NfcWorkerEventCardDetected, worker->context);
                // 检测到卡片后立即返回停止命令，避免重复触发
                return NfcCommandStop;
            } else if(worker->card_detected) {
                // 如果已经检测到卡片，不再重复触发回调
                FURI_LOG_D(TAG, "已经检测到ISO14443-4A卡，忽略重复事件");
            } else {
                FURI_LOG_I(TAG, "检测到ISO14443-4A卡，但脚本要求的是其他类型");
            }
        } else if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeError) {
            FURI_LOG_E(TAG, "ISO14443-4A卡检测失败，错误码: %d", iso14443_4a_event->data->error);
        }
    } else if(event.protocol == NfcProtocolIso14443_4b) {
        // 处理ISO14443-4B协议事件
        // 注意：这里假设ISO14443-4B的事件结构与ISO14443-4A类似
        const Iso14443_4bPollerEvent* iso14443_4b_event = event.event_data;

        if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeReady) {
            FURI_LOG_I(TAG, "ISO14443-4B卡检测成功");
            // 只有当脚本中指定的卡类型是ISO14443-4B时，才认为检测到了卡片
            if(worker->script && worker->script->card_type == CardTypeIso14443_4b &&
               !worker->card_detected) {
                worker->card_detected = true;
                worker->callback(NfcWorkerEventCardDetected, worker->context);
                // 检测到卡片后立即返回停止命令，避免重复触发
                return NfcCommandStop;
            } else if(worker->card_detected) {
                // 如果已经检测到卡片，不再重复触发回调
                FURI_LOG_D(TAG, "已经检测到ISO14443-4B卡，忽略重复事件");
            } else {
                FURI_LOG_I(TAG, "检测到ISO14443-4B卡，但脚本要求的是其他类型");
            }
        } else if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeError) {
            FURI_LOG_E(TAG, "ISO14443-4B卡检测失败，错误码: %d", iso14443_4b_event->data->error);
        }
    } else {
        FURI_LOG_I(TAG, "收到未知协议事件: %d", event.protocol);
    }

    // 继续操作
    return NfcCommandContinue;
}

// 执行APDU命令的回调函数
static NfcCommand nfc_worker_apdu_poller_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    furi_assert(context);

    ApduContext* apdu_ctx = (ApduContext*)context;

    // 如果准备好发送，则处理命令
    if(apdu_ctx->ready) {
        // 设置为未准备好，避免重复处理
        apdu_ctx->ready = false;

        FURI_LOG_I(TAG, "APDU回调函数处理命令");

        // 根据协议类型发送数据块
        if(event.protocol == NfcProtocolIso14443_4a) {
            // 发送数据块
            Iso14443_4aError error = iso14443_4a_poller_send_block(
                (Iso14443_4aPoller*)event.instance, apdu_ctx->tx_buffer, apdu_ctx->rx_buffer);

            if(error == Iso14443_4aErrorNone) {
                // 发送成功，通知线程
                furi_thread_flags_set(apdu_ctx->thread_id, APDU_POLLER_DONE);

                // 如果是最后一条指令，停止轮询器
                if(apdu_ctx->is_last) {
                    return NfcCommandStop;
                }

                return NfcCommandContinue;
            } else {
                // 发送失败，通知线程
                FURI_LOG_E(TAG, "ISO14443-4A命令执行失败，错误码: %d", error);
                furi_thread_flags_set(apdu_ctx->thread_id, APDU_POLLER_ERR);
                return NfcCommandStop;
            }
        } else if(event.protocol == NfcProtocolIso14443_4b) {
            // 发送数据块
            Iso14443_4bError error = iso14443_4b_poller_send_block(
                (Iso14443_4bPoller*)event.instance, apdu_ctx->tx_buffer, apdu_ctx->rx_buffer);

            if(error == Iso14443_4bErrorNone) {
                // 发送成功，通知线程
                furi_thread_flags_set(apdu_ctx->thread_id, APDU_POLLER_DONE);

                // 如果是最后一条指令，停止轮询器
                if(apdu_ctx->is_last) {
                    return NfcCommandStop;
                }

                return NfcCommandContinue;
            } else {
                // 发送失败，通知线程
                FURI_LOG_E(TAG, "ISO14443-4B命令执行失败，错误码: %d", error);
                furi_thread_flags_set(apdu_ctx->thread_id, APDU_POLLER_ERR);
                return NfcCommandStop;
            }
        } else {
            // 不支持的协议
            FURI_LOG_E(TAG, "不支持的协议: %d", event.protocol);
            furi_thread_flags_set(apdu_ctx->thread_id, APDU_POLLER_ERR);
            return NfcCommandStop;
        }
    } else {
        // 未准备好，等待
        furi_delay_ms(10);
    }

    return NfcCommandContinue;
}

// 执行APDU命令的线程函数
static int32_t nfc_worker_apdu_thread(void* context) {
    NfcWorker* worker = context;
    FURI_LOG_I(TAG, "APDU执行线程启动");

    // 根据脚本中指定的卡类型创建相应的轮询器
    NfcProtocol protocol;
    if(worker->script) {
        switch(worker->script->card_type) {
        case CardTypeIso14443_3a:
            protocol = NfcProtocolIso14443_3a;
            FURI_LOG_I(TAG, "使用ISO14443-3A协议");
            break;
        case CardTypeIso14443_3b:
            protocol = NfcProtocolIso14443_3b;
            FURI_LOG_I(TAG, "使用ISO14443-3B协议");
            break;
        case CardTypeIso14443_4a:
            protocol = NfcProtocolIso14443_4a;
            FURI_LOG_I(TAG, "使用ISO14443-4A协议");
            break;
        case CardTypeIso14443_4b:
            protocol = NfcProtocolIso14443_4b;
            FURI_LOG_I(TAG, "使用ISO14443-4B协议");
            break;
        default:
            FURI_LOG_E(TAG, "不支持的卡类型: %d", worker->script->card_type);
            worker->callback(NfcWorkerEventFail, worker->context);
            return -1;
        }
    } else {
        // 如果没有脚本，默认使用ISO14443-4A
        protocol = NfcProtocolIso14443_4a;
        FURI_LOG_I(TAG, "没有指定脚本，默认使用ISO14443-4A协议");
    }

    // 创建新的轮询器用于执行APDU命令
    worker->poller = nfc_poller_alloc(worker->nfc, protocol);
    if(!worker->poller) {
        FURI_LOG_E(TAG, "分配APDU执行轮询器失败，协议: %d", protocol);
        worker->callback(NfcWorkerEventFail, worker->context);
        return -1;
    }

    FURI_LOG_I(TAG, "APDU执行轮询器分配成功");

    // 创建APDU上下文
    ApduContext apdu_ctx;
    memset(&apdu_ctx, 0, sizeof(ApduContext));
    apdu_ctx.tx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);
    apdu_ctx.rx_buffer = bit_buffer_alloc(MAX_APDU_LENGTH * 8);
    apdu_ctx.ready = false;
    apdu_ctx.is_last = false;
    apdu_ctx.thread_id = furi_thread_get_current_id();

    // 分配响应内存
    worker->responses = malloc(sizeof(NfcApduResponse) * worker->script->command_count);
    if(!worker->responses) {
        FURI_LOG_E(TAG, "分配响应内存失败");
        worker->callback(NfcWorkerEventFail, worker->context);

        // 释放资源
        bit_buffer_free(apdu_ctx.tx_buffer);
        bit_buffer_free(apdu_ctx.rx_buffer);

        // 停止轮询器
        if(worker->poller) {
            nfc_poller_stop(worker->poller);
            nfc_poller_free(worker->poller);
            worker->poller = NULL;
        }

        return -1;
    }

    memset(worker->responses, 0, sizeof(NfcApduResponse) * worker->script->command_count);

    // 启动轮询器
    nfc_poller_start(worker->poller, nfc_worker_apdu_poller_callback, &apdu_ctx);

    FURI_LOG_I(TAG, "APDU执行轮询器启动成功");

    // 等待轮询器初始化完成
    furi_delay_ms(100);

    FURI_LOG_I(TAG, "开始执行APDU命令，共 %lu 条", worker->script->command_count);

    bool is_error = false;
    uint32_t response_count = 0;

    // 循环执行每条APDU命令
    for(uint32_t i = 0; i < worker->script->command_count && worker->running; i++) {
        const char* cmd = worker->script->commands[i];
        FURI_LOG_I(TAG, "准备执行APDU命令 %lu: %s", i, cmd);

        // 检查命令长度是否超过限制
        size_t cmd_len = strlen(cmd);
        if(cmd_len / 2 > MAX_APDU_LENGTH) {
            FURI_LOG_E(
                TAG, "APDU命令长度超过限制(%zu > %d): %s", cmd_len / 2, MAX_APDU_LENGTH, cmd);

            // 设置错误信息
            worker->error_message = malloc(100);
            if(worker->error_message) {
                snprintf(
                    worker->error_message,
                    100,
                    "Command too long\n%d bytes max\nCommand: %lu",
                    MAX_APDU_LENGTH,
                    i + 1);
            }

            is_error = true;
            // 立即调用失败回调，而不是等到线程结束
            worker->callback(NfcWorkerEventFail, worker->context);
            break;
        }

        // 解析APDU命令
        uint8_t apdu_data[MAX_APDU_LENGTH];
        uint16_t apdu_len = 0;

        // 将十六进制字符串转换为字节数组
        for(size_t j = 0; j < cmd_len; j += 2) {
            if(j + 1 >= cmd_len) break;

            char hex[3] = {cmd[j], cmd[j + 1], '\0'};
            apdu_data[apdu_len++] = (uint8_t)strtol(hex, NULL, 16);

            if(apdu_len >= MAX_APDU_LENGTH) break;
        }

        // 清空缓冲区
        bit_buffer_reset(apdu_ctx.tx_buffer);
        bit_buffer_reset(apdu_ctx.rx_buffer);

        // 将APDU数据复制到发送缓冲区
        bit_buffer_copy_bytes(apdu_ctx.tx_buffer, apdu_data, apdu_len);

        // 保存命令
        worker->responses[response_count].command = strdup(cmd);

        // 设置是否是最后一条指令
        apdu_ctx.is_last = (i == worker->script->command_count - 1);

        // 设置准备好发送
        apdu_ctx.ready = true;

        // 等待命令执行完成或出错
        uint32_t flags =
            furi_thread_flags_wait(APDU_POLLER_DONE | APDU_POLLER_ERR, FuriFlagWaitAny, 3000);

        if(flags & APDU_POLLER_ERR) {
            // 命令执行出错
            FURI_LOG_E(TAG, "APDU命令执行出错");

            // 设置错误信息
            worker->error_message = malloc(100);
            if(worker->error_message) {
                snprintf(worker->error_message, 100, "APDU command failed\nCommand: %lu", i + 1);
            }

            worker->responses[response_count].response = NULL;
            worker->responses[response_count].response_length = 0;
            is_error = true;
            // 立即调用失败回调，而不是等到线程结束
            worker->callback(NfcWorkerEventFail, worker->context);
            response_count++;
            break;
        } else if(flags & APDU_POLLER_DONE) {
            // 命令执行成功
            size_t rx_bytes_count = bit_buffer_get_size_bytes(apdu_ctx.rx_buffer);

            if(rx_bytes_count > 0) {
                // 分配响应内存
                uint8_t* response = malloc(rx_bytes_count);
                if(!response) {
                    FURI_LOG_E(TAG, "分配响应内存失败");
                    worker->responses[response_count].response = NULL;
                    worker->responses[response_count].response_length = 0;
                    is_error = true;
                    response_count++;
                    break;
                }

                // 复制响应数据
                bit_buffer_write_bytes(apdu_ctx.rx_buffer, response, rx_bytes_count);

                // 保存响应
                worker->responses[response_count].response = response;
                worker->responses[response_count].response_length = rx_bytes_count;

                // 记录APDU命令和响应
                nfc_worker_log_apdu_data(cmd, response, rx_bytes_count);

                FURI_LOG_I(TAG, "APDU命令 %lu 执行成功", i);
            } else {
                // 没有响应
                FURI_LOG_W(TAG, "APDU命令 %lu 执行成功，但没有响应", i);
                worker->responses[response_count].response = NULL;
                worker->responses[response_count].response_length = 0;
            }

            response_count++;
        } else {
            // 超时
            FURI_LOG_E(TAG, "APDU命令执行超时");

            // 设置错误信息
            worker->error_message = malloc(100);
            if(worker->error_message) {
                snprintf(worker->error_message, 100, "Command timeout\nCommand: %lu", i + 1);
            }

            worker->responses[response_count].response = NULL;
            worker->responses[response_count].response_length = 0;
            is_error = true;
            // 立即调用失败回调，而不是等到线程结束
            worker->callback(NfcWorkerEventFail, worker->context);
            response_count++;
            break;
        }

        // 命令之间添加短暂延迟
        furi_delay_ms(50);

        // 检查是否应该退出
        if(!worker->running) {
            FURI_LOG_I(TAG, "用户取消操作");
            is_error = true;
            break;
        }
    }

    // 更新响应计数
    worker->response_count = response_count;

    // 释放资源
    bit_buffer_free(apdu_ctx.tx_buffer);
    bit_buffer_free(apdu_ctx.rx_buffer);

    // 停止轮询器
    if(worker->poller) {
        FURI_LOG_I(TAG, "APDU命令执行完成，停止轮询器");
        nfc_poller_stop(worker->poller);
        nfc_poller_free(worker->poller);
        worker->poller = NULL;
    }

    // 返回结果
    if(!worker->running && !is_error) {
        // 只有在用户取消操作且没有其他错误时才触发中止事件
        FURI_LOG_I(TAG, "用户取消操作");
        worker->callback(NfcWorkerEventAborted, worker->context);
        return -1;
    } else if(!is_error) {
        // 没有错误时触发成功事件
        FURI_LOG_I(TAG, "执行成功");
        worker->callback(NfcWorkerEventSuccess, worker->context);
        return 0;
    } else {
        // 错误已经在上面触发了回调，这里不需要再次触发
        FURI_LOG_E(TAG, "执行失败");
        return -1;
    }
}

// 检测卡片的线程函数
static int32_t nfc_worker_detect_thread(void* context) {
    NfcWorker* worker = context;

    FURI_LOG_I(TAG, "NFC Worker线程启动");

    // 根据脚本中指定的卡类型创建相应的轮询器
    NfcProtocol protocol;
    if(worker->script) {
        switch(worker->script->card_type) {
        case CardTypeIso14443_3a:
            protocol = NfcProtocolIso14443_3a;
            FURI_LOG_I(TAG, "使用ISO14443-3A协议");
            break;
        case CardTypeIso14443_3b:
            protocol = NfcProtocolIso14443_3b;
            FURI_LOG_I(TAG, "使用ISO14443-3B协议");
            break;
        case CardTypeIso14443_4a:
            protocol = NfcProtocolIso14443_4a;
            FURI_LOG_I(TAG, "使用ISO14443-4A协议");
            break;
        case CardTypeIso14443_4b:
            protocol = NfcProtocolIso14443_4b;
            FURI_LOG_I(TAG, "使用ISO14443-4B协议");
            break;
        default:
            FURI_LOG_E(TAG, "不支持的卡类型: %d", worker->script->card_type);
            worker->callback(NfcWorkerEventFail, worker->context);
            return -1;
        }
    } else {
        // 如果没有脚本，默认使用ISO14443-4A
        protocol = NfcProtocolIso14443_4a;
        FURI_LOG_I(TAG, "没有指定脚本，默认使用ISO14443-4A协议");
    }

    // 重置卡片检测标志
    worker->card_detected = false;

    // 创建NFC轮询器用于检测卡片
    worker->poller = nfc_poller_alloc(worker->nfc, protocol);
    if(!worker->poller) {
        FURI_LOG_E(TAG, "分配轮询器失败，协议: %d", protocol);
        worker->callback(NfcWorkerEventFail, worker->context);
        return -1;
    }

    FURI_LOG_I(TAG, "NFC轮询器分配成功");

    // 启动轮询器，使用回调
    nfc_poller_start(worker->poller, nfc_worker_poller_callback, worker);

    FURI_LOG_I(TAG, "NFC轮询器启动成功");

    // 等待卡片，持续检测直到找到卡片或用户取消
    FURI_LOG_I(TAG, "等待卡片");

    // 持续等待，直到检测到卡片或用户取消
    uint32_t attempt = 0;
    while(!worker->card_detected && worker->running) {
        attempt++;
        if(attempt % 5 == 0) {
            FURI_LOG_I(TAG, "等待卡片中... (%lu秒)", attempt / 2);
            // 通知用户放置卡片
            worker->callback(NfcWorkerEventCardLost, worker->context);
        }

        // 使用延迟来等待卡片
        furi_delay_ms(500);

        // 检查是否应该退出
        if(!worker->running) {
            FURI_LOG_I(TAG, "用户取消操作");
            // 只有在没有错误信息的情况下才触发中止回调
            if(!worker->error_message) {
                worker->callback(NfcWorkerEventAborted, worker->context);
            }

            // 停止轮询器
            if(worker->poller) {
                nfc_poller_stop(worker->poller);
                nfc_poller_free(worker->poller);
                worker->poller = NULL;
            }

            return -1;
        }
    }

    // 如果没有检测到卡片，通知用户
    if(!worker->card_detected) {
        FURI_LOG_E(TAG, "未检测到卡片");
        worker->callback(NfcWorkerEventFail, worker->context);

        // 停止轮询器
        if(worker->poller) {
            nfc_poller_stop(worker->poller);
            nfc_poller_free(worker->poller);
            worker->poller = NULL;
        }

        return -1;
    }

    // 检测到卡片后，立即停止检测轮询器
    if(worker->poller) {
        FURI_LOG_I(TAG, "检测到卡片，停止检测轮询器");
        nfc_poller_stop(worker->poller);
        nfc_poller_free(worker->poller);
        worker->poller = NULL;
    }

    // 如果只是检测卡片，到这里就完成了
    if(worker->state == NfcWorkerStateDetecting) {
        worker->callback(NfcWorkerEventSuccess, worker->context);
        return 0;
    }

    // 如果需要执行APDU命令，继续执行
    if(worker->state == NfcWorkerStateRunning && worker->card_detected) {
        // 短暂延迟，确保之前的轮询器完全停止
        furi_delay_ms(500);

        // 启动APDU执行线程
        FuriThread* apdu_thread =
            furi_thread_alloc_ex("NfcWorkerAPDUThread", 8192, nfc_worker_apdu_thread, worker);
        furi_thread_start(apdu_thread);

        // 等待APDU执行线程完成
        furi_thread_join(apdu_thread);
        furi_thread_free(apdu_thread);

        // 返回结果
        if(!worker->running) {
            FURI_LOG_I(TAG, "用户取消操作");
            // 只有在没有错误信息的情况下才触发中止回调
            if(!worker->error_message) {
                worker->callback(NfcWorkerEventAborted, worker->context);
            }
            return -1;
        } else {
            FURI_LOG_I(TAG, "执行成功");
            worker->callback(NfcWorkerEventSuccess, worker->context);
            return 0;
        }
    }

    return 0;
}

// 分配NFC Worker
NfcWorker* nfc_worker_alloc(Nfc* nfc) {
    NfcWorker* worker = malloc(sizeof(NfcWorker));

    worker->thread = NULL;
    worker->stream = furi_stream_buffer_alloc(sizeof(NfcWorkerEvent), 8);
    worker->nfc = nfc;
    worker->poller = NULL;
    worker->script = NULL;
    worker->responses = NULL;
    worker->response_count = 0;
    worker->running = false;
    worker->card_detected = false;
    worker->card_lost = false;
    worker->error_message = NULL;

    return worker;
}

// 释放NFC Worker
void nfc_worker_free(NfcWorker* worker) {
    furi_assert(worker);

    // 确保先停止工作线程
    nfc_worker_stop(worker);

    // 释放响应资源
    if(worker->responses) {
        for(uint32_t i = 0; i < worker->response_count; i++) {
            if(worker->responses[i].command) {
                free(worker->responses[i].command);
            }
            if(worker->responses[i].response) {
                free(worker->responses[i].response);
            }
        }
        free(worker->responses);
        worker->responses = NULL;
        worker->response_count = 0;
    }

    // 释放错误信息
    if(worker->error_message) {
        free(worker->error_message);
        worker->error_message = NULL;
    }

    // 释放流缓冲区
    if(worker->stream) {
        furi_stream_buffer_free(worker->stream);
        worker->stream = NULL;
    }

    // 释放工作线程
    if(worker->thread) {
        furi_thread_free(worker->thread);
        worker->thread = NULL;
    }

    // 释放工作器本身
    free(worker);
}

// 启动NFC Worker
void nfc_worker_start(
    NfcWorker* worker,
    NfcWorkerState state,
    NfcApduScript* script,
    NfcWorkerCallback callback,
    void* context) {
    furi_assert(worker);
    furi_assert(callback);

    worker->state = state;
    worker->script = script;
    worker->callback = callback;
    worker->context = context;
    worker->running = true;
    worker->card_detected = false;
    worker->card_lost = false;

    worker->thread = furi_thread_alloc_ex("NfcWorker", 8192, nfc_worker_detect_thread, worker);
    furi_thread_start(worker->thread);
}

// 停止NFC Worker
void nfc_worker_stop(NfcWorker* worker) {
    furi_assert(worker);

    if(!worker->running) {
        // 已经停止，无需再次停止
        return;
    }

    // 设置运行标志为false，通知线程退出
    worker->running = false;

    // 等待线程退出
    if(worker->thread) {
        furi_thread_join(worker->thread);
        furi_thread_free(worker->thread);
        worker->thread = NULL;
    }

    // 确保轮询器被正确释放
    if(worker->poller) {
        // 先停止轮询器
        nfc_poller_stop(worker->poller);
        // 短暂延迟，确保轮询器完全停止
        furi_delay_ms(10);
        // 然后释放轮询器
        nfc_poller_free(worker->poller);
        worker->poller = NULL;
    }

    // 重置状态
    worker->card_detected = false;
    worker->card_lost = false;
    worker->state = NfcWorkerStateReady;

    // 如果有错误信息，说明已经触发了错误回调，不需要再触发中止回调
    if(worker->callback && !worker->error_message) {
        worker->callback(NfcWorkerEventAborted, worker->context);
    }

    worker->callback = NULL;
    worker->context = NULL;
}

// 检查NFC Worker是否正在运行
bool nfc_worker_is_running(NfcWorker* worker) {
    furi_assert(worker);
    return worker->running;
}

// 获取NFC Worker的响应结果
void nfc_worker_get_responses(
    NfcWorker* worker,
    NfcApduResponse** out_responses,
    uint32_t* out_response_count) {
    furi_assert(worker);
    furi_assert(out_responses);
    furi_assert(out_response_count);

    *out_responses = worker->responses;
    *out_response_count = worker->response_count;

    // 清空worker中的响应，避免重复释放
    worker->responses = NULL;
    worker->response_count = 0;
}

// 获取NFC Worker的错误信息
const char* nfc_worker_get_error_message(NfcWorker* worker) {
    furi_assert(worker);
    return worker->error_message;
}
