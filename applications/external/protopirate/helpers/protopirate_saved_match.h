#pragma once

#include <furi.h>
#include <flipper_format/flipper_format.h>

bool protopirate_saved_match_signal(
    FlipperFormat* received_ff,
    FuriString* out_matched_name,
    FuriString* out_matched_path);
