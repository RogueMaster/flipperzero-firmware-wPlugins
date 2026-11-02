#include "fn_info.h"

#include <stdlib.h>
#include <string.h>

void fn_info_reset(FnInfo* info) {
    if(!info) return;
    memset(info, 0, sizeof(FnInfo));
    info->status.stage = FnStageUnknown;
    info->status.current_doc = FnDocNone;
}

FnInfo* fn_info_alloc(void) {
    FnInfo* info = malloc(sizeof(FnInfo));
    if(info) fn_info_reset(info);
    return info;
}

void fn_info_free(FnInfo* info) {
    free(info);
}
