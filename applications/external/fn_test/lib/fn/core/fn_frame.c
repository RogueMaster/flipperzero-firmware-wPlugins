#include "fn_frame.h"

#include "fn_wire.h"

#include <string.h>

#define FN_FRAME_START 0x04u
#define FN_FRAME_HEADER_LEN 4u /* START + LEN(2) + CMD */

static void fn_frame_set_status(FnFrameStatus* status, FnFrameStatus value) {
    if(status) *status = value;
}

size_t fn_frame_build(
    uint8_t command,
    const uint8_t* data,
    size_t data_len,
    uint8_t* out,
    size_t out_cap,
    FnFrameStatus* status) {
    if(!out) {
        fn_frame_set_status(status, FnFrameNull);
        return 0;
    }
    if(data_len > FN_FRAME_MAX_PAYLOAD || (data_len > 0 && !data)) {
        fn_frame_set_status(status, FnFrameTooLong);
        return 0;
    }

    const size_t total = data_len + FN_FRAME_OVERHEAD;
    if(total > FN_FRAME_MAX_LEN || total > out_cap) {
        fn_frame_set_status(status, FnFrameTooLong);
        return 0;
    }

    out[0] = FN_FRAME_START;
    fn_wire_u16le_put(&out[1], (uint16_t)(data_len + 1));
    out[3] = command;
    if(data_len > 0) {
        memcpy(&out[4], data, data_len);
    }

    const uint16_t crc = fn_wire_crc16_ccitt(&out[1], total - 3);
    fn_wire_u16le_put(&out[total - 2], crc);

    fn_frame_set_status(status, FnFrameOk);
    return total;
}

FnFrameStatus fn_frame_parse(const uint8_t* buf, size_t len, FnFrame* out) {
    if(!buf || !out) return FnFrameNull;
    if(len < FN_FRAME_OVERHEAD) return FnFrameTooShort;
    if(len > FN_FRAME_MAX_LEN) return FnFrameTooLong;
    if(buf[0] != FN_FRAME_START) return FnFrameWrongStart;

    const uint16_t wire_len = fn_wire_u16le_get(&buf[1]);
    if((size_t)wire_len != len - 5) return FnFrameLengthMismatch;

    const uint16_t crc_calc = fn_wire_crc16_ccitt(&buf[1], len - 3);
    const uint16_t crc_wire = fn_wire_u16le_get(&buf[len - 2]);
    if(crc_calc != crc_wire) return FnFrameWrongCrc;

    const uint8_t command = buf[3];
    out->command = command;
    out->code = (uint8_t)(command & 0x7F);
    out->has_warning = (command & 0x80) != 0;
    out->data = &buf[FN_FRAME_HEADER_LEN];
    out->data_len = (size_t)wire_len - 1;
    return FnFrameOk;
}
