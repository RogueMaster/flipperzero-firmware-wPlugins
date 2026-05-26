#pragma once

#include <stdint.h>

#define HELP_TOPIC_COUNT 10

const char* help_topic_name(uint8_t topic);
uint8_t help_line_count(uint8_t topic);
const char* help_line(uint8_t topic, uint8_t line);
