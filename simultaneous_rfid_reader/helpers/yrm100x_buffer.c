#include "yrm100x_buffer.h"
#include <stdlib.h>
#include <string.h>

/**
 * File that handles the buffer for the YRM100 reader 
 * @author frux-c
*/

Buffer* uhf_buffer_alloc(size_t initial_capacity) {
    Buffer* buf = (Buffer*)malloc(sizeof(Buffer));
    buf->data = (uint8_t*)malloc(sizeof(uint8_t) * initial_capacity);
    if(!buf->data) {
        free(buf);
        return NULL;
    }
    buf->size = 0;
    buf->capacity = initial_capacity;
    buf->head = 0;
    buf->tail = 0;
    return buf;
}

bool uhf_buffer_append_single(Buffer* buf, uint8_t data) {
    if(buf->closed) return false;
    buf->data[buf->tail] = data;
    buf->tail = (buf->tail + 1) % buf->capacity;
    if(buf->size < buf->capacity) {
        buf->size++;
    } else {
        buf->head = (buf->head + 1) % buf->capacity;
    }
    return true;
}

bool uhf_buffer_append(Buffer* buf, uint8_t* data, size_t data_size) {
    if(buf->closed) return false;
    for(size_t i = 0; i < data_size; i++) {
        buf->data[buf->tail] = data[i];
        buf->tail = (buf->tail + 1) % buf->capacity;
        if(buf->size < buf->capacity) {
            buf->size++;
        } else {
            buf->head = (buf->head + 1) % buf->capacity;
        }
    }
    return true;
}

uint8_t* uhf_buffer_get_data(Buffer* buf) {
    return &buf->data[buf->head];
}

size_t uhf_buffer_get_size(Buffer* buf) {
    return buf->size;
}

bool uhf_is_buffer_closed(Buffer* buf) {
    return buf->closed;
}

void uhf_buffer_close(Buffer* buf) {
    buf->closed = true;
}

void uhf_buffer_reset(Buffer* buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->size = 0;
    buf->closed = false;
}

void uhf_buffer_free(Buffer* buf) {
    free(buf->data);
    free(buf);
}