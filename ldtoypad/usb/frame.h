#pragma once

#include <furi.h>

#include "usb_toypad.h"
#include "descriptors.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_TYPE_RESPONSE 0x55
#define FRAME_TYPE_REQUEST  0x56

typedef struct {
    unsigned char type;
    unsigned char len;
    unsigned char payload[HID_EP_SZ - 2];
} Frame;

// Define a Response structure
typedef struct {
    Frame frame;
    unsigned char cid;
    unsigned char payload[HID_EP_SZ];
    int payload_len;
} Response;

// Define a Request structure
typedef struct {
    Frame frame;
    unsigned char cmd;
    unsigned char cid;
    unsigned char payload[HID_EP_SZ - 2];
    int payload_len;
} Request;

void parse_frame(Frame* frame, unsigned char* buf, int len);

int build_frame(Frame* frame, unsigned char* buf);

void parse_request(Request* request, Frame* f);

void parse_response(Response* response, Frame* frame);

int build_response(Response* response, unsigned char* buf);

#ifdef __cplusplus
}
#endif