// protopirate_history.h
#pragma once

#include <stddef.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/protocols/base.h>

#define PROTOPIRATE_HISTORY_MAX 20

typedef struct SubGhzEnvironment SubGhzEnvironment;
typedef struct ProtoPirateHistory ProtoPirateHistory;

ProtoPirateHistory* protopirate_history_alloc(void);
void protopirate_history_free(ProtoPirateHistory* instance);
void protopirate_history_reset(ProtoPirateHistory* instance);
uint16_t protopirate_history_get_item(ProtoPirateHistory* instance);
uint16_t protopirate_history_get_last_index(ProtoPirateHistory* instance);
void protopirate_history_format_status_text(
    ProtoPirateHistory* instance,
    char* output,
    size_t output_size);
void protopirate_history_get_status_text(ProtoPirateHistory* instance, FuriString* output);

bool protopirate_history_get_capture_path(
    ProtoPirateHistory* instance,
    uint16_t idx,
    FuriString* out_path);
bool protopirate_history_capture_path_equals(
    ProtoPirateHistory* instance,
    uint16_t idx,
    const char* path);

bool protopirate_history_add_to_history(
    ProtoPirateHistory* instance,
    void* context,
    SubGhzRadioPreset* preset);
bool protopirate_history_add_to_history_at(
    ProtoPirateHistory* instance,
    void* context,
    SubGhzRadioPreset* preset,
    uint32_t update_timestamp);
void protopirate_history_delete_item(ProtoPirateHistory* instance, uint16_t idx);
void protopirate_history_get_text_item_menu(
    ProtoPirateHistory* instance,
    FuriString* output,
    uint16_t idx);
void protopirate_history_get_text_item_detail(
    ProtoPirateHistory* instance,
    uint16_t idx,
    FuriString* output,
    SubGhzEnvironment* environment);
FlipperFormat* protopirate_history_get_raw_data(ProtoPirateHistory* instance, uint16_t idx);

void protopirate_history_release_scratch(ProtoPirateHistory* instance);

void protopirate_history_set_item_str(ProtoPirateHistory* instance, uint16_t idx, const char* str);

void protopirate_history_set_matched_saved(
    ProtoPirateHistory* instance,
    uint16_t idx,
    const char* name,
    const char* path);

const char* protopirate_history_get_matched_saved_path(ProtoPirateHistory* instance, uint16_t idx);

const char* protopirate_history_get_matched_name(ProtoPirateHistory* instance, uint16_t idx);

bool protopirate_history_has_matched_saved(ProtoPirateHistory* instance, uint16_t idx);

void protopirate_history_mark_auto_save_pending(ProtoPirateHistory* instance, uint16_t idx);
bool protopirate_history_find_pending_auto_save(ProtoPirateHistory* instance, uint16_t* idx);
void protopirate_history_mark_auto_save_done(ProtoPirateHistory* instance, uint16_t idx);
void protopirate_history_mark_saved_match_pending(ProtoPirateHistory* instance, uint16_t idx);
bool protopirate_history_find_pending_saved_match(ProtoPirateHistory* instance, uint16_t* idx);
void protopirate_history_mark_saved_match_done(ProtoPirateHistory* instance, uint16_t idx);
