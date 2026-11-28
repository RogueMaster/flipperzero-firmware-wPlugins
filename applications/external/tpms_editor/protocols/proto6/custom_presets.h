#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * ProtoView CC1101 presets have a single definition in custom_presets.c.
 * Keeping only extern declarations here prevents every translation unit
 * from creating its own private copy and avoids ARF -Werror unused-variable
 * failures.
 */
extern uint8_t protoview_subghz_tpms1_fsk_async_regs[][2];
extern const size_t protoview_subghz_tpms1_fsk_async_regs_size;

extern const uint8_t protoview_subghz_tpms2_ook_async_regs[][2];
extern const size_t protoview_subghz_tpms2_ook_async_regs_size;

extern uint8_t protoview_subghz_tpms3_gfsk_async_regs[][2];
extern const size_t protoview_subghz_tpms3_gfsk_async_regs_size;

extern uint8_t protoview_subghz_40k_fsk_async_regs[][2];
extern const size_t protoview_subghz_40k_fsk_async_regs_size;

extern const uint8_t protoview_subghz_40k_ook_async_regs[][2];
extern const size_t protoview_subghz_40k_ook_async_regs_size;
