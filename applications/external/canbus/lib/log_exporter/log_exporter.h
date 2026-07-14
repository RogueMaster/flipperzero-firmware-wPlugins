#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#include <frame_can.h>

void export_log_as_csv(Storage* storage, FuriString* log_paht, FuriString* out_path);
