#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board_power_lifecycle.h"

typedef enum {
    SeaderStartupStageNone = 0,
    SeaderStartupStageCheckingSam,
    SeaderStartupStageRetryingBoard,
} SeaderStartupStage;

const char* seader_startup_stage_header(SeaderStartupStage stage);
const char* seader_startup_stage_text(SeaderStartupStage stage);

const char* seader_board_status_detail_title(SeaderBoardStatus status);
const char* seader_board_status_detail_body(SeaderBoardStatus status, bool retry_exhausted);
const char* seader_board_status_detail_hint(SeaderBoardStatus status);

size_t seader_format_atr_summary(const uint8_t* atr, size_t len, char* out, size_t out_size);
