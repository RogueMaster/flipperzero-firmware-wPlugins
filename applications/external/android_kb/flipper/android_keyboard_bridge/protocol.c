#include "protocol.h"

#include <string.h>

void akb_protocol_init(AkbProtocolParser* parser, FuriMessageQueue* hid_queue) {
    parser->length = 0;
    parser->hid_queue = hid_queue;
}

static bool akb_enqueue(AkbProtocolParser* parser, const AkbHidCmd* cmd) {
    if(!parser->hid_queue) {
        return false;
    }
    return furi_message_queue_put(parser->hid_queue, cmd, 0) == FuriStatusOk;
}

static bool akb_dispatch_frame(AkbProtocolParser* parser, const uint8_t* frame) {
    if(frame[0] != AKB_MAGIC_0 || frame[1] != AKB_MAGIC_1) {
        return false;
    }
    if(frame[2] != AKB_FRAME_PAYLOAD_LEN) {
        return false;
    }

    const uint8_t event = frame[3];
    AkbHidCmd cmd;
    memset(&cmd, 0, sizeof(cmd));

    switch(event) {
    case AKB_EVENT_KEY_DOWN:
        cmd.type = AkbHidCmdKeyDown;
        cmd.mods = frame[4];
        cmd.keycode = frame[5];
        break;
    case AKB_EVENT_KEY_UP:
        cmd.type = AkbHidCmdKeyUp;
        cmd.mods = frame[4];
        cmd.keycode = frame[5];
        break;
    case AKB_EVENT_MOUSE_MOVE:
        cmd.type = AkbHidCmdMouseMove;
        cmd.dx = (int8_t)frame[4];
        cmd.dy = (int8_t)frame[5];
        break;
    case AKB_EVENT_MOUSE_DOWN:
        cmd.type = AkbHidCmdMouseButtonDown;
        cmd.mouse_button = frame[4];
        break;
    case AKB_EVENT_MOUSE_UP:
        cmd.type = AkbHidCmdMouseButtonUp;
        cmd.mouse_button = frame[4];
        break;
    case AKB_EVENT_MOUSE_SCROLL:
        cmd.type = AkbHidCmdMouseScroll;
        cmd.scroll = (int8_t)frame[4];
        break;
    default:
        return false;
    }

    return akb_enqueue(parser, &cmd);
}

size_t akb_protocol_feed(AkbProtocolParser* parser, const uint8_t* data, size_t size) {
    size_t frames = 0;

    for(size_t i = 0; i < size; i++) {
        if(parser->length >= sizeof(parser->buffer)) {
            parser->length = 0;
        }
        parser->buffer[parser->length++] = data[i];

        while(parser->length >= 3) {
            size_t sync = 0;
            while(sync + 1 < parser->length &&
                  (parser->buffer[sync] != AKB_MAGIC_0 ||
                   parser->buffer[sync + 1] != AKB_MAGIC_1)) {
                sync++;
            }

            if(sync > 0) {
                memmove(parser->buffer, parser->buffer + sync, parser->length - sync);
                parser->length -= sync;
            }

            if(parser->length < 3) {
                break;
            }

            const uint8_t payload_len = parser->buffer[2];
            if(payload_len != AKB_FRAME_PAYLOAD_LEN) {
                memmove(parser->buffer, parser->buffer + 1, parser->length - 1);
                parser->length -= 1;
                continue;
            }

            const size_t total = 3U + payload_len;
            if(parser->length < total) {
                break;
            }

            if(akb_dispatch_frame(parser, parser->buffer)) {
                frames++;
            }

            memmove(parser->buffer, parser->buffer + total, parser->length - total);
            parser->length -= total;
        }
    }

    return frames;
}
