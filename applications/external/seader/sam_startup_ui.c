#include "sam_startup_ui.h"

#include <stdio.h>

const char* seader_startup_stage_header(SeaderStartupStage stage) {
    switch(stage) {
    case SeaderStartupStageRetryingBoard:
        return "Retrying Board";
    case SeaderStartupStageCheckingSam:
        return "Checking SAM";
    case SeaderStartupStageNone:
    default:
        return "Starting";
    }
}

const char* seader_startup_stage_text(SeaderStartupStage stage) {
    switch(stage) {
    case SeaderStartupStageRetryingBoard:
        return "Power cycle\nand retry";
    case SeaderStartupStageCheckingSam:
        return "Waiting for\nCCID/SAM";
    case SeaderStartupStageNone:
    default:
        return NULL;
    }
}

const char* seader_board_status_detail_title(SeaderBoardStatus status) {
    switch(status) {
    case SeaderBoardStatusFaultPreEnable:
    case SeaderBoardStatusFaultPostEnable:
        return "Board Fault";
    case SeaderBoardStatusNoResponse:
        return "No Response";
    case SeaderBoardStatusPowerLost:
        return "Power Lost";
    case SeaderBoardStatusRetryRequested:
        return "Retry Board";
    case SeaderBoardStatusPowerReadyPendingValidation:
        return "Checking SAM";
    case SeaderBoardStatusReady:
    case SeaderBoardStatusUnknown:
    default:
        return "No SAM Found";
    }
}

const char* seader_board_status_detail_body(SeaderBoardStatus status, bool retry_exhausted) {
    switch(status) {
    case SeaderBoardStatusFaultPreEnable:
        return "5V fault before\nenable";
    case SeaderBoardStatusFaultPostEnable:
        return "5V fault after\nenable";
    case SeaderBoardStatusNoResponse:
        return retry_exhausted ? "Board powered,\nno SAM after retry" :
                                 "Board powered,\nno CCID/SAM reply";
    case SeaderBoardStatusPowerLost:
        return "USB/5V removed\nboard unpowered";
    case SeaderBoardStatusRetryRequested:
        return "Power cycle the\nboard and retry";
    case SeaderBoardStatusPowerReadyPendingValidation:
        return "Board powered,\nchecking SAM";
    case SeaderBoardStatusReady:
    case SeaderBoardStatusUnknown:
    default:
        return "No SAM detected\non the board";
    }
}

const char* seader_board_status_detail_hint(SeaderBoardStatus status) {
    switch(status) {
    case SeaderBoardStatusFaultPreEnable:
    case SeaderBoardStatusFaultPostEnable:
        return "Check board/cable";
    case SeaderBoardStatusNoResponse:
        return "Reseat board/SAM";
    case SeaderBoardStatusPowerLost:
        return "Reconnect power";
    case SeaderBoardStatusRetryRequested:
        return "Retry bus check";
    case SeaderBoardStatusPowerReadyPendingValidation:
        return "Wait for SAM";
    case SeaderBoardStatusReady:
    case SeaderBoardStatusUnknown:
    default:
        return "Insert supported SAM";
    }
}

size_t seader_format_atr_summary(const uint8_t* atr, size_t len, char* out, size_t out_size) {
    if(!out || out_size == 0U) {
        return 0U;
    }

    if(!atr || len == 0U) {
        return (size_t)snprintf(out, out_size, "ATR: unavailable");
    }

    const size_t shown = len < 6U ? len : 6U;
    size_t used = (size_t)snprintf(out, out_size, "ATR:");
    for(size_t i = 0; i < shown && used + 4U < out_size; i++) {
        used += (size_t)snprintf(out + used, out_size - used, " %02X", atr[i]);
    }

    if(len > shown && used + 4U < out_size) {
        used += (size_t)snprintf(out + used, out_size - used, "...");
    }

    return used;
}
