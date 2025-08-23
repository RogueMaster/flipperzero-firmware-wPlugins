#pragma once
// #include "../subghz_i.h"
#include <lib/subghz/devices/devices.h>
#include <toolbox/pipe.h>

typedef struct SubGhzChatWorker SubGhzChatWorker;

typedef enum {
    SubGhzChatEventNoEvent,
    SubGhzChatEventUserEntrance,
    SubGhzChatEventUserExit,
    SubGhzChatEventInputData,
    SubGhzChatEventRXData,
    SubGhzChatEventNewMessage,
} SubGhzChatEventType;

typedef struct {
    SubGhzChatEventType event;
    char c;
} SubGhzChatEvent;

// TODO: check actual usage

SubGhzChatWorker* subghz_chat_worker_alloc();
void subghz_chat_worker_free(SubGhzChatWorker* instance);
bool subghz_chat_worker_start(
    SubGhzChatWorker* instance,
    const SubGhzDevice* device,
    uint32_t frequency);
void subghz_chat_worker_stop(SubGhzChatWorker* instance);
bool subghz_chat_worker_is_running(SubGhzChatWorker* instance);
SubGhzChatEvent subghz_chat_worker_get_event_chat(SubGhzChatWorker* instance);
void subghz_chat_worker_put_event_chat(SubGhzChatWorker* instance, SubGhzChatEvent* event);
size_t subghz_chat_worker_available(SubGhzChatWorker* instance);
size_t subghz_chat_worker_read(SubGhzChatWorker* instance, uint8_t* data, size_t size);
bool subghz_chat_worker_write(SubGhzChatWorker* instance, uint8_t* data, size_t size);

const SubGhzDevice* subghz_cli_command_get_device(uint32_t* device_ind);

void subghz_cli_radio_device_power_on(void);
void subghz_cli_radio_device_power_off(void);

uint8_t ss_subghz_init(void);
uint8_t ss_subghz_deinit(void);
uint8_t subghz_share_send(uint8_t* data, size_t size);
