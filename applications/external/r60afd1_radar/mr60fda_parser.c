#include "mr60fda_parser.h"
#include <string.h>

void mr60_parser_reset(Mr60Parser* p) {
    memset(p, 0, sizeof(*p));
    p->state = Mr60StHdr0;
}

// On any mid-frame mismatch we either reset, or — if the bad byte happens to
// be 0x53 — treat it as the start of a new frame so we don't lose it.
static void mr60_resync(Mr60Parser* p, uint8_t byte) {
    mr60_parser_reset(p);
    if(byte == MR60_HDR0) {
        p->state = Mr60StHdr1;
        p->sum_acc = MR60_HDR0;
    }
}

bool mr60_parser_feed(Mr60Parser* p, uint8_t byte) {
    switch(p->state) {
    case Mr60StHdr0:
        if(byte == MR60_HDR0) {
            p->sum_acc = byte;
            p->state = Mr60StHdr1;
        }
        return false;

    case Mr60StHdr1:
        if(byte == MR60_HDR1) {
            p->sum_acc += byte;
            p->state = Mr60StCtrl;
        } else {
            mr60_resync(p, byte);
        }
        return false;

    case Mr60StCtrl:
        p->ctrl = byte;
        p->sum_acc += byte;
        p->state = Mr60StCmd;
        return false;

    case Mr60StCmd:
        p->cmd = byte;
        p->sum_acc += byte;
        p->state = Mr60StLenH;
        return false;

    case Mr60StLenH:
        p->length = ((uint16_t)byte) << 8;
        p->sum_acc += byte;
        p->state = Mr60StLenL;
        return false;

    case Mr60StLenL:
        p->length |= byte;
        p->sum_acc += byte;
        p->data_idx = 0;
        if(p->length > MR60_MAX_PAYLOAD) {
            mr60_resync(p, byte);
        } else if(p->length == 0) {
            p->state = Mr60StChk;
        } else {
            p->state = Mr60StData;
        }
        return false;

    case Mr60StData:
        p->data[p->data_idx++] = byte;
        p->sum_acc += byte;
        if(p->data_idx >= p->length) {
            p->state = Mr60StChk;
        }
        return false;

    case Mr60StChk:
        if(byte == p->sum_acc) {
            p->state = Mr60StTail0;
        } else {
            mr60_resync(p, byte);
        }
        return false;

    case Mr60StTail0:
        if(byte == MR60_TAIL0) {
            p->state = Mr60StTail1;
        } else {
            mr60_resync(p, byte);
        }
        return false;

    case Mr60StTail1: {
        bool ok = (byte == MR60_TAIL1);
        // Frame is complete; ctrl/cmd/length/data are valid for the caller.
        // Reset state machine only — keep the payload until next CTRL byte arrives.
        p->state = Mr60StHdr0;
        p->sum_acc = 0;
        return ok;
    }
    }
    return false;
}

void mr60_apply(const Mr60Parser* p, Mr60RadarState* s) {
    s->frames_total++;

    if(p->ctrl == 0x80) {
        if((p->cmd == 0x01 || p->cmd == 0x81) && p->length >= 1) {
            s->presence = (p->data[0] != 0);
        } else if(p->cmd == 0x02 && p->length >= 1) {
            s->motion = p->data[0];
        } else if(p->cmd == 0x03 && p->length >= 1) {
            s->body_sign = p->data[0];
        } else if(p->cmd == 0x04 && p->length >= 2) {
            s->distance_cm = ((uint16_t)p->data[0] << 8) | p->data[1];
        } else if(p->cmd == 0x05 && p->length >= 4) {
            // XY position: each value is unsigned 16-bit, bit15=sign (0=pos,1=neg)
            uint16_t rx = ((uint16_t)p->data[0] << 8) | p->data[1];
            uint16_t ry = ((uint16_t)p->data[2] << 8) | p->data[3];
            s->pos_x = (rx & 0x8000) ? -(int16_t)(rx & 0x7FFF) : (int16_t)(rx & 0x7FFF);
            s->pos_y = (ry & 0x8000) ? -(int16_t)(ry & 0x7FFF) : (int16_t)(ry & 0x7FFF);
        } else if(p->cmd == 0x0E && p->length >= 6) {
            // Height zone data: total(2B), zone0-50cm, zone50-100cm, zone100-150cm, zone150-200cm
            s->height_total = ((uint16_t)p->data[0] << 8) | p->data[1];
            s->height[0] = p->data[2]; // 0-50 cm
            s->height[1] = p->data[3]; // 50-100 cm
            s->height[2] = p->data[4]; // 100-150 cm
            s->height[3] = p->data[5]; // 150-200 cm
        }
    } else if(p->ctrl == 0x81) {
        if(p->cmd == 0x02 && p->length >= 1) {
            s->breath_rate = p->data[0];
        }
    } else if(p->ctrl == 0x83) {
        if(p->cmd == 0x01 && p->length >= 1) {
            s->fall_detected = (p->data[0] != 0);
        } else if(p->cmd == 0x04 && p->length >= 4) {
            s->residence_sec =
                ((uint32_t)p->data[0] << 24) | ((uint32_t)p->data[1] << 16) |
                ((uint32_t)p->data[2] << 8)  |  (uint32_t)p->data[3];
        }
    } else if(p->ctrl == 0x85) {
        if(p->cmd == 0x02 && p->length >= 1) {
            s->heart_rate = p->data[0];
        }
    }
}
