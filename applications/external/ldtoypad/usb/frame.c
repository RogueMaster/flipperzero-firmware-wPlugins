#include "frame.h"

static void calculate_checksum(uint8_t* buf, int length, int place);

// Function to parse a Frame from a buffer
void parse_frame(Frame* frame, unsigned char* buf, int len) {
    UNUSED(len);
    frame->type = buf[0];
    frame->len = buf[1];
    memcpy(frame->payload, buf + 2, frame->len);
}

// Function to build a Frame into a buffer
int build_frame(Frame* frame, unsigned char* buf) {
    buf[0] = frame->type;
    buf[1] = frame->len;
    memcpy(buf + 2, frame->payload, frame->len);
    calculate_checksum(buf, frame->len + 2, -1);
    return frame->len + 3;
}

// Function to parse a Frame into a Request
void parse_request(Request* request, Frame* f) {
    if(request == NULL || f == NULL) return;

    request->frame = *f;
    uint8_t* p = f->payload;

    request->cmd = p[0];
    request->cid = p[1];
    memcpy(request->payload, p + 2, f->len - 2); // Copy payload, excluding cmd and cid
}

// Function to parse a Response from a Frame
void parse_response(Response* response, Frame* frame) {
    response->frame = *frame;
    response->cid = frame->payload[0];
    response->payload_len = frame->len - 1;
    memcpy(response->payload, frame->payload + 1, response->payload_len);
}

// Function to build a Response into a Frame
int build_response(Response* response, unsigned char* buf) {
    response->frame.type = FRAME_TYPE_RESPONSE;
    response->frame.len = response->payload_len + 1;
    response->frame.payload[0] = response->cid;
    memcpy(response->frame.payload + 1, response->payload, response->payload_len);
    return build_frame(&response->frame, buf);
}

static void calculate_checksum(uint8_t* buf, int length, int place) {
    uint8_t checksum = 0;

    if(place == -1) {
        place = length;
    }

    // Calculate checksum (up to 'length')
    for(int i = 0; i < length; i++) {
        checksum = (checksum + buf[i]) % 256;
    }

    // Assign checksum to the last position
    buf[place] = checksum;
}
