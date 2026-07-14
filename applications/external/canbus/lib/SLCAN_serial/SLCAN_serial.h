#pragma once

#include <furi_hal.h>
#include <furi_hal_usb_cdc.h>

#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#include <frame_can.h>

#define SLCAN_CDC_NUM 1

bool SLCAN_open_port(void);
bool SLCAN_close_port(void);

void SLCAN_send_frame(FrameCAN* frame, bool* send_timestamp);
void SLCAN_send_log(Storage* storage, FuriString* log_path, bool* send_timestamp);
