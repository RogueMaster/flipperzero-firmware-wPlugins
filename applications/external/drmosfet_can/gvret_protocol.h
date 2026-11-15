#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void (*reply)(void* context, const uint8_t* data, size_t size);
    void (*frame)(void* context, uint32_t id, bool extended, uint8_t dlc, const uint8_t data[8]);
    bool (*bitrate)(void* context, uint32_t bitrate);
    uint32_t (*timestamp)(void* context);
} GvretIo;

typedef struct {
    GvretIo io;
    void* context;
    uint32_t bitrate;
    uint32_t id;
    uint32_t setup;
    uint32_t invalid_frames;
    uint8_t state;
    uint8_t step;
    uint8_t bus;
    uint8_t dlc;
    uint8_t data[8];
} GvretProtocol;

void gvret_init(GvretProtocol* p, const GvretIo* io, void* context, uint32_t bitrate);
void gvret_feed(GvretProtocol* p, uint8_t byte);
size_t gvret_encode(
    uint8_t output[20],
    uint32_t timestamp,
    uint32_t id,
    bool extended,
    uint8_t dlc,
    const uint8_t data[8]);
