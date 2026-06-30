
#pragma once

#include <lib/toolbox/hex.h>
#include <stddef.h>
#include <stdint.h>

#include "seader.h"

#define SEADER_APDU_RUNNER_FILE_NAME APP_DATA_PATH("script.apdu")

void seader_apdu_runner_init(Seader* seader);
bool seader_apdu_runner_response(Seader* seader, uint8_t* r_apdu, size_t r_len);
