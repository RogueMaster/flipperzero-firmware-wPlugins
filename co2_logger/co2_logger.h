#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct co2_logger co2_logger;

co2_logger* co2_logger_alloc();

void co2_logger_free(co2_logger* instance);

void co2_logger_open(co2_logger* instance);

void co2_logger_close(co2_logger* instance);

bool co2_logger_read_gas_concentration(co2_logger* instance, uint32_t* value);
