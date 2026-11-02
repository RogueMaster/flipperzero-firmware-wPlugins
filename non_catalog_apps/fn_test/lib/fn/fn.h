/**
 * @file fn.h
 * @brief Umbrella-заголовок приватной библиотеки `lib/fn`.
 *
 * Слои: core/ (байты, кадр, ошибки), proto/ (парсеры), transport/ (UART),
 * fn_worker + fn_ops (операции). UI использует только этот заголовок.
 */
#pragma once

#include "core/fn_error.h"
#include "core/fn_frame.h"
#include "core/fn_types.h"
#include "core/fn_wire.h"

#include "proto/fn_commands.h"
#include "proto/fn_ident.h"
#include "proto/fn_info.h"
#include "proto/fn_life.h"
#include "proto/fn_status.h"

#include "fn_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Все объявления находятся в перечисленных заголовках. */

#ifdef __cplusplus
}
#endif
