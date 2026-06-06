#include "RDSCore.h"

#include <limits.h>
#include <string.h>

#define RDS_POLY_10                      0x5B9U
#define RDS_DEFAULT_FLYWHEEL_LIMIT       20U
#define RDS_BURST_CORRECTION_MAX_ENTRIES 120U
#define RDS_FULL_PS_SEGMENT_MASK         0x0FU
#define RDS_FULL_RT_SEGMENT_MASK_2A      0xFFFFU
#define RDS_FULL_RT_SEGMENT_MASK_2B      0x00FFU
#define RDS_BLOCK_STATS_EMIT_INTERVAL    32U

static RdsCorrectionEntry rds_correction_table[RDS_BURST_CORRECTION_MAX_ENTRIES];
static size_t rds_correction_table_count = 0;
static bool rds_correction_table_ready = false;
static int16_t rds_correction_best_by_syndrome[1024];
static bool rds_syndrome_tables_ready = false;
static uint16_t rds_syndrome_low8_table[256];
static uint16_t rds_syndrome_mid8_table[256];
static uint16_t rds_syndrome_high8_table[256];
static uint16_t rds_syndrome_top2_table[4];

static void rds_core_ensure_correction_table(void);
static void rds_core_build_syndrome_tables(void);
static uint8_t rds_core_popcount_u32(uint32_t value);
static bool rds_core_is_block_status_ok(RdsBlockStatus status);
static bool rds_core_block_type_matches_expected(RdsBlockType actual, RdsBlockType expected);
static uint8_t rds_core_group_index_from_block_type(RdsBlockType type);
static void rds_core_reset_group(RdsGroup* group);
static void rds_core_handle_synced_block(RDSCore* core, const RdsBlock* block);
static void rds_core_parse_group_0(RDSCore* core, const RdsGroup* group);
static void rds_core_parse_group_2(RDSCore* core, const RdsGroup* group);
static const RdsCorrectionEntry* rds_core_find_correction(uint16_t error_syndrome10);
static bool rds_core_try_decode_for_type(RdsBlock* block, uint32_t raw26, RdsBlockType type);
static bool rds_core_try_decode_for_type_with_syndrome(
    RdsBlock* block,
    uint32_t raw26,
    uint16_t syndrome10,
    RdsBlockType type);
static bool rds_core_try_decode_search(RdsBlock* block, uint32_t raw26);
static bool rds_core_try_decode_expected(RDSCore* core, RdsBlock* block, uint32_t raw26);
static bool rds_core_try_bit_slip_repair(RDSCore* core, RdsBlock* block, uint32_t raw26);
static uint32_t rds_core_extract_window(const RDSCore* core, uint8_t back_offset);
static void rds_core_maybe_emit_block_stats(RDSCore* core, bool force);

static void rds_core_emit_event(RDSCore* core, RdsEventType type) {
    RdsEvent event = {0};

    if(!core) return;

    event.type = type;
    event.tick_ms = core->event_tick_ms ? core->event_tick_ms : (core->events_emitted + 1U);
    event.pi = core->program.pi;
    memcpy(event.ps, core->program.ps, sizeof(event.ps));
    memcpy(event.rt, core->program.rt, sizeof(event.rt));
    event.pty = core->program.pty;
    event.sync_state = core->sync_state;
    event.total_blocks = core->total_blocks;
    event.valid_blocks = core->valid_blocks;
    event.corrected_blocks = core->corrected_blocks;
    event.uncorrectable_blocks = core->uncorrectable_blocks;

    // Keep newest events: if queue is full, drop the oldest one.
    if(core->event_count >= RDS_EVENT_QUEUE_SIZE) {
        core->event_read_idx = (uint8_t)((core->event_read_idx + 1U) % RDS_EVENT_QUEUE_SIZE);
        core->event_count--;
        core->events_dropped++;
    }

    core->event_queue[core->event_write_idx] = event;
    core->event_write_idx = (uint8_t)((core->event_write_idx + 1U) % RDS_EVENT_QUEUE_SIZE);
    core->event_count++;
    core->events_emitted++;
}

static void rds_core_ensure_correction_table(void) {
    if(rds_correction_table_ready) return;

    rds_core_build_syndrome_tables();

    rds_correction_table_count = rds_core_build_burst_correction_table(
        rds_correction_table, RDS_BURST_CORRECTION_MAX_ENTRIES, 3U);

    for(size_t i = 0; i < 1024U; i++) {
        rds_correction_best_by_syndrome[i] = -1;
    }

    for(size_t i = 0; i < rds_correction_table_count; i++) {
        uint16_t syndrome = rds_correction_table[i].syndrome10;
        int16_t current_idx = rds_correction_best_by_syndrome[syndrome];

        if(current_idx < 0) {
            rds_correction_best_by_syndrome[syndrome] = (int16_t)i;
            continue;
        }

        const RdsCorrectionEntry* current = &rds_correction_table[(size_t)current_idx];
        const RdsCorrectionEntry* candidate = &rds_correction_table[i];

        if(candidate->burst_length < current->burst_length ||
           (candidate->burst_length == current->burst_length &&
            candidate->first_bit_index < current->first_bit_index)) {
            rds_correction_best_by_syndrome[syndrome] = (int16_t)i;
        }
    }

    rds_correction_table_ready = true;
}

static void rds_core_maybe_emit_block_stats(RDSCore* core, bool force) {
    if(!core) return;

    if(force || ((core->total_blocks % RDS_BLOCK_STATS_EMIT_INTERVAL) == 0U)) {
        rds_core_emit_event(core, RdsEventTypeBlockStatsUpdated);
    }
}

static void rds_core_build_syndrome_tables(void) {
    if(rds_syndrome_tables_ready) return;

    for(uint32_t value = 0U; value < 256U; value++) {
        rds_syndrome_low8_table[value] = rds_core_calc_syndrome(value);
        rds_syndrome_mid8_table[value] = rds_core_calc_syndrome(value << 8U);
        rds_syndrome_high8_table[value] = rds_core_calc_syndrome(value << 16U);
    }

    for(uint32_t value = 0U; value < 4U; value++) {
        rds_syndrome_top2_table[value] = rds_core_calc_syndrome(value << 24U);
    }

    rds_syndrome_tables_ready = true;
}

static uint8_t rds_core_popcount_u32(uint32_t value) {
    return (uint8_t)__builtin_popcount(value);
}

static uint32_t rds_core_extract_window(const RDSCore* core, uint8_t back_offset) {
    if(!core || back_offset > 1U) return 0U;

    return (uint32_t)((core->bit_history >> back_offset) & 0x03FFFFFFULL);
}

static bool rds_core_is_block_status_ok(RdsBlockStatus status) {
    return status == RdsBlockStatusValid || status == RdsBlockStatusCorrected;
}

static bool rds_core_block_type_matches_expected(RdsBlockType actual, RdsBlockType expected) {
    if(expected == RdsBlockTypeC) {
        return actual == RdsBlockTypeC || actual == RdsBlockTypeCp;
    }

    return actual == expected;
}

static uint8_t rds_core_group_index_from_block_type(RdsBlockType type) {
    switch(type) {
    case RdsBlockTypeA:
        return 0U;
    case RdsBlockTypeB:
        return 1U;
    case RdsBlockTypeC:
    case RdsBlockTypeCp:
        return 2U;
    case RdsBlockTypeD:
        return 3U;
    default:
        return 0xFFU;
    }
}

static void rds_core_reset_group(RdsGroup* group) {
    if(!group) return;
    memset(group, 0, sizeof(*group));
}

static const RdsCorrectionEntry* rds_core_find_correction(uint16_t error_syndrome10) {
    rds_core_ensure_correction_table();

    int16_t idx = rds_correction_best_by_syndrome[error_syndrome10 & 0x03FFU];
    if(idx < 0) {
        return NULL;
    }

    return &rds_correction_table[(size_t)idx];
}

static bool rds_core_try_decode_for_type(RdsBlock* block, uint32_t raw26, RdsBlockType type) {
    return rds_core_try_decode_for_type_with_syndrome(
        block, raw26, rds_core_calc_syndrome(raw26), type);
}

static bool rds_core_try_decode_for_type_with_syndrome(
    RdsBlock* block,
    uint32_t raw26,
    uint16_t syndrome10,
    RdsBlockType type) {
    const uint16_t expected_offset10 = rds_core_expected_offset(type);

    block->raw26 = raw26 & 0x03FFFFFFU;
    block->syndrome10 = syndrome10;
    block->expected_offset10 = expected_offset10;
    block->error_syndrome10 = 0U;
    block->correction_mask26 = 0U;
    block->type = type;
    block->status = RdsBlockStatusInvalid;
    block->corrected_bits = 0U;

    if(syndrome10 == expected_offset10) {
        block->status = RdsBlockStatusValid;
        block->data16 = (uint16_t)((block->raw26 >> RDS_CHECK_BITS) & 0xFFFFU);
        return true;
    }

    block->error_syndrome10 = syndrome10 ^ expected_offset10;

    const RdsCorrectionEntry* correction = rds_core_find_correction(block->error_syndrome10);
    if(!correction) {
        block->status = RdsBlockStatusUncorrectable;
        block->data16 = (uint16_t)((block->raw26 >> RDS_CHECK_BITS) & 0xFFFFU);
        return false;
    }

    block->correction_mask26 = correction->mask26;
    block->corrected_bits = rds_core_popcount_u32(correction->mask26);
    block->raw26 ^= correction->mask26;
    block->syndrome10 = rds_core_calc_syndrome(block->raw26);
    block->data16 = (uint16_t)((block->raw26 >> RDS_CHECK_BITS) & 0xFFFFU);

    if(block->syndrome10 == expected_offset10) {
        block->status = RdsBlockStatusCorrected;
        return true;
    }

    block->status = RdsBlockStatusUncorrectable;
    return false;
}

static bool rds_core_try_decode_search(RdsBlock* block, uint32_t raw26) {
    /* SEARCH mode with correction enabled (legacy behavior):
     * try all block types and pick the best unique candidate.
     */
    const RdsBlockType types[] = {
        RdsBlockTypeA,
        RdsBlockTypeB,
        RdsBlockTypeC,
        RdsBlockTypeCp,
        RdsBlockTypeD,
    };
    const uint16_t syndrome10 = rds_core_calc_syndrome(raw26);
    RdsBlock best_block = {0};
    int32_t best_score = INT32_MIN;
    uint8_t best_count = 0U;

    for(size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        RdsBlock candidate = {0};
        int32_t score;

        if(!rds_core_try_decode_for_type_with_syndrome(&candidate, raw26, syndrome10, types[i])) {
            continue;
        }
        if(!rds_core_is_block_status_ok(candidate.status)) {
            continue;
        }

        score = (candidate.status == RdsBlockStatusValid) ? 1000 : 0;
        score += (100 - (int32_t)candidate.corrected_bits);

        if(score > best_score) {
            best_score = score;
            best_block = candidate;
            best_count = 1U;
        } else if(score == best_score) {
            best_count++;
        }
    }

    if(best_count == 1U) {
        *block = best_block;
        return true;
    }

    memset(block, 0, sizeof(*block));
    block->raw26 = raw26 & 0x03FFFFFFU;
    block->data16 = (uint16_t)((block->raw26 >> RDS_CHECK_BITS) & 0xFFFFU);
    block->syndrome10 = syndrome10;
    block->status = RdsBlockStatusUncorrectable;
    block->type = RdsBlockTypeUnknown;
    return false;
}

static bool rds_core_try_decode_expected(RDSCore* core, RdsBlock* block, uint32_t raw26) {
    if(core->expected_next_block == RdsBlockTypeC) {
        RdsBlock c_block = {0};
        RdsBlock cp_block = {0};
        bool c_ok = rds_core_try_decode_for_type(&c_block, raw26, RdsBlockTypeC);
        bool cp_ok = rds_core_try_decode_for_type(&cp_block, raw26, RdsBlockTypeCp);
        bool c_valid = c_ok && (c_block.status == RdsBlockStatusValid);
        bool cp_valid = cp_ok && (cp_block.status == RdsBlockStatusValid);

        if(c_ok && !cp_ok) {
            *block = c_block;
            return true;
        }
        if(cp_ok && !c_ok) {
            *block = cp_block;
            return true;
        }
        if(c_valid && !cp_valid) {
            *block = c_block;
            return true;
        }
        if(cp_valid && !c_valid) {
            *block = cp_block;
            return true;
        }
        if(c_ok && cp_ok) {
            memset(block, 0, sizeof(*block));
            block->raw26 = raw26 & 0x03FFFFFFU;
            block->data16 = (uint16_t)((block->raw26 >> RDS_CHECK_BITS) & 0xFFFFU);
            block->syndrome10 = rds_core_calc_syndrome(block->raw26);
            block->status = RdsBlockStatusInvalid;
            block->type = RdsBlockTypeUnknown;
            return false;
        }

        *block = c_block.status == RdsBlockStatusUncorrectable ? c_block : cp_block;
        if(block->status == RdsBlockStatusInvalid) block->status = RdsBlockStatusUncorrectable;
        return false;
    }

    return rds_core_try_decode_for_type(block, raw26, core->expected_next_block);
}

static void rds_core_parse_group_0(RDSCore* core, const RdsGroup* group) {
    const uint16_t block_b = group->blocks[1].data16;
    const uint16_t block_d = group->blocks[3].data16;
    const uint16_t pi = group->blocks[0].data16;
    const uint8_t segment = (uint8_t)(block_b & 0x03U);
    const uint8_t segment_bit = (uint8_t)(1U << segment);
    const size_t segment_index = (size_t)segment * 2U;
    const uint8_t pty = (uint8_t)((block_b >> 5U) & 0x1FU);
    const bool tp = ((block_b >> 10U) & 0x01U) != 0U;
    const bool ta = ((block_b >> 4U) & 0x01U) != 0U;
    const char segment_char0 = (char)((block_d >> 8U) & 0xFFU);
    const char segment_char1 = (char)(block_d & 0xFFU);
    const bool was_ready = core->program.ps_ready;

    if(core->program.pi != pi) {
        core->program.pi = pi;
        core->last_pi = pi;
        core->pi_updates++;
        rds_core_emit_event(core, RdsEventTypePiUpdated);
    }

    if(core->program.pty != pty) {
        core->program.pty = pty;
        rds_core_emit_event(core, RdsEventTypePtyUpdated);
    }

    core->program.tp = tp;
    core->program.ta = ta;

    if(core->program.ps_candidate[segment_index] == segment_char0 &&
       core->program.ps_candidate[segment_index + 1U] == segment_char1) {
        bool changed = core->program.ps[segment_index] != segment_char0 ||
                       core->program.ps[segment_index + 1U] != segment_char1;

        core->ps_segment_mask |= segment_bit;
        core->program.ps[segment_index] = segment_char0;
        core->program.ps[segment_index + 1U] = segment_char1;
        core->program.ps[RDS_PS_LEN] = '\0';
        core->program.ps_ready = (core->ps_segment_mask == RDS_FULL_PS_SEGMENT_MASK);

        if(changed || (!was_ready && core->program.ps_ready)) {
            core->ps_updates++;
            rds_core_emit_event(core, RdsEventTypePsUpdated);
        }
    } else {
        core->program.ps_candidate[segment_index] = segment_char0;
        core->program.ps_candidate[segment_index + 1U] = segment_char1;
        core->ps_segment_mask &= (uint8_t)~segment_bit;
        core->program.ps_ready = false;
    }

    core->program.ps_candidate[RDS_PS_LEN] = '\0';
}

static void rds_core_parse_group_2(RDSCore* core, const RdsGroup* group) {
    const uint16_t block_b = group->blocks[1].data16;
    const uint16_t pi = group->blocks[0].data16;
    const bool version_b = group->version_b;
    const bool rt_ab_flag = ((block_b >> 4U) & 0x01U) != 0U;
    const uint8_t segment_raw = (uint8_t)(block_b & 0x0FU);
    const uint8_t pty = (uint8_t)((block_b >> 5U) & 0x1FU);
    const bool tp = ((block_b >> 10U) & 0x01U) != 0U;
    uint8_t segment;
    uint8_t base_index;
    uint16_t expected_segment_mask;

    if(core->program.pi != pi) {
        core->program.pi = pi;
        core->last_pi = pi;
        core->pi_updates++;
        rds_core_emit_event(core, RdsEventTypePiUpdated);
    }

    if(core->program.pty != pty) {
        core->program.pty = pty;
        rds_core_emit_event(core, RdsEventTypePtyUpdated);
    }

    core->program.tp = tp;

    if(core->program.rt_ab_flag != rt_ab_flag || core->program.rt_length == 0U) {
        memset(core->program.rt, 0, sizeof(core->program.rt));
        memset(core->program.rt_candidate, 0, sizeof(core->program.rt_candidate));
        core->program.rt_segment_mask = 0U;
        core->program.rt_ready = false;
        core->program.rt_ab_flag = rt_ab_flag;
        core->program.rt_length = version_b ? 16U : 64U;
    }

    if(version_b) {
        segment = (uint8_t)(segment_raw & 0x07U);
        expected_segment_mask = RDS_FULL_RT_SEGMENT_MASK_2B;
        base_index = (uint8_t)(segment * 2U);
        if(base_index + 1U >= RDS_RT_LEN) return;
        core->program.rt_candidate[base_index] = (char)((group->blocks[3].data16 >> 8U) & 0xFFU);
        core->program.rt_candidate[base_index + 1U] = (char)(group->blocks[3].data16 & 0xFFU);
    } else {
        segment = segment_raw;
        expected_segment_mask = RDS_FULL_RT_SEGMENT_MASK_2A;
        base_index = (uint8_t)(segment * 4U);
        if(base_index + 3U >= RDS_RT_LEN) return;
        core->program.rt_candidate[base_index] = (char)((group->blocks[2].data16 >> 8U) & 0xFFU);
        core->program.rt_candidate[base_index + 1U] = (char)(group->blocks[2].data16 & 0xFFU);
        core->program.rt_candidate[base_index + 2U] =
            (char)((group->blocks[3].data16 >> 8U) & 0xFFU);
        core->program.rt_candidate[base_index + 3U] = (char)(group->blocks[3].data16 & 0xFFU);
    }

    core->program.rt_candidate[core->program.rt_length] = '\0';
    core->program.rt_segment_mask |= (uint16_t)(1U << segment);

    if(core->program.rt_segment_mask == expected_segment_mask) {
        if(memcmp(core->program.rt, core->program.rt_candidate, core->program.rt_length + 1U) !=
           0) {
            memcpy(core->program.rt, core->program.rt_candidate, core->program.rt_length + 1U);
            core->program.rt_ready = true;
            rds_core_emit_event(core, RdsEventTypeRtUpdated);
        }
        core->program.rt_segment_mask = 0U;
    }
}

static bool rds_core_try_bit_slip_repair(RDSCore* core, RdsBlock* block, uint32_t raw26) {
    RdsBlock repaired = {0};
    uint32_t shifted_raw26;

    if(!core || !block) return false;
    if(core->bit_count < (RDS_BLOCK_BITS + 1U)) return false;

    shifted_raw26 = rds_core_extract_window(core, 1U);
    if(shifted_raw26 == 0U || shifted_raw26 == (raw26 & 0x03FFFFFFU)) return false;

    if(rds_core_try_decode_expected(core, &repaired, shifted_raw26)) {
        *block = repaired;
        core->bit_slip_repairs++;
        core->bit_phase = 0;
        return true;
    }

    return false;
}

static void rds_core_handle_synced_block(RDSCore* core, const RdsBlock* block) {
    const uint8_t group_index = rds_core_group_index_from_block_type(block->type);

    if(group_index == 0U) {
        rds_core_reset_group(&core->current_group);
        core->current_group.blocks[0] = *block;
        core->current_group.count = 1U;
        return;
    }

    if(core->current_group.count != group_index) {
        if(group_index != 0xFFU) {
            rds_core_reset_group(&core->current_group);
        }
        return;
    }

    core->current_group.blocks[group_index] = *block;
    core->current_group.count = (uint8_t)(group_index + 1U);

    if(core->current_group.count == RDS_GROUP_BLOCKS) {
        core->current_group.pi = core->current_group.blocks[0].data16;
        core->current_group.group_type =
            (uint8_t)((core->current_group.blocks[1].data16 >> 12U) & 0x0FU);
        core->current_group.version_b = ((core->current_group.blocks[1].data16 >> 11U) & 0x01U) !=
                                        0U;
        core->current_group.complete = true;
        core->groups_complete++;
        rds_core_handle_group(core, &core->current_group);
        rds_core_reset_group(&core->current_group);
    }
}

void rds_core_reset(RDSCore* core) {
    if(!core) return;

    memset(core, 0, sizeof(*core));
    core->sync_state = RdsSyncStateSearch;
    core->expected_next_block = RdsBlockTypeUnknown;
    core->flywheel_limit = RDS_DEFAULT_FLYWHEEL_LIMIT;
    core->slip_retry_pending = false;
    core->event_read_idx = 0U;
    core->event_write_idx = 0U;
    core->event_count = 0U;
    rds_core_emit_event(core, RdsEventTypeDecoderStarted);
}

void rds_core_restart_sync(RDSCore* core) {
    if(!core) return;

    core->sync_state = RdsSyncStateSearch;
    core->expected_next_block = RdsBlockTypeUnknown;
    core->block_index_in_group = 0;
    core->flywheel_errors = 0;
    core->presync_consecutive = 0;
    core->bit_window = 0;
    core->bit_history = 0;
    core->bit_count = 0;
    core->bit_phase = 0;
    core->ps_segment_mask = 0U;
    core->slip_retry_pending = false;
    core->pilot_detected = false;
    core->rds_carrier_detected = false;
    rds_core_reset_group(&core->current_group);
}

void rds_core_set_tick_ms(RDSCore* core, uint32_t tick_ms) {
    if(!core) return;

    core->event_tick_ms = tick_ms;
}

void rds_core_push_bit(RDSCore* core, uint8_t bit) {
    if(!core) return;

    core->bit_window = ((core->bit_window << 1U) | (uint32_t)(bit & 0x01U)) & 0x03FFFFFFU;
    core->bit_history = ((core->bit_history << 1U) | (uint64_t)(bit & 0x01U)) & 0x07FFFFFFULL;
    if(core->bit_count < 27U) {
        core->bit_count++;
    }
}

bool rds_core_consume_demod_bit(RDSCore* core, uint8_t bit, RdsBlock* decoded_block) {
    RdsBlock local_block = {0};
    bool should_attempt = false;
    bool pilot_ok;
    bool rds_ok;

    if(!core) return false;

    pilot_ok = (core->pilot_level_q8 >= RDS_PILOT_LEVEL_MIN_Q8);
    rds_ok = core->rds_band_level_q8 >= RDS_BAND_LEVEL_MIN_Q8;

#if defined(HOST_BUILD) && !defined(RDS_HOST_ENABLE_QUALITY_GATE)
    /* Disable quality gate for offline testing — pilot/rds ratio depends on
       generator parameters and doesn't reflect real antenna signal quality. */
    pilot_ok = true;
    rds_ok = true;
#endif

    if(pilot_ok && !core->pilot_detected) {
        core->pilot_detected = true;
        rds_core_emit_event(core, RdsEventTypePilotDetected);
    } else if(!pilot_ok) {
        core->pilot_detected = false;
    }

    if(rds_ok && !core->rds_carrier_detected) {
        core->rds_carrier_detected = true;
        rds_core_emit_event(core, RdsEventTypeRdsCarrierDetected);
    } else if(!rds_ok) {
        core->rds_carrier_detected = false;
    }

    if(!(pilot_ok && rds_ok)) {
        if(!pilot_ok) core->quality_gate_pilot_fail++;
        if(!rds_ok) core->quality_gate_rds_fail++;
        /* Gate closed: discard this bit entirely.  Reset bit accumulator
         * so that when the gate re-opens we start with 26 fresh good bits
         * before any decode attempt.  push_bit is intentionally skipped. */
        core->bit_window = 0;
        core->bit_history = 0;
        core->bit_count = 0;
        if(core->sync_state != RdsSyncStateSearch) {
            rds_core_restart_sync(core);
        }
        return false;
    }

    rds_core_push_bit(core, bit);

    /* Track bits spent in sync/presync for diagnostics */
    if(core->sync_state == RdsSyncStateSync || core->sync_state == RdsSyncStatePreSync) {
        core->sync_bits_total++;
    }

    if(core->bit_count < RDS_BLOCK_BITS) {
        return false;
    }

    // Delayed retry path gives the opposite slip direction (+1 bit) on next sample.
    if(core->sync_state == RdsSyncStateSync && core->slip_retry_pending) {
        core->slip_retry_pending = false;
        if(rds_core_try_decode_expected(core, &local_block, core->bit_window)) {
            rds_core_handle_block(core, &local_block);
            if(decoded_block) {
                *decoded_block = local_block;
            }
            return rds_core_is_block_status_ok(local_block.status);
        }
    }

    switch(core->sync_state) {
    case RdsSyncStateSearch:
    case RdsSyncStateLost:
        should_attempt = true;
        break;
    case RdsSyncStatePreSync:
    case RdsSyncStateSync:
        if(core->bit_phase < (int8_t)(RDS_BLOCK_BITS - 1U)) {
            core->bit_phase++;
            return false;
        }
        should_attempt = true;
        break;
    default:
        break;
    }

    if(!should_attempt) {
        return false;
    }

    (void)rds_core_try_decode_block(core, &local_block, core->bit_window);
    rds_core_handle_block(core, &local_block);

    if(decoded_block) {
        *decoded_block = local_block;
    }

    return rds_core_is_block_status_ok(local_block.status);
}

bool rds_core_try_decode_block(RDSCore* core, RdsBlock* block, uint32_t raw26) {
    if(!core || !block) return false;

    if(core->sync_state == RdsSyncStateSearch || core->sync_state == RdsSyncStateLost ||
       core->expected_next_block == RdsBlockTypeUnknown) {
        return rds_core_try_decode_search(block, raw26);
    }

    return rds_core_try_decode_expected(core, block, raw26);
}

void rds_core_handle_block(RDSCore* core, const RdsBlock* block) {
    bool force_block_stats = false;

    if(!core || !block) return;

    core->total_blocks++;

    /* Mode-separated counters for diagnostics */
    bool in_sync =
        (core->sync_state == RdsSyncStateSync || core->sync_state == RdsSyncStatePreSync);

    switch(block->status) {
    case RdsBlockStatusValid:
        core->valid_blocks++;
        core->flywheel_errors = 0U;
        if(in_sync)
            core->sync_valid++;
        else
            core->search_valid++;
        break;
    case RdsBlockStatusCorrected:
        core->corrected_blocks++;
        core->flywheel_errors = 0U;
        if(in_sync)
            core->sync_corrected++;
        else
            core->search_corrected++;
        break;
    case RdsBlockStatusUncorrectable:
        core->uncorrectable_blocks++;
        if(in_sync)
            core->sync_uncorrectable++;
        else
            core->search_uncorrectable++;
        if(core->sync_state == RdsSyncStateSync) {
            core->flywheel_errors++;
            if(core->flywheel_errors > core->flywheel_limit) {
                core->sync_state = RdsSyncStateLost;
                core->sync_losses++;
                rds_core_emit_event(core, RdsEventTypeSyncLost);
                force_block_stats = true;
            }
        }
        break;
    default:
        break;
    }

    switch(core->sync_state) {
    case RdsSyncStateSearch:
        if(rds_core_is_block_status_ok(block->status)) {
            core->sync_state = RdsSyncStatePreSync;
            core->expected_next_block = rds_core_next_block_type(block->type);
            core->block_index_in_group = rds_core_group_index_from_block_type(block->type);
            core->bit_phase = 0;
            core->presync_consecutive = 1U;
            core->presync_attempts++;
        }
        break;
    case RdsSyncStatePreSync:
        if(rds_core_is_block_status_ok(block->status) &&
           rds_core_block_type_matches_expected(block->type, core->expected_next_block)) {
            core->presync_consecutive++;
            if(core->presync_consecutive > core->presync_max_consecutive) {
                core->presync_max_consecutive = core->presync_consecutive;
            }
            core->expected_next_block = rds_core_next_block_type(block->type);
            core->block_index_in_group = rds_core_group_index_from_block_type(block->type);
            core->bit_phase = 0;

            if(core->presync_consecutive >= RDS_PRESYNC_REQUIRED) {
                core->sync_state = RdsSyncStateSync;
                core->flywheel_errors = 0U;
                rds_core_emit_event(core, RdsEventTypeSyncAcquired);
                force_block_stats = true;

                if(block->type == RdsBlockTypeA) {
                    rds_core_handle_synced_block(core, block);
                } else {
                    rds_core_reset_group(&core->current_group);
                }
            }
        } else {
            core->sync_state = RdsSyncStateSearch;
            core->expected_next_block = RdsBlockTypeUnknown;
            core->block_index_in_group = 0U;
            core->flywheel_errors = 0U;
            core->presync_consecutive = 0U;
            core->bit_phase = 0;
            core->slip_retry_pending = false;
        }
        break;
    case RdsSyncStateSync:
        if(rds_core_is_block_status_ok(block->status) &&
           rds_core_block_type_matches_expected(block->type, core->expected_next_block)) {
            core->expected_next_block = rds_core_next_block_type(block->type);
            core->block_index_in_group = rds_core_group_index_from_block_type(block->type);
            core->bit_phase = 0;
            core->slip_retry_pending = false;
            rds_core_handle_synced_block(core, block);
        } else {
            // Try bit-slip repair before giving up on this block.
            RdsBlock repaired_block = {0};
            bool repaired = false;
            bool structural_miss = rds_core_is_block_status_ok(block->status);

            if(rds_core_try_bit_slip_repair(core, &repaired_block, block->raw26) &&
               rds_core_is_block_status_ok(repaired_block.status) &&
               rds_core_block_type_matches_expected(
                   repaired_block.type, core->expected_next_block)) {
                core->expected_next_block = rds_core_next_block_type(repaired_block.type);
                core->block_index_in_group =
                    rds_core_group_index_from_block_type(repaired_block.type);
                core->slip_retry_pending = false;
                rds_core_handle_synced_block(core, &repaired_block);
                repaired = true;
            }

            if(!repaired) {
                rds_core_reset_group(&core->current_group);

                if(structural_miss) {
                    core->sync_state = RdsSyncStatePreSync;
                    core->expected_next_block = rds_core_next_block_type(block->type);
                    core->block_index_in_group = rds_core_group_index_from_block_type(block->type);
                    core->bit_phase = 0;
                    core->presync_consecutive = 1U;
                    core->presync_attempts++;
                    core->flywheel_errors = 0U;
                } else {
                    core->expected_next_block =
                        rds_core_next_block_type(core->expected_next_block);
                    core->block_index_in_group =
                        rds_core_group_index_from_block_type(core->expected_next_block);
                }

                core->slip_retry_pending = false;
            }
        }

        if(core->sync_state == RdsSyncStateLost) {
            rds_core_restart_sync(core);
        }
        break;
    case RdsSyncStateLost:
        rds_core_restart_sync(core);
        break;
    default:
        break;
    }

    rds_core_maybe_emit_block_stats(core, force_block_stats);
}

void rds_core_handle_group(RDSCore* core, const RdsGroup* group) {
    if(!core || !group) return;

    if(!group->complete) return;

    if(group->group_type == 0U) {
        core->groups_type0++;
        rds_core_parse_group_0(core, group);
        return;
    }

    if(group->group_type == 2U) {
        core->groups_type2++;
        rds_core_parse_group_2(core, group);
        return;
    }

    core->groups_other++;
    if(group->pi != 0U && group->pi != core->program.pi) {
        core->program.pi = group->pi;
        rds_core_emit_event(core, RdsEventTypePiUpdated);
    }
}

bool rds_core_pop_event(RDSCore* core, RdsEvent* event) {
    if(!core || !event || core->event_count == 0U) return false;

    *event = core->event_queue[core->event_read_idx];
    core->event_read_idx = (uint8_t)((core->event_read_idx + 1U) % RDS_EVENT_QUEUE_SIZE);
    core->event_count--;
    return true;
}

RdsBlockType rds_core_next_block_type(RdsBlockType current) {
    switch(current) {
    case RdsBlockTypeA:
        return RdsBlockTypeB;
    case RdsBlockTypeB:
        return RdsBlockTypeC;
    case RdsBlockTypeC:
    case RdsBlockTypeCp:
        return RdsBlockTypeD;
    case RdsBlockTypeD:
        return RdsBlockTypeA;
    default:
        return RdsBlockTypeUnknown;
    }
}

uint16_t rds_core_expected_offset(RdsBlockType type) {
    switch(type) {
    case RdsBlockTypeA:
        return RDS_OFFSET_A;
    case RdsBlockTypeB:
        return RDS_OFFSET_B;
    case RdsBlockTypeC:
        return RDS_OFFSET_C;
    case RdsBlockTypeCp:
        return RDS_OFFSET_CP;
    case RdsBlockTypeD:
        return RDS_OFFSET_D;
    default:
        return 0U;
    }
}

uint16_t rds_core_calc_syndrome(uint32_t raw26) {
    raw26 &= 0x03FFFFFFU;

    if(rds_syndrome_tables_ready) {
        uint16_t syndrome = 0U;
        syndrome ^= rds_syndrome_low8_table[raw26 & 0xFFU];
        syndrome ^= rds_syndrome_mid8_table[(raw26 >> 8U) & 0xFFU];
        syndrome ^= rds_syndrome_high8_table[(raw26 >> 16U) & 0xFFU];
        syndrome ^= rds_syndrome_top2_table[(raw26 >> 24U) & 0x03U];
        return (uint16_t)(syndrome & 0x03FFU);
    }

    uint32_t reg = raw26;
    for(int bit = (int)RDS_BLOCK_BITS - 1; bit >= (int)RDS_CHECK_BITS; bit--) {
        if((reg & (1UL << bit)) != 0U) {
            reg ^= (uint32_t)RDS_POLY_10 << (bit - RDS_CHECK_BITS);
        }
    }

    return (uint16_t)(reg & 0x03FFU);
}

size_t rds_core_build_burst_correction_table(
    RdsCorrectionEntry* entries,
    size_t max_entries,
    uint8_t max_burst_len) {
    size_t count = 0;

    if(!entries || max_burst_len == 0U) return 0;
    if(max_burst_len > 5U) max_burst_len = 5U;

    for(uint8_t burst_len = 1; burst_len <= max_burst_len; burst_len++) {
        for(uint8_t first_bit = 0; first_bit + burst_len <= RDS_BLOCK_BITS; first_bit++) {
            uint32_t mask = 0U;
            for(uint8_t offset = 0; offset < burst_len; offset++) {
                uint8_t bit_index = (uint8_t)(RDS_BLOCK_BITS - 1U - (first_bit + offset));
                mask |= 1UL << bit_index;
            }

            if(count >= max_entries) return count;

            entries[count].syndrome10 = rds_core_calc_syndrome(mask);
            entries[count].mask26 = mask;
            entries[count].burst_length = burst_len;
            entries[count].first_bit_index = first_bit;
            count++;
        }
    }

    return count;
}
