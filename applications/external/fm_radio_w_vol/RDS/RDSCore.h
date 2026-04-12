#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RDS_BLOCK_BITS         26U
#define RDS_DATA_BITS          16U
#define RDS_CHECK_BITS         10U
#define RDS_GROUP_BLOCKS       4U
#define RDS_PS_LEN             8U
#define RDS_RT_LEN             64U
#define RDS_EVENT_QUEUE_SIZE   8U
#define RDS_PILOT_LEVEL_MIN_Q8 5120U
#define RDS_BAND_LEVEL_MIN_Q8  5120U
#define RDS_PRESYNC_REQUIRED   3U

#define RDS_OFFSET_A  0x0FCU
#define RDS_OFFSET_B  0x198U
#define RDS_OFFSET_C  0x168U
#define RDS_OFFSET_CP 0x350U
#define RDS_OFFSET_D  0x1B4U

typedef enum {
    RdsSyncStateSearch = 0,
    RdsSyncStatePreSync,
    RdsSyncStateSync,
    RdsSyncStateLost,
} RdsSyncState;

typedef enum {
    RdsBlockTypeUnknown = 0,
    RdsBlockTypeA,
    RdsBlockTypeB,
    RdsBlockTypeC,
    RdsBlockTypeCp,
    RdsBlockTypeD,
} RdsBlockType;

typedef enum {
    RdsBlockStatusInvalid = 0,
    RdsBlockStatusValid,
    RdsBlockStatusCorrected,
    RdsBlockStatusUncorrectable,
} RdsBlockStatus;

typedef enum {
    RdsEventTypeNone = 0,
    RdsEventTypeDecoderStarted,
    RdsEventTypePilotDetected,
    RdsEventTypeRdsCarrierDetected,
    RdsEventTypeSyncAcquired,
    RdsEventTypeSyncLost,
    RdsEventTypePiUpdated,
    RdsEventTypePsUpdated,
    RdsEventTypeRtUpdated,
    RdsEventTypePtyUpdated,
    RdsEventTypeBlockStatsUpdated,
} RdsEventType;

typedef struct {
    uint32_t raw26;
    uint16_t data16;
    uint16_t syndrome10;
    uint16_t expected_offset10;
    uint16_t error_syndrome10;
    uint32_t correction_mask26;
    RdsBlockType type;
    RdsBlockStatus status;
    uint8_t corrected_bits;
} RdsBlock;

typedef struct {
    RdsBlock blocks[RDS_GROUP_BLOCKS];
    uint8_t count;
    uint16_t pi;
    uint8_t group_type;
    bool version_b;
    bool complete;
} RdsGroup;

typedef struct {
    char ps[RDS_PS_LEN + 1U];
    char ps_candidate[RDS_PS_LEN + 1U];
    char rt[RDS_RT_LEN + 1U];
    char rt_candidate[RDS_RT_LEN + 1U];
    uint16_t pi;
    uint16_t rt_segment_mask;
    uint8_t pty;
    uint8_t rt_length;
    bool rt_ab_flag;
    bool tp;
    bool ta;
    bool ps_ready;
    bool rt_ready;
} RdsProgramInfo;

typedef struct {
    uint16_t syndrome10;
    uint32_t mask26;
    uint8_t burst_length;
    uint8_t first_bit_index;
} RdsCorrectionEntry;

typedef struct {
    RdsEventType type;
    uint32_t tick_ms;
    uint16_t pi;
    char ps[RDS_PS_LEN + 1U];
    char rt[RDS_RT_LEN + 1U];
    uint8_t pty;
    RdsSyncState sync_state;
    uint32_t total_blocks;
    uint32_t valid_blocks;
    uint32_t corrected_blocks;
    uint32_t uncorrectable_blocks;
} RdsEvent;

typedef struct {
    RdsSyncState sync_state;
    RdsBlockType expected_next_block;
    uint8_t block_index_in_group;
    uint8_t flywheel_errors;
    uint8_t flywheel_limit;
    uint8_t presync_consecutive;

    uint32_t bit_window;
    uint64_t bit_history;
    uint8_t bit_count;
    int8_t bit_phase;

    uint32_t pilot_level_q8;
    uint32_t rds_band_level_q8;
    uint32_t lock_quality_q16;

    uint32_t total_blocks;
    uint32_t valid_blocks;
    uint32_t corrected_blocks;
    uint32_t uncorrectable_blocks;
    uint32_t sync_losses;
    uint32_t bit_slip_repairs;

    RdsGroup current_group;
    RdsProgramInfo program;
    uint8_t ps_segment_mask;
    bool slip_retry_pending;
    bool pilot_detected;
    bool rds_carrier_detected;
    RdsEvent event_queue[RDS_EVENT_QUEUE_SIZE];
    uint8_t event_read_idx;
    uint8_t event_write_idx;
    uint8_t event_count;
} RDSCore;

void rds_core_reset(RDSCore* core);
void rds_core_restart_sync(RDSCore* core);
void rds_core_push_bit(RDSCore* core, uint8_t bit);
bool rds_core_consume_demod_bit(RDSCore* core, uint8_t bit, RdsBlock* decoded_block);
bool rds_core_try_decode_block(RDSCore* core, RdsBlock* block, uint32_t raw26);
void rds_core_handle_block(RDSCore* core, const RdsBlock* block);
void rds_core_handle_group(RDSCore* core, const RdsGroup* group);
bool rds_core_pop_event(RDSCore* core, RdsEvent* event);

RdsBlockType rds_core_next_block_type(RdsBlockType current);
uint16_t rds_core_expected_offset(RdsBlockType type);
uint16_t rds_core_calc_syndrome(uint32_t raw26);
size_t rds_core_build_burst_correction_table(
    RdsCorrectionEntry* entries,
    size_t max_entries,
    uint8_t max_burst_len);
