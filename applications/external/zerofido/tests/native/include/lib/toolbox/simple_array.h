#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SimpleArray SimpleArray;
typedef void SimpleArrayData;
typedef void SimpleArrayElement;

typedef void (*SimpleArrayInit)(SimpleArrayElement *elem);
typedef void (*SimpleArrayReset)(SimpleArrayElement *elem);
typedef void (*SimpleArrayCopy)(SimpleArrayElement *elem, const SimpleArrayElement *other);

typedef struct {
    SimpleArrayInit init;
    SimpleArrayReset reset;
    SimpleArrayCopy copy;
    size_t type_size;
} SimpleArrayConfig;

extern const SimpleArrayConfig simple_array_config_uint8_t;

SimpleArray *simple_array_alloc(const SimpleArrayConfig *config);
void simple_array_free(SimpleArray *instance);
void simple_array_init(SimpleArray *instance, uint32_t count);
void simple_array_reset(SimpleArray *instance);
void simple_array_copy(SimpleArray *instance, const SimpleArray *other);
bool simple_array_is_equal(const SimpleArray *instance, const SimpleArray *other);
uint32_t simple_array_get_count(const SimpleArray *instance);
SimpleArrayElement *simple_array_get(SimpleArray *instance, uint32_t index);
const SimpleArrayElement *simple_array_cget(const SimpleArray *instance, uint32_t index);
SimpleArrayData *simple_array_get_data(SimpleArray *instance);
const SimpleArrayData *simple_array_cget_data(const SimpleArray *instance);
