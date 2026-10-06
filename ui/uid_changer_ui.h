#pragma once

#include "../nfc/magic_tag.h"
#include <gui/view.h>

typedef enum {
    UidPageSource,
    UidPageManualProtocol,
    UidPageManualLength,
    UidPageGen4Target,
    UidPageReading,
    UidPageReadResult,
    UidPageEdit,
    UidPageBlock0Edit,
    UidPageMagic,
    UidPageWrite,
} UidPage;

typedef struct {
    uint8_t step;
    UidPage page;
    char title[24];
    char lines[3][25];
    char info_lines[5][28];
    uint8_t info_count;
    int8_t selected;
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t card_type;
    uint8_t scan_anim_phase;
    MagicGenType gen;
    bool popup_active;
    char popup_text[26];
} UidFlowModel;

const char* mtools_uid_gen_name(MagicGenType gen);
void mtools_uid_flow_draw(Canvas* canvas, void* context);
