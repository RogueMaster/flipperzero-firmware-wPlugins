// views/protopirate_receiver.h
#pragma once

#include <gui/view.h>
#include "../helpers/protopirate_types.h"

typedef struct ProtoPirateReceiver ProtoPirateReceiver;
typedef struct ProtoPirateHistory ProtoPirateHistory;

typedef void (*ProtoPirateReceiverCallback)(ProtoPirateCustomEvent event, void* context);

void protopirate_view_receiver_set_callback(
    ProtoPirateReceiver* receiver,
    ProtoPirateReceiverCallback callback,
    void* context);

ProtoPirateReceiver* protopirate_view_receiver_alloc(bool auto_save);
void protopirate_view_receiver_free(ProtoPirateReceiver* receiver);
View* protopirate_view_receiver_get_view(ProtoPirateReceiver* receiver);

void protopirate_view_receiver_add_item_to_menu(
    ProtoPirateReceiver* receiver,
    const char* name,
    uint8_t type);

void protopirate_view_receiver_add_data_statusbar(
    ProtoPirateReceiver* receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external_radio);

uint16_t protopirate_view_receiver_get_idx_menu(ProtoPirateReceiver* receiver);
void protopirate_view_receiver_set_idx_menu(ProtoPirateReceiver* receiver, uint16_t idx);
void protopirate_view_receiver_set_rssi(ProtoPirateReceiver* receiver, float rssi);
void protopirate_view_receiver_set_lock(ProtoPirateReceiver* receiver, ProtoPirateLock lock);
void protopirate_view_receiver_set_autosave(ProtoPirateReceiver* receiver, bool auto_save);
void protopirate_view_receiver_set_sub_decode_mode(
    ProtoPirateReceiver* receiver,
    bool sub_decode_mode);
void protopirate_view_receiver_reset_menu(ProtoPirateReceiver* receiver);

void protopirate_view_receiver_sync_menu_from_history(
    ProtoPirateReceiver* receiver,
    ProtoPirateHistory* history);

void protopirate_view_receiver_pop_first_menu_item(ProtoPirateReceiver* receiver);
void protopirate_view_receiver_delete_item(ProtoPirateReceiver* receiver, uint16_t idx);

void protopirate_view_receiver_append_menu_row_from_history(
    ProtoPirateReceiver* receiver,
    ProtoPirateHistory* history,
    uint16_t idx);
