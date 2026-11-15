// GVRET byte layout and device handshake extracted from our CANBUS application.
// Transport is injected so the compiled parser can be tested without USB/CAN.
#include "gvret_protocol.h"
#include <string.h>

enum {
    Idle,
    Command,
    Frame,
    Setup
};
static void le32(uint8_t* out, uint32_t n) {
    for(uint8_t i = 0; i < 4; ++i)
        out[i] = n >> (8 * i);
}
void gvret_init(GvretProtocol* p, const GvretIo* io, void* context, uint32_t bitrate) {
    memset(p, 0, sizeof(*p));
    p->io = *io;
    p->context = context;
    p->bitrate = bitrate;
}
static void reply(GvretProtocol* p, uint8_t command) {
    uint8_t out[12] = {0xf1, command};
    size_t size = 0;
    switch(command) {
    case 1:
        le32(out + 2, p->io.timestamp(p->context));
        size = 6;
        break;
    case 6:
        out[2] = 1;
        le32(out + 3, p->bitrate);
        size = 12;
        break;
    case 7:
        out[2] = 352 & 255;
        out[3] = 352 >> 8;
        out[4] = 1;
        size = 8;
        break;
    case 9:
        size = 2;
        break;
    case 12:
        out[2] = 1;
        size = 3;
        break;
    case 13:
        size = 3;
        break;
    default:
        break;
    }
    if(size) p->io.reply(p->context, out, size);
}
void gvret_feed(GvretProtocol* p, uint8_t b) {
    switch(p->state) {
    case Idle:
        if(b == 0xf1) p->state = Command;
        break;
    case Command:
        p->step = 0;
        if(b == 0) {
            p->state = Frame;
            p->id = 0;
            p->dlc = 0;
        } else if(b == 5) {
            p->state = Setup;
            p->setup = 0;
        } else {
            reply(p, b);
            p->state = Idle;
        }
        break;
    case Frame:
        if(p->step < 4)
            p->id |= (uint32_t)b << (8 * p->step);
        else if(p->step == 4)
            p->bus = b;
        else if(p->step == 5)
            p->dlc = b & 15;
        else if(p->step - 6 < 8)
            p->data[p->step - 6] = b;
        ++p->step;
        if(p->step >= 6 && p->step >= 6 + p->dlc) {
            bool extended = (p->id & 0x80000000U) != 0;
            uint32_t id = p->id & 0x1fffffffU;
            if(p->bus == 0 && p->dlc <= 8 && !(p->id & 0x60000000U) && (extended || id <= 0x7ff))
                p->io.frame(p->context, id, extended, p->dlc, p->data);
            else
                ++p->invalid_frames;
            p->state = Idle;
        }
        break;
    case Setup:
        if(p->step < 4) p->setup |= (uint32_t)b << (8 * p->step);
        if(++p->step == 8) {
            uint32_t speed = p->setup & 0x000fffff;
            if(speed && speed <= 1000000 && p->io.bitrate(p->context, speed)) p->bitrate = speed;
            p->state = Idle;
        }
        break;
    default:
        p->state = Idle;
        break;
    }
}
size_t gvret_encode(
    uint8_t out[20],
    uint32_t timestamp,
    uint32_t id,
    bool extended,
    uint8_t dlc,
    const uint8_t data[8]) {
    if(dlc > 8 || id > (extended ? 0x1fffffffU : 0x7ffU)) return 0;
    out[0] = 0xf1;
    out[1] = 0;
    le32(out + 2, timestamp);
    le32(out + 6, id | (extended ? 0x80000000U : 0));
    out[10] = dlc;
    if(dlc) memcpy(out + 11, data, dlc);
    out[11 + dlc] = 0;
    return 12 + dlc;
}
