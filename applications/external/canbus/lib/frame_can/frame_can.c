#include "frame_can.h"

FrameCAN* frame_can_alloc(void) {
    FrameCAN* frame = malloc(sizeof(FrameCAN));

    frame->timestamp = (uint16_t*)calloc(1, sizeof(uint16_t));
    frame->extended = (bool*)calloc(1, sizeof(bool));
    frame->dir = furi_string_alloc();
    frame->can_id = furi_string_alloc();
    frame->len = (char*)calloc(1, sizeof(char));
    frame->dlc = furi_string_alloc();

    return frame;
}

void frame_can_free(FrameCAN* frame) {
    free(frame->timestamp);
    free(frame->extended);
    furi_string_free(frame->dir);
    furi_string_free(frame->can_id);
    free(frame->len);
    furi_string_free(frame->dlc);

    free(frame);
}
