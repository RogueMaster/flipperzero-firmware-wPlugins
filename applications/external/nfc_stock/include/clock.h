/**
 * Time port (dependency inversion): domain code depends on this declaration;
 * persistence/fs_compat implements it for host and device.
 */
#pragma once

#include <stdint.h>

uint32_t app_now_timestamp(void);
