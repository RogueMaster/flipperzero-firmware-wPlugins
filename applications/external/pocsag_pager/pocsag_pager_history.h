#pragma once

#include <furi.h>
#include <lib/subghz/types.h>

typedef struct PCSGHistory PCSGHistory;

typedef enum {
    PCSGHistoryStateAddKeyUnknown,
    PCSGHistoryStateAddKeyTimeOut,
    PCSGHistoryStateAddKeyNewDada,
    PCSGHistoryStateAddKeyUpdateData,
    PCSGHistoryStateAddKeyOverflow,
} PCSGHistoryStateAddKey;

PCSGHistory* pcsg_history_alloc(void);
void pcsg_history_free(PCSGHistory* instance);
void pcsg_history_reset(PCSGHistory* instance);

uint16_t pcsg_history_get_item(PCSGHistory* instance);
uint8_t pcsg_history_get_type_protocol(PCSGHistory* instance, uint16_t idx);
uint32_t pcsg_history_get_frequency(PCSGHistory* instance, uint16_t idx);
bool pcsg_history_get_text_space_left(PCSGHistory* instance, FuriString* output);
void pcsg_history_get_text_item_menu(PCSGHistory* instance, FuriString* output, uint16_t idx);

/** Offload full decoded message to microSD and keep only a compact preview in RAM. */
PCSGHistoryStateAddKey
    pcsg_history_add_to_history(PCSGHistory* instance, void* context, SubGhzRadioPreset* preset);

/** Load full message header + body on demand from microSD for display in the scrollable view. */
bool pcsg_history_load_message(
    PCSGHistory* instance,
    uint16_t idx,
    FuriString* out_header,
    FuriString* out_body);
